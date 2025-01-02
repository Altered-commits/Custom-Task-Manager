#include "ctm_process_screen.h"

//Don't really want these macros, they are messing up the std::max and std::min functions
#undef max
#undef min

//Equivalent to OnInit function
CTMProcessScreen::CTMProcessScreen()
    : processInfoBuffer(processInfoBufferSize)
{
    //Initialize NT DLL functions
    if(!CTMConstructorInitNTDLL())
        return;

    //Start a new thread for event tracing
    if(!CTMConstructorInitEventTracingThread())
        return;

    //While we are here, register a resource guard for cleaning up process handle map
    resourceGuard.RegisterCleanupFunction(handleCleanupFunctionName, [this](){
        for(auto &&[_, processHandle] : processIdToHandleMap)
            CloseHandle(processHandle);
    
        processIdToHandleMap.clear();
    });

    //Initialize the array beforehand so we get some content to display
    UpdateProcessInfo();
    SetInitialized(true);
}

//Equivalent to OnClean function
CTMProcessScreen::~CTMProcessScreen() 
{
    CTMDestructorCleanEventTracingThread();
    CTMDestructorCleanMappedHandles();
    SetInitialized(false);
}

//--------------------CONSTRUCTOR INIT AND DESTRUCTOR CLEANUP FUNCTIONS--------------------
bool CTMProcessScreen::CTMConstructorInitNTDLL()
{
    hNtdll = GetModuleHandleW(L"ntdll.dll");
    if(!hNtdll)
    {
        CTM_LOG_ERROR("Failed to get module handle for ntdll.dll");
        return false;
    }

    NtQueryInformationProcess = reinterpret_cast<NtQueryInformationProcess_t>(
        GetProcAddress(hNtdll, "NtQueryInformationProcess")
    );
    NtQuerySystemInformation  = reinterpret_cast<NtQuerySystemInformation_t>(
        GetProcAddress(hNtdll, "NtQuerySystemInformation")
    );

    if(!NtQueryInformationProcess || !NtQuerySystemInformation)
    {
        CTM_LOG_ERROR("Failed to get proc addresses for ntdll.dll functions");
        hNtdll = nullptr;
        return false;
    }

    return true;
}

bool CTMProcessScreen::CTMConstructorInitEventTracingThread()
{
    if(!processUsageEventTracing.Start())
    {
        CTM_LOG_ERROR("Failed to start event tracing. Look at the above errors for more information.");
        return false;
    }

    //After we start the etw, most of the things can go wrong if god doesn't like you (yes you, the user of this program)
    //Register a cleanup function to prevent this from happening
    resourceGuard.RegisterCleanupFunction(etwCleanupFunctionName, [this](){
        processUsageEventTracing.Stop();
    });

    //Will indicate success or failure by getting the response from thread
    std::atomic<bool> initSuccess{true};

    processUsageEventTracingThread = std::thread([this, &initSuccess](){
        //Now call ProcessEvents which should block this thread until it is stopped (if it did not fail that is)
        if (!processUsageEventTracing.ProcessEvents())
            initSuccess.store(false);
    });

    //Sleep for 10ms which allows the above thread to start and do stuff. This way, we get an idea if the thread failed or not
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    //If ProcessEvents failed, stop the event tracing entirely
    if(!initSuccess.load())
    {
        CTM_LOG_ERROR("Failed to process events for event tracing.");
        processUsageEventTracing.Stop();
        if(processUsageEventTracingThread.joinable())
            processUsageEventTracingThread.join(); //Ensure the thread has finished before returning

        //Unregister the cleanup function as all the cleaning work is already done above
        resourceGuard.UnregisterCleanupFunction(etwCleanupFunctionName);
        return false;
    }

    //If initialization succeeded, return true
    return true;
}

void CTMProcessScreen::CTMDestructorCleanEventTracingThread()
{
    processUsageEventTracing.Stop();
    if(processUsageEventTracingThread.joinable())
        processUsageEventTracingThread.join();
    
    //The process was successfully RAII destructed, no need for the cleanup function anymore, say bye bye to it
    resourceGuard.UnregisterCleanupFunction(etwCleanupFunctionName);
    
    CTM_LOG_INFO("Event Tracing Thread destroyed successfully.");
}

void CTMProcessScreen::CTMDestructorCleanMappedHandles()
{
    //Just call CloseHandle on every single entry of processIdToHandleMap
    for(auto &&[_, processHandle] : processIdToHandleMap)
        CloseHandle(processHandle);
    
    //Reset the entire map now
    processIdToHandleMap.clear();
    
    //Well everything went well so... SAY BYE BYE TO CLEANUP FUNCTION
    resourceGuard.UnregisterCleanupFunction(handleCleanupFunctionName);
}

//--------------------MAIN RENDER AND UPDATE FUNCTIONS--------------------
void CTMProcessScreen::OnRender()
{
    if (ImGui::BeginTable("ProcessesTable", 6, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV |
                                               ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollX))
    {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("PID", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("CPU (%)", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Memory (MB)", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Network (MB/s)", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("File RW (MB/s)", ImGuiTableColumnFlags_WidthFixed);

        ImGui::TableHeadersRow();

        //Render rest of the processes as is
        for (auto&& groupedProcess : groupedProcessesMap)
        {
            auto& appName      = groupedProcess.first;
            auto& appProcesses = groupedProcess.second;

            //Start a new row
            ImGui::TableNextRow();

            //Remove the default hovered & active background for tree node, we set it ourselves cuz the background color is too thin in height
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, {0, 0, 0, 0});
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, {0, 0, 0, 0});
            
            //First column -> name of the process group (tree structure)
            ImGui::TableSetColumnIndex(0);
            bool expandTree = ImGui::TreeNodeEx(appName.c_str(), ImGuiTreeNodeFlags_SpanAllColumns);

            ImGui::PopStyleColor(2);
            
            //If the TreeNode is hovered over, set the entire rows background color
            if(ImGui::IsItemHovered())
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, headerBgColorU32);

            //Check for right click on this tree node and if the user did right click, save that specific group and open the popup
            if(ImGui::IsItemClicked(ImGuiMouseButton_Right))
            {
                processVariant  = groupedProcess.first;
                SetPopupBit(static_cast<std::uint8_t>(PopupBitsetIndex::ShouldOpenPopup), true);
                SetPopupBit(static_cast<std::uint8_t>(PopupBitsetIndex::IsProcessGroup), true);
                
                //Assume the group can be terminated initially
                bool canTerminate = true;

                //How to check if a process can be terminated? Simply check if the pid doesn't exist in exclusion set.
                //If it exists, then we can't terminate it
                for (auto&& i : appProcesses)
                {
                    if (processExcludedHandleSet.find(i.processId) != processExcludedHandleSet.end())
                    {
                        canTerminate = false;
                        break;
                    }
                }
                //Set termination status based on the check
                SetPopupBit(static_cast<std::uint8_t>(PopupBitsetIndex::CanTerminate), canTerminate);
            }

            //Second column -> Process ID (if there is only one instance, display the process ID, else display it with instances of process)
            ImGui::TableSetColumnIndex(1);
            if(appProcesses.size() == 1)
                ImGui::Text("%d", appProcesses[0].processId);
            
            //Third, fourth and fith column -> Display total usage initially
            double totalCPUUsage = 0.0, totalMemoryUsage = 0.0, totalNetworkUsage = 0.0, totalFileUsage = 0.0;
            for(auto&& process : appProcesses)
            {
                totalCPUUsage      += process.cpuUsage;
                totalMemoryUsage   += process.memoryUsage;
                totalNetworkUsage  += process.networkUsage;
                totalFileUsage     += process.fileUsage;
            }

            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.2lf", totalCPUUsage);

            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%.2lf", totalMemoryUsage);

            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%.2lf", totalNetworkUsage);

            ImGui::TableSetColumnIndex(5);
            ImGui::Text("%.2lf", totalFileUsage);

            //If we expand the tree, display rest of the details
            if(expandTree)
            {
                //As we group processes by their name, for child processes, no need to store the name seperately
                RenderProcessVector(appProcesses, appName);
                //Close the tree node afterwards
                ImGui::TreePop();
            }
        }

        ImGui::EndTable();
    }

    //Render any popup which 'popped up' during the loop
    RenderProcessOptionsPopup();
}

void CTMProcessScreen::OnUpdate()
{
    UpdateProcessInfo();
}

//--------------------HELPER FUNCTIONS--------------------
void CTMProcessScreen::RenderProcessVector(ProcessInfoVector& appProcesses, const std::string& parentName)
{
    //Set by selectable
    bool isHovered = false;

    for(auto&& process : appProcesses)
    {
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::Indent();
        
        //Create an item that spans the full width of the row
        ImGui::PushID(process.processId);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, headerBgColorVec4);
        
        isHovered = ImGui::Selectable(parentName.c_str(), false, ImGuiSelectableFlags_SpanAllColumns);
        
        ImGui::PopStyleColor();
        ImGui::PopID();

        //Also if its right clicked, then set the variant to contain process id and open popup menu
        if(ImGui::IsItemClicked(ImGuiMouseButton_Right))
        {
            processVariant  = process.processId;
            SetPopupBit(static_cast<std::uint8_t>(PopupBitsetIndex::ShouldOpenPopup), true);
            SetPopupBit(static_cast<std::uint8_t>(PopupBitsetIndex::IsProcessGroup), false);
            //Here also we check if the process can be terminated or not using the exclusion set. '==' means it can be terminated
            SetPopupBit(static_cast<std::uint8_t>(PopupBitsetIndex::CanTerminate),
                            processExcludedHandleSet.find(process.processId) == processExcludedHandleSet.end());
        }

        ImGui::Unindent();

        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%d", process.processId);

        ImGui::TableSetColumnIndex(2);
        ImGui::Text("%.2lf", process.cpuUsage);

        ImGui::TableSetColumnIndex(3);
        ImGui::Text("%.2lf", process.memoryUsage);

        ImGui::TableSetColumnIndex(4);
        ImGui::Text("%.2lf", process.networkUsage);

        ImGui::TableSetColumnIndex(5);
        ImGui::Text("%.2lf", process.fileUsage);
    }
}

void CTMProcessScreen::RenderProcessOptionsPopup()
{
    //Open the popup if it isnt already open
    if(GetPopupBit(static_cast<std::uint8_t>(PopupBitsetIndex::ShouldOpenPopup)))
    {
        ImGui::OpenPopup(popupStringId);
        //We only open it once
        FlipPopupBit(static_cast<std::uint8_t>(PopupBitsetIndex::ShouldOpenPopup));
    }
    
    //The inner content will only render when OpenPopup is called
    if(ImGui::BeginPopup(popupStringId))
    {
        bool isProcessGroup = GetPopupBit(static_cast<std::uint8_t>(PopupBitsetIndex::IsProcessGroup));
        bool canTerminate   = GetPopupBit(static_cast<std::uint8_t>(PopupBitsetIndex::CanTerminate));

        //Find out if its process group or a child
        if(isProcessGroup)
            ImGui::Text("Process Group -> %s", std::get<std::string>(processVariant).c_str());
        else
            ImGui::Text("Process Child, PID -> %d", std::get<DWORD>(processVariant));

        ImGui::Separator();
        ImGui::Dummy({-1.0, 5.0});
        
        //Disable MenuItem if we can't terminate the process
        if(!canTerminate)
            ImGui::BeginDisabled();

        //User asked for termination of process
        if(ImGui::MenuItem(isProcessGroup ? "Terminate Group" : "Terminate Process"))
        {
            if(isProcessGroup)
                TerminateGroupProcess();
            else
            {
                //Get the process id from processVariant
                DWORD processId = std::get<DWORD>(processVariant);
                //Doing it like this so its reusable inside 'TerminateGroupProcess'
                TerminateChildProcess(processId);
            }

            ImGui::CloseCurrentPopup();
        }

        if(!canTerminate)
            ImGui::EndDisabled();
        
        //Close the popup
        if(ImGui::MenuItem("Back"))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }
}

//--------------------
void CTMProcessScreen::UpdateProcessInfo()
{
    NTSTATUS status;
    //Resize buffer as long as we get an error, aka the buffer isnt big enough
    do
    {
        status = NtQuerySystemInformation(SystemProcessInformation, processInfoBuffer.data(),
                                                    processInfoBuffer.size(), &processInfoBufferSize);

        if(status == STATUS_INFO_LENGTH_MISMATCH)
            processInfoBuffer.resize(processInfoBufferSize);
    }
    while (status == STATUS_INFO_LENGTH_MISMATCH);

    //Do the do
    if(status == STATUS_SUCCESS)
    {
        PSYSTEM_PROCESS_INFORMATION systemProcessInfo = reinterpret_cast<PSYSTEM_PROCESS_INFORMATION>(processInfoBuffer.data());

        //Process name converted from a wide string to normal string
        CHAR processName[MAX_PATH] = "<System Idle Process>";

        //Before we go ahead and try to update information, lock the event tracing map
        std::lock_guard<std::mutex> lock(globalPsEtwMutex); //globalPsEtwMutex is global

        //Get the current system times
        FILETIME ftSysKernelTime, ftSysUserTime;
        GetSystemTimes(nullptr, &ftSysKernelTime, &ftSysUserTime);

        //Loop through all the processes as long as this stuffs valid
        while(systemProcessInfo)
        {
            auto& imageName = systemProcessInfo->ImageName;
            if (imageName.Length > 0 && imageName.Buffer != nullptr)
            {
                //Ensure we don't exceed the buffer size
                int bytesWritten = WideCharToMultiByte(CP_UTF8, 0, imageName.Buffer, imageName.Length / sizeof(WCHAR),
                                                    processName, sizeof(processName) - 1, NULL, NULL);

                //Null-terminate the string in case WideCharToMultiByte doesn't do it
                if (bytesWritten > 0)
                    processName[bytesWritten] = '\0';
            }
            
            //It may seem weird that UniqueProcessId is an 'HANDLE' even tho its a pid. Just convert it to DWORD and it works fine
            DWORD  processId = static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(systemProcessInfo->UniqueProcessId));
            HANDLE hProcess  = GetProcessHandleFromId(processId);

            //We will use some hacky hacks to get usage data as we can't open the process for its data
            if (hProcess == nullptr)
                UpdateProcessMapWithoutProcessHandle(processId, processName, 
                    reinterpret_cast<PCTM_SYSTEM_PROCESS_INFORMATION>(systemProcessInfo), ftSysKernelTime, ftSysUserTime);
            //We will be closing process handles through destructor and/or while cleaning stale entries
            else
                UpdateProcessMapWithProcessHandle(hProcess, processId, processName, ftSysKernelTime, ftSysUserTime);

            //No more entries, break outta loop
            if(systemProcessInfo->NextEntryOffset == 0)
                break;
            
            //More entries, go forward
            systemProcessInfo = reinterpret_cast<PSYSTEM_PROCESS_INFORMATION>(
                                    reinterpret_cast<BYTE*>(systemProcessInfo) + systemProcessInfo->NextEntryOffset
                                );
        }

        //Update the previous system times (kernel and user)
        ftPrevSysKernelTime = ftSysKernelTime;
        ftPrevSysUserTime   = ftSysUserTime;

        //Time to remove stale entries
        RemoveStaleEntries();
    }
    else
        CTM_LOG_ERROR("Failed to get system process information.");
}

void CTMProcessScreen::UpdateProcessMapWithProcessHandle(HANDLE hProcess, DWORD processId, const std::string& processName,
                                                            FILETIME ftSysKernel, FILETIME ftSysUser)
{
    /*
     * Some processes allow OpenProcess to run on them, which can be used to get valid stuff without using weird undocumented custom stuff
     */
    //Memory Usage
    double memUsage = CalculateMemoryUsage(hProcess);

    //CPU Usage
    double cpuUsage = CalculateCpuUsage(hProcess, processId, ftSysKernel, ftSysUser);

    //Network Usage
    double networkUsage = 0;
    auto   networkIt    = globalProcessNetworkUsageMap.find(processId);
    if(networkIt != globalProcessNetworkUsageMap.end())
    {
        //Set networkIt to 0 as soon as you use networkIt, that will mark the data per second
        networkUsage = (networkIt->second / (1024.0 * 1024.0));
        networkIt->second   = 0;
    }

    //File Usage
    double fileUsage = 0;
    auto   fileIt    = globalProcessFileUsageMap.find(processId);
    if(fileIt != globalProcessFileUsageMap.end())
    {
        //Set fileIt to 0 as soon as you use fileIt, that will mark the data per second
        fileUsage  = (fileIt->second / (1024.0 * 1024.0));
        fileIt->second = 0;
    }

    //Update the grouped processes map
    UpdateProcessMap(processId, processName, memUsage, cpuUsage, networkUsage, fileUsage);
}

void CTMProcessScreen::UpdateProcessMapWithoutProcessHandle(DWORD processId, const std::string& processName,
                                PCTM_SYSTEM_PROCESS_INFORMATION processInformation, FILETIME ftSysKernel, FILETIME ftSysUser)
{
    /*
     * The process cant be opened, we will use some undocumented, non backwards compatibility stuff. THIS IS THE ONLY WAY
     */
    //Memory Usage
    double memUsage = (processInformation->WorkingSetPrivateSize.QuadPart / (1024.0 * 1024.0));

    //Network Usage
    double networkUsage = 0;
    auto   networkIt    = globalProcessNetworkUsageMap.find(processId);
    if(networkIt != globalProcessNetworkUsageMap.end())
    {
        //Set networkIt to 0 as soon as you use networkIt, that will mark the data per second
        networkUsage = (networkIt->second / (1024.0 * 1024.0));
        networkIt->second   = 0;
    }
    
    //File Usage
    double fileUsage = 0;
    auto   fileIt    = globalProcessFileUsageMap.find(processId);
    if(fileIt != globalProcessFileUsageMap.end())
    {
        //Set fileIt to 0 as soon as you use fileIt, that will mark the data per second
        fileUsage  = (fileIt->second / (1024.0 * 1024.0));
        fileIt->second = 0;
    }

    //CPU Usage
    double cpuUsage = CalculateCpuUsageDelta(processId, ftSysKernel, ftSysUser,
                            processInformation->KernelTime, processInformation->UserTime);

    UpdateProcessMap(processId, processName, memUsage, cpuUsage, networkUsage, fileUsage);
}

void CTMProcessScreen::UpdateProcessMap(DWORD processId, const std::string& processName,
                        double memUsage, double cpuUsage, double networkUsage, double fileUsage)
{
    //Get the process if it exists. If it doesn't exist, it will auto create it for us
    auto& processVector = groupedProcessesMap[processName];

    //If the processVector is empty that means its an entirely new process entry
    if(processVector.empty())
        processVector.emplace_back(processId, memUsage, cpuUsage, networkUsage, fileUsage);
    
    //If the processVector is not empty, that means it may or may not exist beforehand
    else
    {
        //Check if the element exists beforehand
        auto it = std::find_if(processVector.begin(), processVector.end(), [&processId](const ProcessInfo& childProc){
            return childProc.processId == processId;
        });

        //If it exists, simply update its values
        if(it != processVector.end())
        {
            it->cpuUsage     = cpuUsage;
            it->memoryUsage  = memUsage;
            it->networkUsage = networkUsage;
            it->fileUsage    = fileUsage;
            it->isStaleEntry = FALSE;
        }
        //The element does not exist, its a new one under this category
        else
            processVector.emplace_back(processId, memUsage, cpuUsage, networkUsage, fileUsage);
    }
}

//--------------------
HANDLE CTMProcessScreen::GetProcessHandleFromId(DWORD processId)
{
    //Early return if process is found in the excluded set
    if (processExcludedHandleSet.find(processId) != processExcludedHandleSet.end())
        return nullptr;

    auto it = processIdToHandleMap.find(processId);
    //The handle exists in the map, return it
    if (it != processIdToHandleMap.end())
        return it->second;
    
    //The handle doesn't exist in the map, try to 'OpenProcess' and get the process handle
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_TERMINATE, FALSE, processId);
    //Successfully opened the process, store the handle in the map and return it
    if (hProcess)
        processIdToHandleMap[processId] = hProcess;
    //Failed to open process, add the processId to the exclusion set
    else
        processExcludedHandleSet.insert(processId);

    return hProcess;
}

void CTMProcessScreen::TerminateChildProcess(DWORD processId)
{
    //Get its HANDLE from 'processIdToHandleMap' (before that, find if the handle exists or not)
    auto it = processIdToHandleMap.find(processId);
    if(it != processIdToHandleMap.end()) //HANDLE exists
    {
        if(!TerminateProcess(it->second, 0))
            CTM_LOG_ERROR("Failed to terminate process with pid: ", processId, ". Error code: ", GetLastError());
        else
            CTM_LOG_SUCCESS("Successfully terminated process with pid: ", processId);
    }
    else //HANDLE doesn't exist
        CTM_LOG_ERROR("Failed to find process with pid: ", processId, " in map. The process may have been terminated beforehand.");
}

void CTMProcessScreen::TerminateGroupProcess()
{
    //First of all, get the key of the group itself
    auto& processGroupKey = std::get<std::string>(processVariant);

    //Check if the group exists or not
    auto it = groupedProcessesMap.find(processGroupKey);
    if(it != groupedProcessesMap.end()) //Exists, loop through all the processes and terminate them
    {
        CTM_LOG_INFO("Terminating process group -> ", processGroupKey);
        for (auto &&i : it->second)
            TerminateChildProcess(i.processId);
    }
    else //Doesn't exist, may have been terminated beforehand
        CTM_LOG_ERROR("Failed to terminate process group -> ", processGroupKey, ". The group may have been terminated beforehand.");
}

void CTMProcessScreen::RemoveStaleEntries()
{
    for(auto it = groupedProcessesMap.begin(); it != groupedProcessesMap.end(); )
    {
        auto& processVector = it->second;

        //Check for stale entries in process vector and remove them
        processVector.erase(
            std::remove_if(processVector.begin(), processVector.end(),
                            //The logic is, if the entry has been marked as 'FALSE' by the 'UpdateProcessInfo' function-
                            //-then the entry will be marked as stale by this code but won't remove it
                            //Else, the entry is removed
                            [this](ProcessInfo& child)
                            {
                                if(!child.isStaleEntry) {
                                    child.isStaleEntry = TRUE;
                                    return false;
                                }
                                //Remove the entry from other maps using key
                                DWORD processIdToRemove = child.processId;
                                globalProcessNetworkUsageMap.erase(processIdToRemove);
                                globalProcessFileUsageMap.erase(processIdToRemove);
                                perProcessPreviousInformationMap.erase(processIdToRemove);
                                
                                //For processIdToHandleMap, we need to 'CloseHandle' before erasing the entry IF it exists in the map
                                auto it = processIdToHandleMap.find(processIdToRemove);
                                if(it != processIdToHandleMap.end())
                                {
                                    CloseHandle(it->second);
                                    processIdToHandleMap.erase(it);
                                }

                                return true;
                            }),
            processVector.end());
        
        //If this vector is empty, then remove the entry from the map itself
        if(processVector.empty())
            it = groupedProcessesMap.erase(it);
        else
            ++it;
    }
}

//--------------------FUNCTIONS FOR OUR BITSET--------------------
void CTMProcessScreen::SetPopupBit(std::uint8_t pos, bool val)
{
    popupBitset = (popupBitset & ~(1 << pos)) | (static_cast<std::uint8_t>(val) << pos);
}

bool CTMProcessScreen::GetPopupBit(std::uint8_t pos)
{
    return (popupBitset & (1 << pos));
}

void CTMProcessScreen::FlipPopupBit(std::uint8_t pos)
{
    popupBitset ^= (1 << pos);
}

//--------------------Just keeping these seperate--------------------
double CTMProcessScreen::CalculateMemoryUsage(HANDLE hProcess)
{
    double memoryUsage = 0.0;

    //Use NT API to get PrivateWorkingSetSize
    VM_COUNTERS_EX2 vmCounters = {};
    //A workaround as winternl.h doesn't include entire range of enums of PROCESSINFOCLASS
    BYTE ProcessVmCounters = 3;
    NTSTATUS status = NtQueryInformationProcess(hProcess, (PROCESSINFOCLASS)ProcessVmCounters, &vmCounters, sizeof(vmCounters), nullptr);

    if (NT_SUCCESS(status))
        memoryUsage = vmCounters.PrivateWorkingSetSize / (1024.0 * 1024.0); //Convert to MB
    else
    {
        //Fallback to GetProcessMemoryInfo if NT API fails
        PROCESS_MEMORY_COUNTERS pmc;
        if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc)))
            memoryUsage = pmc.WorkingSetSize / (1024.0 * 1024.0);
    }

    return memoryUsage;
}

double CTMProcessScreen::CalculateCpuUsage(HANDLE hProcess, DWORD processId, 
                            FILETIME ftSysKernel, FILETIME ftSysUser)
{
    //Get the current process times (we dont need ftProcCreation and ftProcExit, but we still need to pass it to the function)
    FILETIME ftProcCreation, ftProcExit, ftProcKernel, ftProcUser;
    if (!GetProcessTimes(hProcess, &ftProcCreation, &ftProcExit, &ftProcKernel, &ftProcUser))
        return -1.0;

    return CalculateCpuUsageDelta(processId, ftSysKernel, ftSysUser,
                            reinterpret_cast<LARGE_INTEGER&>(ftProcKernel), reinterpret_cast<LARGE_INTEGER&>(ftProcUser));
}

double CTMProcessScreen::CalculateCpuUsageDelta(DWORD processId, FILETIME ftSysKernel, FILETIME ftSysUser,
                            LARGE_INTEGER procKernel, LARGE_INTEGER procUser)
{
    //Get the previous CPU time information
    auto& previousCpuTimeInfo = perProcessPreviousInformationMap[processId];

    LARGE_INTEGER currentProcKernelTime = procKernel,
                  currentProcUserTime   = procUser,
    //Previous Proc times
                  prevProcKernelTime    = reinterpret_cast<LARGE_INTEGER&>(previousCpuTimeInfo.prevProcKernelTime),
                  prevProcUserTime      = reinterpret_cast<LARGE_INTEGER&>(previousCpuTimeInfo.prevProcUserTime),
    //Current Sys times
                  currentSysKernelTime  = reinterpret_cast<LARGE_INTEGER&>(ftSysKernel),
                  currentSysUserTime    = reinterpret_cast<LARGE_INTEGER&>(ftSysUser),
    //Previous Sys times
                  previousSysKernelTime = reinterpret_cast<LARGE_INTEGER&>(ftPrevSysKernelTime),
                  previousSysUserTime   = reinterpret_cast<LARGE_INTEGER&>(ftPrevSysUserTime);

    //Calculate differences
    ULONGLONG sysTimeDelta  = (currentSysKernelTime.QuadPart - previousSysKernelTime.QuadPart) + 
                              (currentSysUserTime.QuadPart - previousSysUserTime.QuadPart);
    ULONGLONG procTimeDelta = (currentProcKernelTime.QuadPart - prevProcKernelTime.QuadPart) +
                              (currentProcUserTime.QuadPart - prevProcUserTime.QuadPart);
    
    //Update the previous cpu values
    previousCpuTimeInfo.prevProcKernelTime = reinterpret_cast<FILETIME&>(procKernel);
    previousCpuTimeInfo.prevProcUserTime   = reinterpret_cast<FILETIME&>(procUser);

    //Final CPU Usage
    return (((double)procTimeDelta) / ((double)sysTimeDelta)) * 100.0;
}
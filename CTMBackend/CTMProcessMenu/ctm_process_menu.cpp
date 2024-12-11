#include "ctm_process_menu.h"

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

    //Initialize the array beforehand so we get some content to display
    UpdateProcessInfo();
    SetInitialized(true);
}

//Equivalent to OnClean function
CTMProcessScreen::~CTMProcessScreen() 
{
    CTMDestructorCleanNTDLL();
    CTMDestructorCleanEventTracingThread();
    CTMDestructorCleanMappedHandles();
    SetInitialized(false);
}

//--------------------CONSTRUCTOR INIT AND DESTRUCTOR CLEANUP FUNCTIONS--------------------
bool CTMProcessScreen::CTMConstructorInitNTDLL()
{
    hNtdll = GetModuleHandleW(L"ntdll.dll");
    if (!hNtdll)
    {
        std::cerr << "Failed to load ntdll.dll\n";
        return false;
    }

    NtQueryInformationProcess = reinterpret_cast<NtQueryInformationProcess_t>(
        GetProcAddress(hNtdll, "NtQueryInformationProcess")
    );
    NtQuerySystemInformation  = reinterpret_cast<NtQuerySystemInformation_t>(
        GetProcAddress(hNtdll, "NtQuerySystemInformation")
    );

    if (!NtQueryInformationProcess || !NtQuerySystemInformation)
    {
        std::cerr << "Failed to resolve NT DLL functions\n";
        hNtdll = nullptr;
        return false;
    }

    return true;
}

bool CTMProcessScreen::CTMConstructorInitEventTracingThread()
{
    if(!processUsageEventTracing.Start())
    {
        std::cerr << "Failed to initialize event tracing.\n";
        return false;
    }

    //Will indicate success or failure by getting the response from thread
    std::atomic<bool> initSuccess{true};

    processUsageEventTracingThread = std::thread([this, &initSuccess](){
        //Now call ProcessEvents which blocks this thread until completed (if it did not fail that is)
        if (!processUsageEventTracing.ProcessEvents())
            initSuccess.store(false);
    });

    //Sleep for 10ms which allows the above thread to start and do stuff. This way, we get an idea if the thread failed or not
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    //If ProcessEvents failed, stop the event tracing entirely
    if (!initSuccess.load())
    {
        std::cerr << "UDP event listener failed to initialize.\n";
        processUsageEventTracing.Stop();
        processUsageEventTracingThread.join(); //Ensure the thread has finished before returning
        return false;
    }

    //If initialization succeeded, return true
    return true;
}

void CTMProcessScreen::CTMDestructorCleanNTDLL()
{
    //Set all of it to nullptr
    hNtdll = nullptr;
    NtQueryInformationProcess = nullptr;
    NtQuerySystemInformation  = nullptr;
}

void CTMProcessScreen::CTMDestructorCleanEventTracingThread()
{
    processUsageEventTracing.Stop();
    if(processUsageEventTracingThread.joinable())
        processUsageEventTracingThread.join();
    
    std::cout << "THREAD DESTRUCTOR\n";
}

void CTMProcessScreen::CTMDestructorCleanMappedHandles()
{
    //Just call CloseHandle on every single entry of processIdToHandleMap
    for (auto &&[_, processHandle] : processIdToHandleMap)
        CloseHandle(processHandle);
    
    //Reset the entire map now
    processIdToHandleMap.clear();
}

//--------------------MAIN RENDER AND UPDATE FUNCTIONS--------------------
void CTMProcessScreen::OnRender()
{
    if (ImGui::BeginTable("ProcessesTable", 5, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("PID", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("CPU (%)", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Memory (MB)", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Network (MB/s)", ImGuiTableColumnFlags_WidthFixed);

        ImGui::TableHeadersRow();

        for (const auto& groupedProcess : groupedProcessesMap)
        {
            const auto& appName      = groupedProcess.first;
            const auto& appProcesses = groupedProcess.second;

            ImGui::TableNextRow();

            //First column -> name of the process (tree structure)
            ImGui::TableSetColumnIndex(0);
            bool expandTree = ImGui::TreeNodeEx(appName.c_str(), ImGuiTreeNodeFlags_SpanFullWidth);

            //Second column -> Process ID (if there is only one instance, display the process ID, else display it with instances of process)
            ImGui::TableSetColumnIndex(1);
            if(appProcesses.size() == 1)
                ImGui::Text("%d", appProcesses[0].processId);
            
            //Third, fourth and fith column -> Display total usage initially
            double totalCPUUsage = 0.0, totalMemoryUsage = 0.0, totalNetworkUsage = 0;
            for(const auto& process : appProcesses)
            {
                totalCPUUsage      += process.cpuUsage;
                totalMemoryUsage   += process.memoryUsage;
                totalNetworkUsage  += process.networkUsage;
            }

            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.2lf", totalCPUUsage);

            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%.2lf", totalMemoryUsage);

            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%.2lf", totalNetworkUsage);

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
}

void CTMProcessScreen::OnUpdate()
{
    UpdateProcessInfo();
}

//--------------------HELPER FUNCTIONS--------------------
void CTMProcessScreen::RenderProcessVector(const std::vector<ProcessInfo>& appProcesses, const std::string& parentName)
{
    for(const auto& process : appProcesses)
    {
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::Indent();
        ImGui::Text("%s", parentName.c_str());
        ImGui::Unindent();

        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%d", process.processId);

        ImGui::TableSetColumnIndex(2);
        ImGui::Text("%.2lf", process.cpuUsage);

        ImGui::TableSetColumnIndex(3);
        ImGui::Text("%.2lf", process.memoryUsage);

        ImGui::TableSetColumnIndex(4);
        ImGui::Text("%.2lf", process.networkUsage);
    }
}

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
        std::cerr << "Failed to get system process information.\n";
}

void CTMProcessScreen::UpdateProcessMapWithProcessHandle(HANDLE hProcess, DWORD processId, const std::string& processName,
                                                            FILETIME ftSysKernel, FILETIME ftSysUser)
{
    /*
     * Some processes allow OpenProcess to run on them, which can be used to get valid stuff without using weird undocumented custom stuff
     */
    //Memory Usage
    DOUBLE memUsage = CalculateMemoryUsage(hProcess);

    //CPU Usage
    DOUBLE cpuUsage = CalculateCpuUsage(hProcess, processId, ftSysKernel, ftSysUser);

    //Network Usage
    DOUBLE networkUsage = 0;
    auto it = globalProcessNetworkUsageMap.find(processId);
    if(it != globalProcessNetworkUsageMap.end())
    {
        //Set it to 0 as soon as you use it, that will mark the data per second
        networkUsage = (it->second / (1024.0 * 1024.0));
        it->second   = 0;
    }

    //Update the grouped processes map
    UpdateProcessMap(processId, processName, memUsage, cpuUsage, networkUsage);
}

void CTMProcessScreen::UpdateProcessMapWithoutProcessHandle(DWORD processId, const std::string& processName,
                                PCTM_SYSTEM_PROCESS_INFORMATION processInformation, FILETIME ftSysKernel, FILETIME ftSysUser)
{
    /*
     * The process cant be opened, we will use some undocumented, non backwards compatibility stuff. THIS IS THE ONLY WAY
     */
    //Memory Usage
    DOUBLE memUsage = (processInformation->WorkingSetPrivateSize.QuadPart / (1024.0 * 1024.0));

    //Network Usage
    DOUBLE networkUsage = 0;
    auto it = globalProcessNetworkUsageMap.find(processId);
    if(it != globalProcessNetworkUsageMap.end())
    {
        //Set it to 0 as soon as you use it, that will mark the data per second
        networkUsage = (it->second / (1024.0 * 1024.0));
        it->second   = 0;
    }

    //CPU Usage
    DOUBLE cpuUsage = CalculateCpuUsageDelta(processId, ftSysKernel, ftSysUser,
                            processInformation->KernelTime, processInformation->UserTime);

    UpdateProcessMap(processId, processName, memUsage, cpuUsage, networkUsage);
}

void CTMProcessScreen::UpdateProcessMap(DWORD processId, const std::string& processName,
                        double memUsage, double cpuUsage, double networkUsage)
{
    //Get the process if it exists. If it doesn't exist, it will auto create it for us
    auto& processVector = groupedProcessesMap[processName];

    //If the processVector is empty that means its an entirely new process entry
    if(processVector.empty())
        processVector.emplace_back(processId, memUsage, cpuUsage, networkUsage);
    
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
            it->isStaleEntry = FALSE;
        }
        //The element does not exist, its a new one under this category
        else
            processVector.emplace_back(processId, memUsage, cpuUsage, networkUsage);
    }
}

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
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    //Successfully opened the process, store the handle in the map and return it
    if (hProcess)
        processIdToHandleMap[processId] = hProcess;
    //Failed to open process, add the processId to the exclusion set
    else
        processExcludedHandleSet.insert(processId);

    return hProcess;
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
    return (((DOUBLE)procTimeDelta) / ((DOUBLE)sysTimeDelta)) * 100.0;
}
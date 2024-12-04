#include "ctm_process_menu.h"

//Equivalent to OnInit function
CTMProcessScreen::CTMProcessScreen()
    : processInfoBuffer(processInfoBufferSize)
{
    //Get NT api module
    if(!CTMConstructorInitNTDLL())
        return;
    
    //Start a new thread for udp usage listener
    if(!CTMConstructorInitUDPUsageThread())
        return;

    //Initialize the array beforehand so we get some content
    UpdateProcessInfo();
    SetInitialized(true);
}

//Equivalent to OnClean function
CTMProcessScreen::~CTMProcessScreen()
{
    CTMDestructorCleanNTDLL();
    CTMDestructorCleanUDPUsageThread();
    SetInitialized(false);
}

//--------------------CONSTRUCTOR INIT AND DESTRUCTOR CLEANUP FUNCTIONS--------------------
bool CTMProcessScreen::CTMConstructorInitNTDLL()
{
    //Get NT api module
    hNtdll = GetModuleHandleW(L"ntdll.dll");
    if (!hNtdll)
    {
        std::cerr << "Failed to load ntdll.dll\n";
        return false;
    }

    //Resolve NtQueryInformationProcess
    NtQueryInformationProcess = reinterpret_cast<NtQueryInformationProcess_t>(
        GetProcAddress(hNtdll, "NtQueryInformationProcess")
    );
    NtQuerySystemInformation  = reinterpret_cast<NtQuerySystemInformation_t>(
        GetProcAddress(hNtdll, "NtQuerySystemInformation")
    );

    if (!NtQueryInformationProcess || !NtQuerySystemInformation)
    {
        std::cerr << "Failed to resolve NT DLL functions\n";
        FreeLibrary(hNtdll);
        hNtdll = nullptr;
        return false;
    }

    return true;
}

bool CTMProcessScreen::CTMConstructorInitUDPUsageThread()
{
    if(!udpUsageEventListener.Start())
    {
        std::cerr << "Failed to initialize UDP Usage listener\n";
        return false;
    }

    //Will indicate success or failure by getting the response from thread
    std::atomic<bool> initSuccess{true};

    udpUsageEventListenerThread = std::thread([this, &initSuccess](){
        //Now call ProcessEvents which blocks this thread until completed
        if (!udpUsageEventListener.ProcessEvents())
            initSuccess.store(false);
    });

    //Sleep for 10ms which allows thread to start and do stuff till then
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    //If ProcessEvents failed, handle accordingly
    if (!initSuccess.load())
    {
        std::cerr << "UDP event listener failed to initialize.\n";
        udpUsageEventListener.Stop();
        udpUsageEventListenerThread.join(); //Ensure the thread has finished before returning
        return false;
    }

    //If initialization succeeded, return true
    return true;
}

void CTMProcessScreen::CTMDestructorCleanNTDLL()
{
    //Free Library
    if (hNtdll)
    {
        FreeLibrary(hNtdll);
        hNtdll = nullptr;
    }
    //Set Function pointers to null
    NtQueryInformationProcess = nullptr;
    NtQuerySystemInformation  = nullptr;
}

void CTMProcessScreen::CTMDestructorCleanUDPUsageThread()
{
    udpUsageEventListener.Stop();
    if(udpUsageEventListenerThread.joinable())
        udpUsageEventListenerThread.join();
    
    std::cout << "THREAD DESTRUCTOR\n";
}

//--------------------MAIN RENDER AND UPDATE FUNCTIONS--------------------
void CTMProcessScreen::OnRender()
{
    if (ImGui::BeginTable("ProcessesTable", 6, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("CPU (%)", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Memory (MB)", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Disk (MB/s)", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("UDP Usage(Bytes/s)", ImGuiTableColumnFlags_WidthFixed);

        ImGui::TableHeadersRow();

        for (const auto& process : groupedProcesses)
        {
            const auto& appName      = process.first;
            const auto& appProcesses = process.second;

            ImGui::TableNextRow();

            //First column -> name of the process (tree structure)
            ImGui::TableSetColumnIndex(0);
            bool expandTree = ImGui::TreeNodeEx(appName.c_str(), ImGuiTreeNodeFlags_SpanFullWidth);

            //Second column -> Process ID (if there is only one instance, display the process ID, else display it with instances of process)
            ImGui::TableSetColumnIndex(1);
            if(appProcesses.size() == 1)
                ImGui::Text("%d", appProcesses[0].processId);
            
            //Third, fourth and fith column -> Display total usage initially
            double totalCPUUsage = 0.0, totalMemoryUsage = 0.0, totalDiskUsage = 0.0;
            ULONGLONG totalUDPUsageBytes = 0;
            for(const auto& process : appProcesses)
            {
                totalCPUUsage      += process.cpuUsage;
                totalMemoryUsage   += process.memoryUsage;
                totalDiskUsage     += process.diskUsage;
                totalUDPUsageBytes += process.udpUsage;
            }

            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.2lf", totalCPUUsage);

            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%.2lf", totalMemoryUsage);

            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%.2lf", totalDiskUsage);

            ImGui::TableSetColumnIndex(5);
            ImGui::Text("%llu", totalUDPUsageBytes);

            //If we expand the tree, display rest of the details
            if(expandTree)
            {
                RenderProcessInstances(appProcesses);
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
void CTMProcessScreen::RenderProcessInstances(const std::vector<ProcessInfo>& appProcesses)
{
    for(const auto& process : appProcesses)
    {
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::Indent();
        ImGui::Text("%s", process.name.c_str());
        ImGui::Unindent();

        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%d", process.processId);

        ImGui::TableSetColumnIndex(2);
        ImGui::Text("%.2lf", process.cpuUsage);

        ImGui::TableSetColumnIndex(3);
        ImGui::Text("%.2lf", process.memoryUsage);

        ImGui::TableSetColumnIndex(4);
        ImGui::Text("%.2lf", process.diskUsage);
    }
}

void CTMProcessScreen::UpdateProcessInfo()
{
    NTSTATUS status;
    //Resize buffer as long as we get an error, aka the buffer isnt big enough
    do
    {
        status = NtQuerySystemInformation(SystemProcessInformation, processInfoBuffer.data(),
                                                    processInfoBufferSize, &processInfoBufferSize);

        if(status == STATUS_INFO_LENGTH_MISMATCH)
            processInfoBuffer.resize(processInfoBufferSize);
    }
    while (status == STATUS_INFO_LENGTH_MISMATCH);

    //Do the do
    if(status == STATUS_SUCCESS)
    {
        PSYSTEM_PROCESS_INFORMATION systemProcessInfo = reinterpret_cast<PSYSTEM_PROCESS_INFORMATION>(processInfoBuffer.data());

        //Process name converted from a wide string to normal string
        CHAR processName[MAX_PATH] = "(SystemProcess)";

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

            DWORD processId = static_cast<DWORD>(reinterpret_cast<uintptr_t>(systemProcessInfo->UniqueProcessId));
            HANDLE hProcess = nullptr;

            if(processId != 0)
                hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);

            if (hProcess == nullptr)
            {
                if (GetLastError() == ERROR_ACCESS_DENIED)
                    UpdateMapWithProcessInfo(processId, processName, 0.0, 0.0, 0.0, 0ULL);
                else if(processId != 0)
                    std::cerr << "Failed to open process ID " << processId << ". Error: " << GetLastError() << "\n";
            } 
            else
            {
                UpdateMapWithProcessInfo(processId, processName, 1.0, 0.0, 0.0, 0ULL);
                CloseHandle(hProcess);
            }

            //No more entries, break outta loop
            if(systemProcessInfo->NextEntryOffset == 0)
                break;
            
            //More entries, go forward
            systemProcessInfo = reinterpret_cast<PSYSTEM_PROCESS_INFORMATION>(
                                    reinterpret_cast<BYTE*>(systemProcessInfo) + systemProcessInfo->NextEntryOffset
                                );
        }

        //Time to remove stale entries
        RemoveStaleEntries();
    }
    else
        std::cerr << "Failed to get system process information.\n";

    //Lets also clean the globalProcessUDPUsageMap to reset values
    std::size_t previousMapSize = globalProcessUDPUsageMap.size();
    globalProcessUDPUsageMap.clear();
    globalProcessUDPUsageMap.reserve(previousMapSize);
}

void CTMProcessScreen::UpdateMapWithProcessInfo(DWORD processId, const std::string& processName,
                        double memUsage, double diskUsage, double cpuUsage, ULONGLONG udpUsage)
{
    auto it = groupedProcesses.find(processName);
    //The process is not new
    if(it != groupedProcesses.end())
    {
        auto& processVector = it->second;
        bool  processFound  = false;

        //Try to find the process id in vector
        for (auto &&child : processVector)
        {
            //Found the process, update it
            if(child.processId == processId)
            {
                //Update the value and mark the entry as 'not stale'
                child.isStaleEntry = FALSE;
                processFound = true;
                break;
            }
        }
        if(!processFound)
            //We did not find the process, it's a new process under this group
            processVector.emplace_back(processId, processName, memUsage, diskUsage, cpuUsage, udpUsage);
    }
    //It's a new process, add it to the group
    else
        groupedProcesses[processName].emplace_back(processId, processName, memUsage, diskUsage, cpuUsage, udpUsage);
}

void CTMProcessScreen::RemoveStaleEntries()
{
    for(auto it = groupedProcesses.begin(); it != groupedProcesses.end(); )
    {
        auto& processVector = it->second;

        //Check for stale entries in process vector and remove them
        processVector.erase(std::remove_if(processVector.begin(), processVector.end(),
                                            //The logic is, if the entry has been marked as 'FALSE' by the 'UpdateProcessInfo' function-
                                            //-then the entry will be marked as stale by this code but won't remove it
                                            //Else, the entry is removed
                                            [](ProcessInfo& child)
                                            {
                                                if(!child.isStaleEntry) {
                                                    child.isStaleEntry = TRUE;
                                                    return false;
                                                }
                                                return true;
                                            }),
                            processVector.end());
        
        //If this vector is empty, then remove the entry from the map itself
        if(processVector.empty())
            it = groupedProcesses.erase(it);
        else
            ++it;
    }
}

// void CTMProcessScreen::UpdateProcessInfo()
// {
//     FILETIME sysIdle, sysKernel, sysUser;
//     GetSystemTimes(&sysIdle, &sysKernel, &sysUser);

//     //Updated with the latest information on processes, then moved to the original "groupedProcesses" map
//     ProcessMap updatedGroupedProcesses;
//     //Reserve beforehand to make it a bit efficient
//     updatedGroupedProcesses.reserve(groupedProcesses.size());

//     //Get all the process ids
//     DWORD processIds[2048], processCount;
//     if (!EnumProcesses(processIds, sizeof(processIds), &processCount))
//         return;

//     processCount /= sizeof(DWORD);

//     //Before we update anything, lock the mutex. This mutex stops the udp listener from futher processing events-
//     //-until we unlock it
//     std::lock_guard<std::mutex> lock(udpUsageMapMutex);

//     for (unsigned int i = 0; i < processCount; i++)
//     {
//         DWORD processId = processIds[i];
//         if (processId == 0)
//             continue;

//         HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
//         if (hProcess)
//         {
//             CHAR processName[MAX_PATH] = "<unknown>";
//             HMODULE hMod;
//             DWORD cbNeeded;

//             //Get application base name
//             if (EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeeded))
//                 GetModuleBaseNameA(hProcess, hMod, processName, sizeof(processName) / sizeof(CHAR));

//             //Calculate Memory Usage per process
//             double memoryUsage = CalculateMemoryUsage(hProcess);

//             //Calculate Disk usage per process
//             double diskUsage   = CalculateDiskUsage(hProcess, processId);

//             //Calculate CPU usage per process
//             FILETIME ftProcCreation, ftProcExit, ftProcKernel, ftProcUser;
//             double cpuUsage = 0.0;

//             if (GetProcessTimes(hProcess, &ftProcCreation, &ftProcExit, &ftProcKernel, &ftProcUser))
//             {
//                 auto& prevInfo = perProcessPreviousInformation[processId];

//                 if (prevInfo.prevProcKernelTime.dwLowDateTime || prevInfo.prevProcKernelTime.dwHighDateTime)
//                 {
//                     cpuUsage = CalculateCpuUsage(
//                         hProcess,
//                         prevSysKernel, prevSysUser,
//                         prevInfo.prevProcKernelTime, prevInfo.prevProcUserTime
//                     );
//                 }

//                 //Update the previous information
//                 prevInfo.prevProcKernelTime = ftProcKernel;
//                 prevInfo.prevProcUserTime = ftProcUser;
//             }

//             //Calculate udp usage per process
//             ULONGLONG udpUsage = 0;
//             auto it = globalProcessUDPUsageMap.find(processId);
//             if(it != globalProcessUDPUsageMap.end())
//                 udpUsage = it->second;

//             updatedGroupedProcesses[processName].emplace_back(processId, processName, memoryUsage, diskUsage, cpuUsage, udpUsage);
//             CloseHandle(hProcess);
//         }
//     }

//     prevSysKernel = sysKernel;
//     prevSysUser = sysUser;

//     groupedProcesses = std::move(updatedGroupedProcesses);
//     //Lets also clean the globalProcessUDPUsageMap to reset values
//     std::size_t previousMapSize = globalProcessUDPUsageMap.size();
//     globalProcessUDPUsageMap.clear();
//     globalProcessUDPUsageMap.reserve(previousMapSize);
// }

double CTMProcessScreen::CalculateDiskUsage(HANDLE hProcess, DWORD processId)
{
    double diskUsage = 0.0;
    
    //Get current disk io usage
    IO_COUNTERS ioCounters = {};
   
    if(GetProcessIoCounters(hProcess, &ioCounters))
    {
        auto& prevInfo = perProcessPreviousInformation[processId];

        ULONGLONG readBytesDelta  = ioCounters.ReadTransferCount  - prevInfo.prevIOCounter.ReadTransferCount;
        ULONGLONG writeBytesDelta = ioCounters.WriteTransferCount - prevInfo.prevIOCounter.WriteTransferCount;

        diskUsage = (readBytesDelta + writeBytesDelta) / (1024.0 * 1024.0); //MB/s

        prevInfo.prevIOCounter = ioCounters;
    }

    return diskUsage;
}

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

double CTMProcessScreen::CalculateCpuUsage(HANDLE hProcess, FILETIME ftPrevSysKernel, FILETIME ftPrevSysUser, FILETIME ftPrevProcKernel, FILETIME ftPrevProcUser)
{
    FILETIME ftSysIdle, ftSysKernel, ftSysUser;
    FILETIME ftProcCreation, ftProcExit, ftProcKernel, ftProcUser;

    // Get system times
    if (!GetSystemTimes(&ftSysIdle, &ftSysKernel, &ftSysUser)) {
        return -1.0;
    }

    // Get process times
    if (!GetProcessTimes(hProcess, &ftProcCreation, &ftProcExit, &ftProcKernel, &ftProcUser)) {
        return -1.0;
    }

    // Convert FILETIME to ULARGE_INTEGER for arithmetic
    ULARGE_INTEGER sysKernel, sysUser, procKernel, procUser;
    ULARGE_INTEGER prevSysKernel, prevSysUser, prevProcKernel, prevProcUser;

    sysKernel.QuadPart = ((ULARGE_INTEGER&)ftSysKernel).QuadPart;
    sysUser.QuadPart = ((ULARGE_INTEGER&)ftSysUser).QuadPart;
    procKernel.QuadPart = ((ULARGE_INTEGER&)ftProcKernel).QuadPart;
    procUser.QuadPart = ((ULARGE_INTEGER&)ftProcUser).QuadPart;

    prevSysKernel.QuadPart = ((ULARGE_INTEGER&)ftPrevSysKernel).QuadPart;
    prevSysUser.QuadPart = ((ULARGE_INTEGER&)ftPrevSysUser).QuadPart;
    prevProcKernel.QuadPart = ((ULARGE_INTEGER&)ftPrevProcKernel).QuadPart;
    prevProcUser.QuadPart = ((ULARGE_INTEGER&)ftPrevProcUser).QuadPart;

    // Calculate differences
    ULONGLONG sysTimeDelta = (sysKernel.QuadPart - prevSysKernel.QuadPart) + (sysUser.QuadPart - prevSysUser.QuadPart);
    ULONGLONG procTimeDelta = (procKernel.QuadPart - prevProcKernel.QuadPart) + (procUser.QuadPart - prevProcUser.QuadPart);

    // Get the number of logical processors
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    int numProcessors = sysInfo.dwNumberOfProcessors;

    // Calculate CPU usage as a percentage
    return (double(procTimeDelta) / double(sysTimeDelta)) * numProcessors * 100.0;
}
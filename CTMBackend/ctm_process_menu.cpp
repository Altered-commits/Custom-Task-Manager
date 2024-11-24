#include "ctm_process_menu.h"

//Equivalent to OnInit function
CTMProcessScreen::CTMProcessScreen()
{
    //Get NT api module
    hNtdll = GetModuleHandleW(L"ntdll.dll");
    if (!hNtdll)
    {
        std::cerr << "Failed to load ntdll.dll\n";
        return;
    }

    //Resolve NtQueryInformationProcess
    NtQueryInformationProcess = reinterpret_cast<NtQueryInformationProcess_t>(
        GetProcAddress(hNtdll, "NtQueryInformationProcess")
    );

    if (!NtQueryInformationProcess)
    {
        std::cerr << "Failed to resolve NtQueryInformationProcess\n";
        FreeLibrary(hNtdll);
        hNtdll = nullptr;
        return;
    }

    //Initialize the array beforehand so we get some content
    UpdateProcessInfo();
    SetInitialized(true);
}

//Equivalent to OnClean function
CTMProcessScreen::~CTMProcessScreen()
{
    if (hNtdll)
    {
        FreeLibrary(hNtdll);
        hNtdll = nullptr;
    }
    SetInitialized(false);
}

void CTMProcessScreen::OnRender()
{
    if (ImGui::BeginTable("ProcessesTable", 5, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("CPU (%)", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Memory (MB)", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Disk (MB/s)", ImGuiTableColumnFlags_WidthFixed);

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
            for(const auto& process : appProcesses)
            {
                totalCPUUsage    += process.cpuUsage;
                totalMemoryUsage += process.memoryUsage;
                totalDiskUsage   += process.diskUsage;
            }

            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.2lf", totalCPUUsage);

            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%.2lf", totalMemoryUsage);

            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%.2lf", totalDiskUsage);

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

//Helper functions
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
    FILETIME sysIdle, sysKernel, sysUser;
    GetSystemTimes(&sysIdle, &sysKernel, &sysUser);

    //Updated with the latest information on processes, then moved to the original "groupedProcesses" map
    ProcessMap updatedGroupedProcesses;

    //Get all the process ids
    DWORD processIds[1024], processCount;
    if (!EnumProcesses(processIds, sizeof(processIds), &processCount))
        return;

    processCount /= sizeof(DWORD);

    for (unsigned int i = 0; i < processCount; i++)
    {
        DWORD processId = processIds[i];
        if (processId == 0)
            continue;

        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
        if (hProcess)
        {
            CHAR processName[MAX_PATH] = "<unknown>";
            HMODULE hMod;
            DWORD cbNeeded;

            //Get application base name
            if (EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeeded))
                GetModuleBaseNameA(hProcess, hMod, processName, sizeof(processName) / sizeof(CHAR));

            //Calculate Memory Usage per process
            double memoryUsage = CalculateMemoryUsage(hProcess);

            //Calculate Disk usage per process
            double diskUsage   = CalculateDiskUsage(hProcess, processId);

            //Calculate CPU usage per process
            FILETIME ftProcCreation, ftProcExit, ftProcKernel, ftProcUser;
            double cpuUsage = 0.0;

            if (GetProcessTimes(hProcess, &ftProcCreation, &ftProcExit, &ftProcKernel, &ftProcUser))
            {
                auto& prevInfo = perProcessPreviousInformation[processId];

                if (prevInfo.prevProcKernelTime.dwLowDateTime || prevInfo.prevProcKernelTime.dwHighDateTime)
                {
                    cpuUsage = CalculateCpuUsage(
                        hProcess,
                        prevSysKernel, prevSysUser,
                        prevInfo.prevProcKernelTime, prevInfo.prevProcUserTime
                    );
                }

                // Update the previous information
                prevInfo.prevProcKernelTime = ftProcKernel;
                prevInfo.prevProcUserTime = ftProcUser;
            }

            updatedGroupedProcesses[processName].emplace_back(processId, processName, memoryUsage, diskUsage, cpuUsage);
            CloseHandle(hProcess);
        }
    }

    prevSysKernel = sysKernel;
    prevSysUser = sysUser;

    groupedProcesses = std::move(updatedGroupedProcesses);
}

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
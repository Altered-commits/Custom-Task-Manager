#include "ctm_perf_cpu_screen.h"

//Hindering with my std::min. I just decided i did not want max as well so yeah
#undef min
#undef max

//Equivalent to OnInit function
CTMPerformanceCPUScreen::CTMPerformanceCPUScreen()
{
    //Try getting 'NtQuerySystemInformation' function from ntdll.dll, if can't don't let this class initialize
    if(!CTMConstructorInitNTDLL())
        return;

    //Try getting entire CPU info, if we can't, return. Never let it initialize
    if(!CTMConstructorGetCPUInfo())
        return;

    SetInitialized(true);
}

//Equivalent to OnClean function
CTMPerformanceCPUScreen::~CTMPerformanceCPUScreen()
{
    SetInitialized(false);
}

//--------------------CONSTRUCTOR FUNCTIONS--------------------
bool CTMPerformanceCPUScreen::CTMConstructorInitNTDLL()
{
    //Try getting ntdll.dll module
    hNtdll = GetModuleHandleA("ntdll.dll");
    if(!hNtdll)
    {
        std::cerr << "Failed to get ntdll.dll\n";
        return false;
    }

    NtQuerySystemInformation  = reinterpret_cast<NtQuerySystemInformation_t>(
        GetProcAddress(hNtdll, "NtQuerySystemInformation")
    );

    if(!NtQuerySystemInformation)
    {
        std::cerr << "Failed to resolve NT DLL function\n";
        return false;
    }

    return true;
}

bool CTMPerformanceCPUScreen::CTMConstructorGetCPUInfo()
{
    //If we failed to get CPU name, return
    if(!CTMConstructorGetCPUInfoFromRegistry())
        return false;
    
    //Initialize system information
    SYSTEM_INFO sysInfo = { 0 };
    GetSystemInfo(&sysInfo);

    //Get system architecture (string version)
    auto archIdx = static_cast<std::size_t>(MetricsVectorIndex::Architecture);
    switch(sysInfo.wProcessorArchitecture)
    {
        case PROCESSOR_ARCHITECTURE_AMD64:
            metricsVector[archIdx].second = "x64";
            break;
        case PROCESSOR_ARCHITECTURE_ARM:
            metricsVector[archIdx].second = "ARM";
            break;
        case PROCESSOR_ARCHITECTURE_ARM64:
            metricsVector[archIdx].second = "ARM64";
            break;
        case PROCESSOR_ARCHITECTURE_INTEL:
            metricsVector[archIdx].second = "x86";
            break;
        case PROCESSOR_ARCHITECTURE_IA64:
            metricsVector[archIdx].second = "Intel Itanium-based";
            break;
        default:
            metricsVector[archIdx].second = "Unknown";
            break;
    }

    //Get no. of logical processors
    DWORD numLogicalProcessors = sysInfo.dwNumberOfProcessors;
    metricsVector[static_cast<std::size_t>(MetricsVectorIndex::LogicalProcessors)].second = numLogicalProcessors;

    //Also while we are at it, reserve enough memory in 'currInfo' and 'prevInfo' vectors. They contain per logical processor usage
    currProcessorPerformanceInfo.resize(numLogicalProcessors);
    prevProcessorPerformanceInfo.resize(numLogicalProcessors);

    //Also set the total size taken by the vectors. Size is how many bytes they use in memory
    processorPerformanceInfoSize = numLogicalProcessors * sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION);

    //ALSO set the grid rows and cols for the heatmap, sqrt and ceil to find the number of columns (we ceil cuz we want more columns than rows)
    //Use some weird ass formula to calculate number of rows
    heatmapGridColumns = static_cast<DWORD>(std::ceil(std::sqrt(numLogicalProcessors)));
    heatmapGridRows    = (numLogicalProcessors + heatmapGridColumns - 1) / heatmapGridColumns;

    //This needs to be different from others as grid rows and columns are not necessarily same, hence its total size is rows * cols
    //-1.0 denotes that the cell is not in use, if the value wasn't overriden that is
    perLogicalProcessorUsage.resize(heatmapGridRows * heatmapGridColumns, -1.0);

    //Process rest of the CPU info
    return CTMConstructorGetCPULogicalInfo();
}

bool CTMPerformanceCPUScreen::CTMConstructorGetCPUInfoFromRegistry()
{
    //Get Registry cuz HKEY_LOCAL_MACHINE has the cpu name
    HKEY hKey;
    LSTATUS result = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hKey);
    if(result != ERROR_SUCCESS)
    {
        std::cerr << "Failed to open the 'HKEY_LOCAL_MACHINE' for reading CPU name and base speed. Error: " << result << '\n';
        return false;
    }

    //Get the CPU name... atleast try to get it
    if(!CTMConstructorGetCPUName(hKey))
    {
        RegCloseKey(hKey);
        return false;
    }

    //Get the base speed in MHz, we will convert it to GHz
    DWORD type          = 0;
    DWORD baseSpeed     = 0;
    DWORD baseSpeedSize = sizeof(baseSpeed);
    result = RegQueryValueExA(hKey, "~MHz", nullptr, &type, reinterpret_cast<LPBYTE>(&baseSpeed), &baseSpeedSize);
    if(result != ERROR_SUCCESS || type != REG_DWORD)
    {
        std::cerr << "Failed to retrieve '~MHz'. Error: " << result << '\n';
        RegCloseKey(hKey);
        return false;
    }
    //Add it manually to the metricsVector, converting it into GHz
    metricsVector[static_cast<std::size_t>(MetricsVectorIndex::BaseSpeed)].second = baseSpeed / 1000.0;

    //Finally close the registry
    RegCloseKey(hKey);
    return true;
}

bool CTMPerformanceCPUScreen::CTMConstructorGetCPUName(HKEY hKey)
{
    DWORD type = 0;
    DWORD cpuNameBufferSize = 0;

    LSTATUS result = RegQueryValueExA(hKey, "ProcessorNameString", nullptr, &type, nullptr, &cpuNameBufferSize);
    if(result != ERROR_SUCCESS || type != REG_SZ)
    {
        std::cerr << "Failed to query size of 'ProcessorNameString'. Error: " << result << '\n';
        return false;
    }

    //Give it a big enough buffer, the value is given by RegQueryValueExA
    std::unique_ptr<BYTE[]> cpuNameBufferPtr = std::make_unique<BYTE[]>(cpuNameBufferSize);

    //Try getting CPU name
    result = RegQueryValueExA(hKey, "ProcessorNameString", nullptr, &type, cpuNameBufferPtr.get(), &cpuNameBufferSize);
    if(result != ERROR_SUCCESS || type != REG_SZ)
    {
        std::cerr << "Failed to retrieve 'ProcessorNameString'. Error: " << result << '\n';
        return false;
    }

    //We were able to retrieve it. Now heres the thing, if the cpuNameBufferSize is greater than sizeof(cpuNameBuffer), we add '...' at the end
    //Else we just copy it as is with null terminator at the end
    constexpr DWORD maxBufferSize = sizeof(cpuNameBuffer);
    if(cpuNameBufferSize > maxBufferSize)
    {
        //Copy name and append "..."
        std::memcpy(cpuNameBuffer, cpuNameBufferPtr.get(), maxBufferSize - 4);
        cpuNameBuffer[maxBufferSize - 4] = '.';
        cpuNameBuffer[maxBufferSize - 3] = '.';
        cpuNameBuffer[maxBufferSize - 2] = '.';
        cpuNameBuffer[maxBufferSize - 1] = '\0';
    }
    else
    {
        //Copy full name and ensure we null terminate it
        std::memcpy(cpuNameBuffer, cpuNameBufferPtr.get(), cpuNameBufferSize);
        cpuNameBuffer[std::min(cpuNameBufferSize, maxBufferSize - 1)] = '\0';
    }

    return true;
}

bool CTMPerformanceCPUScreen::CTMConstructorGetCPULogicalInfo()
{
    //Get processor information, but before that, get the size required to hold this buffer
    std::unique_ptr<BYTE[]>               processorInfoBuffer  = nullptr;
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION processorInfoPtr     = nullptr;
    DWORD                                 processorInfoLen     = 0; //This is the total bytes it takes in memory
    DWORD                                 processorInfoCount   = 0; //This is the number of entries basically
    //Some processor information collected below
    DWORD                                 processorCoreCount   = 0;
    DWORD                                 processorSocketCount = 0; //Number of sockets in motherboard where CPU can be installed
    ULONGLONG                             processorL1CacheSize = 0;
    ULONGLONG                             processorL2CacheSize = 0;
    ULONGLONG                             processorL3CacheSize = 0;

    GetLogicalProcessorInformation(nullptr, &processorInfoLen);
    //Check for insufficient buffer, it would be this case most of the time as processorInfoLen is 0 beforehand
    DWORD errorCode = GetLastError();
    if(errorCode == ERROR_INSUFFICIENT_BUFFER)
    {
        //Resize the buffer
        processorInfoBuffer = std::make_unique<BYTE[]>(processorInfoLen);

        //Try getting the processor information again. If it fails, then return false
        if(!GetLogicalProcessorInformation(reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION>(processorInfoBuffer.get()),
                                &processorInfoLen))
        {
            std::cerr << "Failed to get processor information. Error code: " << GetLastError() << '\n';
            return false;
        }
    }
    //Else it failed due to some other error, simply return false
    else
    {
        std::cerr << "GetLogicalProcessorInformation failed initially with error code: " << errorCode << '\n';
        return false;
    }
    //Well if you reached till here, that means we were successful in retrieving the processor information
    //Assign the pointer to the buffer, this pointer is responsible for moving around buffer getting information
    //Also get the total number of entries
    processorInfoPtr   = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION>(processorInfoBuffer.get());
    processorInfoCount = processorInfoLen / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
    
    //Loop through all the entries in the buffer. Think of it as an array of SYSTEM_LOGICAL_... (i'm not typing it all)
    for(DWORD idx = 0; idx < processorInfoCount; idx++)
    {
        switch (processorInfoPtr->Relationship)
        {
            case RelationProcessorCore:
                processorCoreCount++; //Count the total number of physical cores
                break;

            case RelationProcessorPackage:
                processorSocketCount++;
                break;

            case RelationCache:
            {
                PCACHE_DESCRIPTOR cache     = &processorInfoPtr->Cache;
                DWORD             cacheSize = cache->Size;
                switch (cache->Level)
                {
                    case 1: //L1 cache
                        processorL1CacheSize += cacheSize;
                        break;
                    case 2: //L2 cache
                        processorL2CacheSize += cacheSize;
                        break;
                    case 3: //L3 cache
                        processorL3CacheSize += cacheSize;
                        break;
                }
                break;
            }
        }

        //Go forward to next entry
        processorInfoPtr++;
    }

    //Add all the collected information to the 'metricsVector'
    //1) Sockets
    metricsVector[static_cast<std::size_t>(MetricsVectorIndex::Sockets)].second = processorSocketCount;
    //2) Cores
    metricsVector[static_cast<std::size_t>(MetricsVectorIndex::Cores)].second = processorCoreCount;
    //3) Cache size, if they even exist. Also we converting cache size into MB
    if(processorL1CacheSize)
        metricsVector.emplace_back("L1 Cache", (processorL1CacheSize / (1024.0 * 1024.0)));
    if(processorL2CacheSize)
        metricsVector.emplace_back("L2 Cache", (processorL2CacheSize / (1024.0 * 1024.0)));
    if(processorL3CacheSize)
        metricsVector.emplace_back("L3 Cache", (processorL3CacheSize / (1024.0 * 1024.0)));
    
    //Everything went good
    return true;
}

//--------------------MAIN RENDER AND UPDATE FUNCTIONS--------------------
void CTMPerformanceCPUScreen::OnRender()
{
    ImVec2 windowSize = ImGui::GetWindowSize();
    //Plot the CPU Usage Graph, also specify the max and min range of the graph cuz why not
    ImGui::TextUnformatted("100%");
    PlotUsageGraph("Overall CPU Usage (over 60 Seconds)", {-1, 300}, 0.0f, 100.0f, { 0.075f, 0.792f, 0.988f, 1.0f });
    ImGui::TextUnformatted("0%");

    //Give some spacing before displaying CPU Info
    ImGui::Dummy({-1.0f, 15.0f});

    //Add some padding to the frame (see the collapsing header, see the blank space around the text)
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 10.0f));

    //Create a collapsable header containing our per logical processors heatmap
    if(ImGui::CollapsingHeader("Logical Processors Heatmap"))
        RenderLogicalProcessorHeatmap();

    //Even more spacing
    ImGui::Dummy({-1.0f, 5.0f});

    if(ImGui::CollapsingHeader("CPU Statistics"))
        RenderCPUStatistics();
    
    ImGui::PopStyleVar();
}

void CTMPerformanceCPUScreen::OnUpdate()
{
    UpdateXAxis();
    double currentCpuUsage = GetTotalCPUUsage();
    PlotPoint(GetCurrentXAxisValue(), static_cast<float>(currentCpuUsage));

    //Add all the stuff to metrics vector
    metricsVector[0].second = currentCpuUsage;

    //Update per logical processor information
    UpdatePerLogicalProcessorInfo();
}

//--------------------RENDER FUNCTIONS--------------------
void CTMPerformanceCPUScreen::RenderCPUStatistics()
{
    //Show the statistics in a table format
    if(ImGui::BeginTable("CPUStatisticsTable", 2, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV))
    {
        ImGui::TableSetupColumn("Metrics");
        ImGui::TableSetupColumn("Values");
        ImGui::TableHeadersRow();

        for(std::size_t i = 0; i < metricsVector.size(); i++)
        {
            auto&&[metric, value] = metricsVector[i];

            ImGui::TableNextRow();
            //Metrics column
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(metric);
            //Value column
            ImGui::TableSetColumnIndex(1);
            std::visit([&i](auto&& arg)
            {
                using T = std::decay_t<decltype(arg)>;

                //If its CPU usage display '%', if its BaseSpeed display GHz, else display 'MB' (at the end of value)
                if constexpr(std::is_same_v<T, double>)
                {
                    if(i == static_cast<std::size_t>(MetricsVectorIndex::Usage))
                        ImGui::Text("%.2lf%%", arg);
                    else if(i == static_cast<std::size_t>(MetricsVectorIndex::BaseSpeed))
                        ImGui::Text("%.2lfGHz", arg);
                    else
                        ImGui::Text("%.2lfMB", arg);
                }

                else if constexpr(std::is_same_v<T, DWORD>)
                    ImGui::Text("%d", arg);

                else if constexpr(std::is_same_v<T, const char*>)
                    ImGui::TextUnformatted(arg);
            }, value);
        }

        ImGui::EndTable();
    }
}

void CTMPerformanceCPUScreen::RenderLogicalProcessorHeatmap()
{
    ImPlot::PushColormap(ImPlotColormap_Viridis);

    //Plot the color scale to give an idea of what the colors mean
    ImPlot::ColormapScale("Usage (in %)", 0.0, 100.0, {130, 300});

    //Add a small question mark icon next to the color scale, this is our tooltip
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");

    //Display tooltip when hovering over the question mark
    if(ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("If you see -1.0 in any cell, its a padding cell and not an actual logical processor. Ignore it.");
        ImGui::EndTooltip();
    }

    //We want the heatmap to be in the same line (horizontal to) as color scale / tooltip
    ImGui::SameLine();

    if(ImPlot::BeginPlot("Logical Processors Usage", {-1.0, 300}, ImPlotFlags_NoMenus | ImPlotFlags_NoMouseText | ImPlotFlags_NoLegend))
    {
        //Remove points (the values that are displayed below and on the left of graph / map) on X and Y axis
        ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoTickLabels, ImPlotAxisFlags_NoTickLabels);

        //Finally plot the heatmap hoping it actually displays good stuff
        ImPlot::PlotHeatmap("", perLogicalProcessorUsage.data(), heatmapGridRows, heatmapGridColumns, 0.0, 100.0, "%.2lf");

        //If we hover over the map, get the mouse position, and check which cell it is hovering over.
        //From there we display the core of the cell (like Core 0, Core 1... and so on)
        if(ImPlot::IsPlotHovered())
        {
            ImPlotPoint mousePos = ImPlot::GetPlotMousePos();
            //Map the mousePos ([0, 1]) to each cells X and Y indexes (like 0,1; 2,3)
            int cellsXCoord = (static_cast<int>(heatmapGridRows - (mousePos.y * heatmapGridRows))); //mousePos.y is [1..0] so we invert it [0..1] to get values normally
            int cellsYCoord = (static_cast<int>(mousePos.x * heatmapGridColumns));
            
            //Get the value of the cell, we using row major stuff
            int    index     = cellsXCoord * heatmapGridColumns + cellsYCoord;
            double cellValue = perLogicalProcessorUsage[index];

            //If the cell at that area has a proper usage (not below 0.0), display core number
            if(cellValue > -1e-7) //To account for rounding errors in floating point
            {
                ImGui::BeginTooltip();
                ImGui::Text("Core %d", index);
                ImGui::EndTooltip();
            }
        }
        
        ImPlot::EndPlot();
    }

    ImPlot::PopColormap();
}

//--------------------HELPER FUNCTIONS--------------------
double CTMPerformanceCPUScreen::GetTotalCPUUsage()
{
    //These values represent the total amount of time the system has spent in various states
    FILETIME ftIdleTime, ftKernelTime, ftUserTime;
    if(!GetSystemTimes(&ftIdleTime, &ftKernelTime, &ftUserTime))
        return 0.0f;
    
    //Change FILETIME to a ULARGE_INTEGER (aka 64-bit unsigned int) to make arithmetic easier
    ULARGE_INTEGER currentIdleTime   = reinterpret_cast<ULARGE_INTEGER&>(ftIdleTime),
                   currentKernelTime = reinterpret_cast<ULARGE_INTEGER&>(ftKernelTime),
                   currentUserTime   = reinterpret_cast<ULARGE_INTEGER&>(ftUserTime);
    
    //This represents the total amount of CPU time used in both kernel and user mode (both in previous call and current call)
    ULONGLONG prevTotalTime    = prevKernelTime.QuadPart + prevUserTime.QuadPart,
              currentTotalTime = currentKernelTime.QuadPart + currentUserTime.QuadPart;

    //This difference represents the total CPU time and total Idle time used between the two measurements
    ULONGLONG totalTimeDiff = currentTotalTime - prevTotalTime;
    ULONGLONG idleTimeDiff  = currentIdleTime.QuadPart - prevIdleTime.QuadPart;

    //Save current times for the next call
    prevIdleTime   = currentIdleTime;
    prevKernelTime = currentKernelTime;
    prevUserTime   = currentUserTime;

    //Calculate the CPU usage value in percentage (check if totalTimeDiff isn't 0, as we use it in division)
    return totalTimeDiff ? ((100.0 * (totalTimeDiff - idleTimeDiff)) / totalTimeDiff) : 0.0;
}

void CTMPerformanceCPUScreen::UpdatePerLogicalProcessorInfo()
{
    //Get the per logical processor information first
    //Failed to get the required information
    if(NtQuerySystemInformation(static_cast<SYSTEM_INFORMATION_CLASS>(SystemProcessorPerformanceInformation),
                            currProcessorPerformanceInfo.data(), processorPerformanceInfoSize, nullptr) != STATUS_SUCCESS)
    {
        std::cerr << "Failed to query system information. Did not get processor performance information\n";
        return;
    }
    //Successfully queried the information, now calculate the usage like we did in 'GetTotalCPUUsage' function-
    //-but for each logical processor.
    //Because all containers (for processor performance info) are guaranteed same size, we can simply use .size() on any of the vector-
    //-except for final usage vector!!!!
    for (std::size_t i = 0; i < currProcessorPerformanceInfo.size(); i++)
    {
        auto& currInfoIthIndex = currProcessorPerformanceInfo[i];
        auto& prevInfoIthIndex = prevProcessorPerformanceInfo[i];

        LONGLONG currTotalTime = currInfoIthIndex.KernelTime.QuadPart + currInfoIthIndex.UserTime.QuadPart;
        LONGLONG prevTotalTime = prevInfoIthIndex.KernelTime.QuadPart + prevInfoIthIndex.UserTime.QuadPart;

        LONGLONG totalTimeDiff = currTotalTime - prevTotalTime;
        LONGLONG idleTimeDiff  = currInfoIthIndex.IdleTime.QuadPart - prevInfoIthIndex.IdleTime.QuadPart;

        //Store the final result in 'perLogicalProcessorUsage' vector
        perLogicalProcessorUsage[i] = (totalTimeDiff ? ((100.0 * (totalTimeDiff - idleTimeDiff)) / totalTimeDiff) : 0.0);
    }
    
    //Finally save the current information to previous information.
    prevProcessorPerformanceInfo = currProcessorPerformanceInfo;
}

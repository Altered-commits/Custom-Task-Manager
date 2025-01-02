#include "ctm_perf_cpu_screen.h"

//Hindering with my std::min. I just decided i did not want max as well so yeah
#undef min
#undef max

//Equivalent to OnInit function
CTMPerformanceCPUScreen::CTMPerformanceCPUScreen()
{
    //Try getting 'NtQuerySystemInformation' function from ntdll.dll, if can't, don't let this class initialize
    if(!CTMConstructorInitNTDLL())
        return;

    //Get details like CPU name, base speed, etc from WMI. If can't, still allow initialization
    if(!CTMConstructorQueryWMI())
        CTM_LOG_WARNING("Failed to fully initialize details from WMI, expect 'Failed' values to exist in the table.");

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
        CTM_LOG_ERROR("Failed to get module handle for ntdll.dll");
        return false;
    }

    NtQuerySystemInformation  = reinterpret_cast<NtQuerySystemInformation_t>(
        GetProcAddress(hNtdll, "NtQuerySystemInformation")
    );

    if(!NtQuerySystemInformation)
    {
        CTM_LOG_ERROR("Failed to get 'NtQuerySystemInformation' from ntdll.dll");
        return false;
    }

    return true;
}

bool CTMPerformanceCPUScreen::CTMConstructorQueryWMI()
{
    //Use the namespace in which the ComPtr, CComVariant exists
    //Basically like a unique_ptr but for COM
    using Microsoft::WRL::ComPtr;

    //Some important variables
    HRESULT hres = S_OK;
    ComPtr<IEnumWbemClassObject> pEnumerator;

    //Get the WMI services which can be used to query stuff
    auto pServices = wmiManager.GetServices();
    //Check if its nullptr or not, if it is nullptr, then wmi wasnt initialized properly
    WMI_QUERYING_START_CONDITION(!pServices)
        WMI_QUERYING_FAILED_PURE_ERROR("WMI failed to initialize.")
    WMI_QUERYING_END_CONDITION()

    //It isn't nullptr, query whatever u need
    hres = pServices->ExecQuery(
            bstr_t{L"WQL"},                                        //Query language type. WQL is acronym for WMI Query Language
            bstr_t{L"SELECT Name, Manufacturer, MaxClockSpeed FROM Win32_Processor"}, //Query to get CPU name, Max clock speed and Manufacturer
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, //Check: https://learn.microsoft.com/en-us/windows/win32/api/wbemcli/nf-wbemcli-iwbemservices-execquery#parameters
            NULL,                                                  //Context can be NULL
            pEnumerator.GetAddressOf()                             //Pointer to data in short, just need to enumerate thru the data
        );
    
    //Just in case, cuz i am not using resource guard. So i can't really afford access violations
    WMI_QUERYING_START_CONDITION(FAILED(hres) || pEnumerator == nullptr)
        WMI_QUERYING_FAILED_PURE_ERROR("Failed to query WMI for processor info or Enumerator returned was nullptr.")        
    WMI_QUERYING_END_CONDITION()

    //Now we need to process the query result
    ComPtr<IWbemClassObject> pClassObject;
    ULONG uReturn = 0;

    //Loop through the results though there should only be one result for Win32_Processor
    while(pEnumerator)
    {
        hres = pEnumerator->Next(WBEM_INFINITE, 1, pClassObject.GetAddressOf(), &uReturn);

        //No results were found, break outta loop
        if(uReturn == 0)
            break;
        
        //Be absolutely sure...
        WMI_QUERYING_START_CONDITION(FAILED(hres) || pClassObject == nullptr)
            WMI_QUERYING_FAILED_PURE_ERROR("Failed to go to next entry on WMI enumerator or class object is nullptr.")
        WMI_QUERYING_END_CONDITION()

        //Initialize variants, this is where we will first store our data
        CTMVariant cpuName, vendorName, maxClockSpeed;

        //1) CPU Name
        hres = pClassObject->Get(L"Name", 0, &cpuName, NULL, NULL);
        WMI_QUERYING_START_CONDITION(FAILED(hres) || cpuName.vt != VT_BSTR || !cpuName.bstrVal)
            WMI_QUERYING_FAILED_BUFFER_ERROR(cpuNameBuffer, "Failed to get CPU Name.")
        WMI_QUERYING_END_CONDITION()

        //Succeeded in getting the CPU name, copy it to 'cpuNameBuffer', leave the last byte just in case
        WMI_QUERYING_TRY_WSTOS(cpuNameBuffer, cpuName.bstrVal, "Failed to convert CPU Name to multibyte string.")

        //2) Vendor Name
        hres = pClassObject->Get(L"Manufacturer", 0, &vendorName, NULL, NULL);
        WMI_QUERYING_START_CONDITION(FAILED(hres) || vendorName.vt != VT_BSTR || !vendorName.bstrVal)
            WMI_QUERYING_FAILED_BUFFER_ERROR(vendorNameBuffer, "Failed to get CPU Vendor Name.")
        WMI_QUERYING_END_CONDITION()

        //Succeeded in getting the Vendor name, copy it to 'vendorNameBuffer', leave the last byte just in case
        WMI_QUERYING_TRY_WSTOS(vendorNameBuffer, vendorName.bstrVal, "Failed to convert Vendor Name to multibyte string.")

        //3) Base Speed
        hres = pClassObject->Get(L"MaxClockSpeed", 0, &maxClockSpeed, NULL, NULL);
        WMI_QUERYING_START_CONDITION(FAILED(hres) || maxClockSpeed.vt != VT_I4 || maxClockSpeed.intVal <= 0)
            metricsVector[static_cast<std::size_t>(MetricsVectorIndex::MaxClockSpeed)].second = "Failed";
            WMI_QUERYING_FAILED_PURE_ERROR("Failed to get CPU Max Clock Speed");
        WMI_QUERYING_END_CONDITION()
        
        //Succeeded in getting the Max Clock Speed, copy it to metricsVector and move on
        metricsVector[static_cast<std::size_t>(MetricsVectorIndex::MaxClockSpeed)].second = ((double)maxClockSpeed.intVal / 1000.0);

        //Quite useless to do since its only one iteration, but let it stay here for now
        pClassObject.Reset();
    }

    //Enumerator will be auto released as we using ComPtr
    return true;
}

bool CTMPerformanceCPUScreen::CTMConstructorGetCPUInfo()
{
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
            CTM_LOG_ERROR("Failed to get logical processor information. Error code: ", GetLastError());
            return false;
        }
    }
    //Else it failed due to some other error, simply return false
    else
    {
        CTM_LOG_ERROR("Failed to query logical processor information initially. Error code: ", errorCode);
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
    isHeatmapHeaderExpanded = ImGui::CollapsingHeader("Logical Processors Heatmap");
    if(isHeatmapHeaderExpanded)
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

    //Update per logical processor information if 'isHeatmapHeaderExpanded' is true
    if(isHeatmapHeaderExpanded)
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
                    else if(i == static_cast<std::size_t>(MetricsVectorIndex::MaxClockSpeed))
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
        CTM_LOG_ERROR("Failed to get processor performance information.");
        return;
    }
    //Successfully queried the information, now calculate the usage like we did in 'GetTotalCPUUsage' function-
    //-but for each logical processor.
    //As all containers (except for final usage vector) for processor performance info are guaranteed to be same size-
    //-we can simply use .size() of any vector
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

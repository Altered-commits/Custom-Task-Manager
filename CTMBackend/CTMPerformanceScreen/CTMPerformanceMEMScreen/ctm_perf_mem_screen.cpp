#include "ctm_perf_mem_screen.h"

//Equivalent to OnInit function
CTMPerformanceMEMScreen::CTMPerformanceMEMScreen()
{
    if(!CTMConstructorInitMemoryInfo())
        CTM_LOG_WARNING("Expect improper Memory data (Values maybe 0).");

    if(!CTMConstructorInitPerformanceInfo())
        CTM_LOG_WARNING("Expect improper Memory Paged / Cache data (Values maybe 0).");
    
    if(!CTMConstructorQueryWMI())
        CTM_LOG_WARNING("Expect improper RAM info.");

    SetInitialized(true);
}

//Equivalent to OnClean function
CTMPerformanceMEMScreen::~CTMPerformanceMEMScreen()
{
    SetInitialized(false);
}

//--------------------CONSTRUCTOR FUNCTIONS--------------------
bool CTMPerformanceMEMScreen::CTMConstructorInitMemoryInfo()
{
    //This variable is in the class itself if u didn't notice it yet
    memStatus.dwLength = sizeof(MEMORYSTATUSEX);

    if(!GlobalMemoryStatusEx(&memStatus))
    {
        CTM_LOG_ERROR("Failed to get global memory status. Error code: ", GetLastError());
        return false;
    }

    //The total amount of memory actually installed in the system
    ULONGLONG totalInstalledMemoryInKB = 0;
    if(!GetPhysicallyInstalledSystemMemory(&totalInstalledMemoryInKB))
    {
        CTM_LOG_ERROR("Failed to get total installed memory on system. Error code: ", GetLastError());
        return false;
    }

    //Convert the total installed memory and total OS usable memory into GB
    double totalInstalledMemoryInGB = totalInstalledMemoryInKB / KBToGB;
    double totalOSUsableMemoryInGB  = memStatus.ullTotalPhys   / BToGB;

    //and place it in 'metricsVector'
    metricsVector[static_cast<std::size_t>(MetricsVectorIndex::InstalledMemory)].second = totalInstalledMemoryInGB;
    metricsVector[static_cast<std::size_t>(MetricsVectorIndex::OSUsableMemory)].second  = totalOSUsableMemoryInGB;

    //Calc total hardware reserved memory in MB
    metricsVector[static_cast<std::size_t>(MetricsVectorIndex::HardwareReservedMemory)].second = 
                                                    (totalInstalledMemoryInGB - totalOSUsableMemoryInGB) * 1024.0;

    //Calc the committed memory as well in GB
    totalPageFile = memStatus.ullTotalPageFile / BToGB;
    double committedPageFile = totalPageFile - memStatus.ullAvailPageFile / BToGB;
    metricsVector[static_cast<std::size_t>(MetricsVectorIndex::CommittedMemory)].second = committedPageFile;

    //Calculate the current memory usage at this point in time along with total available memory
    double totalAvailableMemoryInGB = memStatus.ullAvailPhys / BToGB;
    metricsVector[static_cast<std::size_t>(MetricsVectorIndex::MemoryUsage)].second     = totalOSUsableMemoryInGB - totalAvailableMemoryInGB;
    metricsVector[static_cast<std::size_t>(MetricsVectorIndex::AvailableMemory)].second = totalAvailableMemoryInGB;

    //All details done
    return true;
}

bool CTMPerformanceMEMScreen::CTMConstructorInitPerformanceInfo()
{
    //Now is the time for performance info. Variable in class itself
    perfInfo.cb = sizeof(PERFORMANCE_INFORMATION);

    //Used this function cuz all the variables from this function are dynamic
    return UpdateMemoryPerfInfo();
}

bool CTMPerformanceMEMScreen::CTMConstructorQueryWMI()
{
    //Query what you need from WMI
    ComPtr<IEnumWbemClassObject> pEnumeratorPhyMemoryArray = wmiManager.GetEnumeratorFromQuery(
        L"SELECT MemoryDevices FROM Win32_PhysicalMemoryArray"
    );
    ComPtr<IEnumWbemClassObject> pEnumeratorPhyMemory      = wmiManager.GetEnumeratorFromQuery(
        L"SELECT Banklabel, Capacity, Configuredclockspeed, FormFactor, Manufacturer FROM Win32_PhysicalMemory"
    );

    //If pEnumeratorPhyMemory is nullptr, then main query failed. No need to print error as 'GetEnumeratorFromQuery' already does it
    CTM_WMI_START_CONDITION(pEnumeratorPhyMemory == nullptr)
        return false;
    CTM_WMI_END_CONDITION()

    //Now we need to process the query result
    ComPtr<IWbemClassObject> pClassObject;
    ULONG   uReturn = 0;
    HRESULT hres    = S_OK;

    //Just a simple buffer to hold bank label for each RAM stick, any truncation will be handled appropriately
    //How did i decide the size?
    //This: https://learn.microsoft.com/en-us/windows/win32/cimwin32prov/win32-physicalmemory -> BankLabel -> Qualifiers
    // plus
    //Length of "Memory Slot: " string, which turns out to be 14 including null terminator
    constexpr std::size_t bankLabelOffset = sizeof("Memory Slot: ");
    constexpr std::size_t bankLabelSize   = 64 + bankLabelOffset;
    char  bankLabelString[bankLabelSize] = "Memory Slot: ";
    
    //Increment this in the loop using 'pEnumeratorPhyMemory'
    DWORD currentlyUsedSlots = 0;

    //Get the no. of slots available on motherboard, that is, if pEnumeratorPhyMemoryArray exists
    CTM_WMI_START_CONDITION(!pEnumeratorPhyMemoryArray)
        CTM_LOG_ERROR("Failed to get RAM slots, expect 0 value for total slots.");
    CTM_WMI_ELSE_CONDITION()
        hres = pEnumeratorPhyMemoryArray->Next(WBEM_INFINITE, 1, pClassObject.GetAddressOf(), &uReturn);

        CTM_WMI_START_CONDITION(uReturn == 0)
            CTM_LOG_WARNING("No entries were found for RAM slot info. Expect 0 value for total slots.");
        //Be absolutely sure...
        CTM_WMI_ELIF_CONDITION(FAILED(hres) || pClassObject == nullptr)
            CTM_LOG_WARNING("Failed to go to next entry or class object was nullptr. Expect 0 value for total slots.");
        CTM_WMI_ELSE_CONDITION()
            CTMVariant memorySlots;

            hres = pClassObject->Get(L"MemoryDevices", 0, &memorySlots, NULL, NULL);

            CTM_WMI_START_CONDITION(FAILED(hres) || memorySlots.vt != VT_I4 || memorySlots.intVal <= 0)
                CTM_LOG_ERROR("Failed to get total RAM slots value. Expect 0 value for total slots.");
            CTM_WMI_ELSE_CONDITION()
                totalMemorySlots = memorySlots.intVal;
                //Reset the value of class object cuz why not
                pClassObject.Reset();
            CTM_WMI_END_CONDITION()
        CTM_WMI_END_CONDITION()
    CTM_WMI_END_CONDITION()

    //Loop through the results of Win32_PhysicalMemory, multiple RAM sticks maybe present
    while(pEnumeratorPhyMemory)
    {
        hres = pEnumeratorPhyMemory->Next(WBEM_INFINITE, 1, pClassObject.GetAddressOf(), &uReturn);

        //No results were found, break outta loop
        if(uReturn == 0)
            break;
        
        //Be absolutely sure...
        CTM_WMI_START_CONDITION(FAILED(hres) || pClassObject == nullptr)
            CTM_WMI_ERROR_RET("Failed to go to next entry on WMI enumerator or class object was nullptr.")
        CTM_WMI_END_CONDITION()

        //Increment the slots in use
        currentlyUsedSlots++;

        //Initialize variants, this is where we will first store our data
        CTMVariant bankLabel, memoryCapacity, configuredSpeed, formFactor, manufacturer;
        
        //1) BankLabel
        hres = pClassObject->Get(L"BankLabel", 0, &bankLabel, NULL, NULL);
        CTM_WMI_START_CONDITION(FAILED(hres) || bankLabel.vt != VT_BSTR || !bankLabel.bstrVal)
            CTM_WMI_ERROR_RET("Failed to get Bank Label. Cannot get the Memory Slot to display info.")
        CTM_WMI_ELSE_CONDITION()
            CTM_WMI_WSTOS_WITH_ERROR_CONT(wmiManager, &bankLabelString[bankLabelOffset - 1], bankLabelSize - bankLabelOffset,
                                bankLabel.bstrVal, "Failed to copy Bank Label.")
        CTM_WMI_END_CONDITION()

        //Goood, we got the key for the RAM / Memory info map, create an entry in the map first and get its reference
        auto& memoryInfo = memoryInfoMap[bankLabelString];

        //2) Capacity
        hres = pClassObject->Get(L"Capacity", 0, &memoryCapacity, NULL, NULL);
        CTM_WMI_START_CONDITION(FAILED(hres) || memoryCapacity.vt != VT_BSTR  || !memoryCapacity.bstrVal)
            CTM_LOG_ERROR("Failed to get per RAM capacity.");
        CTM_WMI_ELSE_CONDITION()
            //Not bothering error checking rn cuz chances of ULL overflowing is like... bruh
            memoryInfo.capacity = std::wcstoull(memoryCapacity.bstrVal, nullptr, 10) / BToGB;
        CTM_WMI_END_CONDITION()

        //2) ConfiguredClockSpeed
        hres = pClassObject->Get(L"Configuredclockspeed", 0, &configuredSpeed, NULL, NULL);
        CTM_WMI_START_CONDITION(FAILED(hres) || configuredSpeed.vt != VT_I4 || configuredSpeed.intVal <= 0)
            CTM_LOG_ERROR("Failed to get per RAM configured clock speed.");
        CTM_WMI_ELSE_CONDITION()
            memoryInfo.configClockSpeed = configuredSpeed.intVal;
        CTM_WMI_END_CONDITION()

        //2) FormFactor
        hres = pClassObject->Get(L"FormFactor", 0, &formFactor, NULL, NULL);
        CTM_WMI_START_CONDITION(FAILED(hres) || formFactor.vt != VT_I4)
            CTM_LOG_ERROR("Failed to get per RAM form factor.");
        CTM_WMI_ELSE_CONDITION()
            CTMConstructorFormFactorStringify(memoryInfo, formFactor.intVal);
        CTM_WMI_END_CONDITION()

        //2) Manufacturer
        hres = pClassObject->Get(L"Manufacturer", 0, &manufacturer, NULL, NULL);
        CTM_WMI_START_CONDITION(FAILED(hres))
            CTM_WMI_ERROR_RET("Failed to get Stuff.");
        CTM_WMI_ELSE_CONDITION()
            CTM_WMI_WSTOS_WITH_ERROR(wmiManager, memoryInfo.manufacturer, manufacturer.bstrVal, "Failed to get RAM manufacturer.")
        CTM_WMI_END_CONDITION()

        pClassObject.Reset();
    }

    //Write the used slots to metricsVector
    metricsVector[static_cast<std::size_t>(MetricsVectorIndex::MemorySlots)].second = currentlyUsedSlots;

    //Enumerator will be auto released as we using ComPtr
    return true;
}

void CTMPerformanceMEMScreen::CTMConstructorFormFactorStringify(MemoryInfo& memoryInfo, int formFactor)
{
    const char* formFactorStringRepr = nullptr;

    switch(formFactor)
    {
        case 1:  formFactorStringRepr = "Other"; break;
        case 2:  formFactorStringRepr = "SIP"; break;
        case 3:  formFactorStringRepr = "DIP"; break;
        case 4:  formFactorStringRepr = "ZIP"; break;
        case 5:  formFactorStringRepr = "SOJ"; break;
        case 6:  formFactorStringRepr = "Proprietary"; break;
        case 7:  formFactorStringRepr = "SIMM"; break;
        case 8:  formFactorStringRepr = "DIMM"; break;
        case 9:  formFactorStringRepr = "TSOP"; break;
        case 10: formFactorStringRepr = "PGA"; break;
        case 11: formFactorStringRepr = "RIMM"; break;
        case 12: formFactorStringRepr = "SODIMM"; break;
        case 13: formFactorStringRepr = "SRIMM"; break;
        case 14: formFactorStringRepr = "SMD"; break;
        case 15: formFactorStringRepr = "SSMP"; break;
        case 16: formFactorStringRepr = "QFP"; break;
        case 17: formFactorStringRepr = "TQFP"; break;
        case 18: formFactorStringRepr = "SOIC"; break;
        case 19: formFactorStringRepr = "LCC"; break;
        case 20: formFactorStringRepr = "PLCC"; break;
        case 21: formFactorStringRepr = "BGA"; break;
        case 22: formFactorStringRepr = "FPBGA"; break;
        case 23: formFactorStringRepr = "LGA"; break;
        case 0:
        default:
            formFactorStringRepr = "Unknown Form Factor";
            break;
    }

    memoryInfo.formFactor = formFactorStringRepr;
}

//--------------------MAIN RENDER AND UPDATE FUNCTIONS--------------------
void CTMPerformanceMEMScreen::OnRender()
{
    ImVec2 windowSize              = ImGui::GetWindowSize();
    double totalOSUsableMemoryInGB = std::get<double>(metricsVector[static_cast<std::size_t>(MetricsVectorIndex::OSUsableMemory)].second);

    //Plotting memory usage, this result is pretty much the same as that of Task Manager
    ImGui::Text("%.2lfGB", totalOSUsableMemoryInGB);
    PlotUsageGraph("Memory in use (Over 60 Seconds)", {-1, 300}, 0.0f, totalOSUsableMemoryInGB, { 0.588f, 0.463f, 0.929f, 1.0f });
    ImGui::TextUnformatted("0GB");

    //Give some spacing vertically before displaying CPU Info
    ImGui::Dummy({-1.0f, 15.0f});

    //Add some padding to the frame (see the blank space around the text of the collapsable header)
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 10.0f));

    //Statistics
    isStatisticsHeaderExpanded = ImGui::CollapsingHeader("Memory Statistics");
    if(isStatisticsHeaderExpanded)
        RenderMemoryStatistics();

    ImGui::PopStyleVar();
}

void CTMPerformanceMEMScreen::OnUpdate()
{
    UpdateXAxis();
    double totalMemoryInUseInGB = UpdateMemoryStatus();
    PlotPoint(GetCurrentXAxisValue(), totalMemoryInUseInGB);

    //If the statistics header is collapsed, then only update the paged / cached memory data
    if(isStatisticsHeaderExpanded)
        (void)UpdateMemoryPerfInfo(); //No need for return value
}

//--------------------RENDER FUNCTIONS--------------------
void CTMPerformanceMEMScreen::RenderMemoryStatistics()
{
    if(ImGui::BeginTable("MEMStatisticsTable", 2, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV))
    {
        ImGui::TableSetupColumn("Metrics");
        ImGui::TableSetupColumn("Values");
        ImGui::TableHeadersRow();

        //Basic Memory Info
        for(std::size_t i = 0; i < metricsVector.size(); i++)
        {
            auto&&[metric, value] = metricsVector[i];

            ImGui::TableNextRow();
            //Metrics column
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(metric);
            //Value column
            ImGui::TableSetColumnIndex(1);
            std::visit([&i, this](auto&& arg)
            {
                using T = std::decay_t<decltype(arg)>;

                //Display GB at the end of every value except Hardware Reserved, its in MB. Also committed memory is displayed differently
                if constexpr(std::is_same_v<T, double>)
                {
                    if(i == static_cast<std::size_t>(MetricsVectorIndex::CommittedMemory))
                        ImGui::Text("%.2lf out of %.2lfGB", arg, totalPageFile);
                    else if(i != static_cast<std::size_t>(MetricsVectorIndex::HardwareReservedMemory))
                        ImGui::Text("%.2lfGB", arg);
                    else
                        ImGui::Text("%.2lfMB", arg);
                }
                //For slot usage
                else if constexpr(std::is_same_v<T, DWORD>)
                    ImGui::Text("%d used out of %d", arg, totalMemorySlots);
                else if constexpr(std::is_same_v<T, const char*>)
                    ImGui::TextUnformatted(arg);
            }, value);
        }

        //Any specific 'RAM stick' detail is showed below
        for(auto &&[key, info] : memoryInfoMap)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if(ImGui::TreeNodeEx(key.c_str(), ImGuiTreeNodeFlags_SpanAllColumns))
            {
                RenderPerRAMInfo(info);
                ImGui::TreePop();
            }
        }
        
        ImGui::EndTable();
    }
}

void CTMPerformanceMEMScreen::RenderPerRAMInfo(MemoryInfo& memoryInfo)
{
    //Honestly speaking, i could very easily make this use some sort of array but rn...
    //I feel like this is enough
    //For your understanding:
    //0th index -> Metric
    //1th index -> Value

    ImGui::Indent();
    
    ImGui::TableNextRow();
    
    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted("Capacity");
    ImGui::TableSetColumnIndex(1);
    ImGui::Text("%.2lfGB", memoryInfo.capacity);

    ImGui::TableNextRow();

    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted("Clock Speed");
    ImGui::TableSetColumnIndex(1);
    ImGui::Text("%dMhz", memoryInfo.configClockSpeed);

    ImGui::TableNextRow();

    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted("RAM Type");
    ImGui::TableSetColumnIndex(1);
    ImGui::Text("%s", memoryInfo.formFactor);

    ImGui::TableNextRow();

    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted("Manufacturer");
    ImGui::TableSetColumnIndex(1);
    ImGui::Text("%s", memoryInfo.manufacturer);

    ImGui::Unindent();
}

//--------------------UPDATE FUNCTIONS--------------------
double CTMPerformanceMEMScreen::UpdateMemoryStatus()
{
    //This is initialized so no harm in getting this, no error would be thrown
    double totalOSUsableMemoryInGB = std::get<double>(metricsVector[static_cast<std::size_t>(MetricsVectorIndex::OSUsableMemory)].second);
    
    if(!GlobalMemoryStatusEx(&memStatus))
    {
        CTM_LOG_ERROR("Failed to get global memory status for updation of statistics. Error code: ", GetLastError());
        return 0.0;
    }

    //Recalc the currently used memory and available memory
    double totalAvailableMemoryInGB = memStatus.ullAvailPhys / BToGB;
    double totalMemoryInUseInGB     = totalOSUsableMemoryInGB - totalAvailableMemoryInGB;

    metricsVector[static_cast<std::size_t>(MetricsVectorIndex::MemoryUsage)].second     = totalMemoryInUseInGB;
    metricsVector[static_cast<std::size_t>(MetricsVectorIndex::AvailableMemory)].second = totalAvailableMemoryInGB;

    //Recalc the committed memory. Chances are, 'memStatus.ullTotalPageFile' may change due to some system settings
    totalPageFile = memStatus.ullTotalPageFile / BToGB;
    double committedPageFile = totalPageFile - memStatus.ullAvailPageFile / BToGB;
    
    metricsVector[static_cast<std::size_t>(MetricsVectorIndex::CommittedMemory)].second = committedPageFile;

    //Might as well return the memory in use
    return totalMemoryInUseInGB;
}

bool CTMPerformanceMEMScreen::UpdateMemoryPerfInfo()
{
    if(!GetPerformanceInfo(&perfInfo, sizeof(perfInfo)))
    {
        CTM_LOG_ERROR("Failed to get performance information. Error code: ", GetLastError());
        return false;
    }

    //1) Cached data in GB
    double cachedData = (perfInfo.SystemCache * perfInfo.PageSize) / BToGB;
    metricsVector[static_cast<std::size_t>(MetricsVectorIndex::CachedData)].second = cachedData;

    //2) Paged pool in GB
    double pagedPool = (perfInfo.KernelPaged * perfInfo.PageSize) / BToGB;
    metricsVector[static_cast<std::size_t>(MetricsVectorIndex::PagedPool)].second = pagedPool;
    
    //3) Non-Paged pool in GB
    double nonPagedPool = (perfInfo.KernelNonpaged * perfInfo.PageSize) / BToGB;
    metricsVector[static_cast<std::size_t>(MetricsVectorIndex::NonPagedPool)].second = nonPagedPool;

    return true;
}

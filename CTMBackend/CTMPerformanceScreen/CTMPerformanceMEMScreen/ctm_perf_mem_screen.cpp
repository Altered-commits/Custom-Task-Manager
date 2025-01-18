#include "ctm_perf_mem_screen.h"

//Hindering with my std::max
#undef max

//Equivalent to OnInit function
CTMPerformanceMEMScreen::CTMPerformanceMEMScreen()
{
    //I won't let this screen initialize when we are not able to get basic stuff
    if(!CTMConstructorInitPDH())
    {
        CTM_LOG_ERROR("Failed to initialize Performance Data Helper. Not initializing Memory Screen.");
        return;
    }

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
    CTMDestructorCleanupPDH();
    SetInitialized(false);
}

//--------------------CONSTRUCTOR AND DESTRUCTOR FUNCTIONS--------------------
bool CTMPerformanceMEMScreen::CTMConstructorInitPDH()
{
    //Open the pdh query stuff idk
    PDH_STATUS status = PdhOpenQueryW(NULL, 0, &hQuery);
    if(status != ERROR_SUCCESS)
    {
        CTM_LOG_ERROR("Failed to open PDH query. Error code: ", status);
        return false;
    }

    //While we are at it, register a resource guard to cleanup PDH query incase of a disaster
    resourceGuard.RegisterCleanupFunction(pdhCleanupFunctionName, [this](){
        PdhCloseQuery(hQuery);
    });

    //Add a counter to get modified page list bytes, just like task manager
    status = PdhAddEnglishCounterW(hQuery, L"\\Memory\\Modified Page List Bytes", 0, &hModifiedMemoryCounter);
    if(status != ERROR_SUCCESS)
    {
        CTM_LOG_ERROR("Failed to add counter for modified page list. Error code: ", status);
        //Close it as we can't even add a counter. Also unregister the resource guard
        PdhCloseQuery(hQuery);
        resourceGuard.UnregisterCleanupFunction(pdhCleanupFunctionName);
        return false;
    }

    //Query it once as we will re-query it after every one second updation, giving us the good values
    //Also if we can't even query it initially, close the query and return false
    status = PdhCollectQueryData(hQuery);
    if(status != ERROR_SUCCESS)
    {
        CTM_LOG_ERROR("Failed to collect data from PDH initially. Error code: ", status);
        //Close it as we can't get any data anyways. Also unregister the resource guard
        PdhCloseQuery(hQuery);
        resourceGuard.UnregisterCleanupFunction(pdhCleanupFunctionName);
        return false;
    }

    //PDH is initialized and can be queried
    return true;
}

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
    double totalInstalledMemoryInGB = CTM_KB_TO_GB(totalInstalledMemoryInKB);
    double totalOSUsableMemoryInGB  = CTM_BYTES_TO_GB(memStatus.ullTotalPhys);

    //and place it in 'metricsVector'
    metricsVector[static_cast<std::size_t>(MetricsVectorIndex::InstalledMemory)].second = totalInstalledMemoryInGB;
    metricsVector[static_cast<std::size_t>(MetricsVectorIndex::OSUsableMemory)].second  = totalOSUsableMemoryInGB;

    //Calc total hardware reserved memory in MB
    metricsVector[static_cast<std::size_t>(MetricsVectorIndex::HardwareReservedMemory)].second = 
                                                    (totalInstalledMemoryInGB - totalOSUsableMemoryInGB) * 1024.0;

    //Calc the committed memory as well in GB
    totalPageFile = CTM_BYTES_TO_GB(memStatus.ullTotalPageFile);
    double committedPageFile = totalPageFile - CTM_BYTES_TO_GB(memStatus.ullAvailPageFile);
    metricsVector[static_cast<std::size_t>(MetricsVectorIndex::CommittedMemory)].second = committedPageFile;

    //Calculate the current memory usage at this point in time along with total available memory
    double totalAvailableMemoryInGB = CTM_BYTES_TO_GB(memStatus.ullAvailPhys);
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
    bool functionSucceeded = UpdateMemoryPerfInfo();

    //Also before we return, write the page size to variable 'totalPageSize' only in the case where function succeeded
    if(functionSucceeded)
        totalPageSize = perfInfo.PageSize;
    
    return functionSucceeded;
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
            //Not bothering error checking rn cuz chances of ULL overflowing is like... bruh. Its a big ass value
            memoryInfo.capacity = CTM_BYTES_TO_GB(std::wcstoull(memoryCapacity.bstrVal, nullptr, 10));
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

void CTMPerformanceMEMScreen::CTMDestructorCleanupPDH()
{
    //Close the PDH query and unregister resource guard as its no longer needed
    PdhCloseQuery(hQuery);
    resourceGuard.UnregisterCleanupFunction(pdhCleanupFunctionName);
}

//--------------------MAIN RENDER AND UPDATE FUNCTIONS--------------------
void CTMPerformanceMEMScreen::OnRender()
{
    double totalOSUsableMemoryInGB = std::get<double>(metricsVector[static_cast<std::size_t>(MetricsVectorIndex::OSUsableMemory)].second);

    //Plotting memory usage, this result is pretty much the same as that of Task Manager
    ImGui::Text("%.2lfGB", totalOSUsableMemoryInGB);
    PlotUsageGraph("Memory in use (Over 60 Seconds)", {-1.0f, 300.0f}, 0.0, totalOSUsableMemoryInGB, { 0.588f, 0.463f, 0.929f, 1.0f });
    ImGui::TextUnformatted("0GB");

    //Give some spacing vertically before displaying Memory Info
    ImGui::Dummy({-1.0f, 15.0f});

    //Add some padding to the frame (see the blank space around the text of the collapsable header)
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 10.0f));

    isMemoryCompositionExpanded = ImGui::CollapsingHeader("Memory Composition");
    if(isMemoryCompositionExpanded)
        RenderMemoryComposition();

    //Even more spacing vertically
    ImGui::Dummy({-1.0f, 5.0f});

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
    PlotYAxis(totalMemoryInUseInGB);

    //If the statistics header or memory composition header is collapsed, then only update the paged and cached memory data
    //Cuz the memory composition also uses some data from this function
    if(isStatisticsHeaderExpanded || isMemoryCompositionExpanded)
        (void)UpdateMemoryPerfInfo(); //No need for return value
    
    //Rest of the memory composition stuff not handled by the 'UpdateMemoryPerfInfo'
    if(isMemoryCompositionExpanded)
        UpdateMemoryCompositionInfo();
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
    ImGui::Text("%dMHz", memoryInfo.configClockSpeed);

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

void CTMPerformanceMEMScreen::RenderMemoryComposition()
{
    //Total available region. The composition is going to span the whole width of window
    ImVec2 windowSize = ImGui::GetContentRegionAvail();

    //Having to write out how to check memory usage for each block cuz people might be confused (aka dumb ;))
    ImGui::TextUnformatted("Tip: Hover over the bar to check Memory Composition for each block.");

    //Get memory info available in metricsVector. The only difference is, we will convert everything to MB if not in MB already
    double memoryHardwareReserved = std::get<double>(metricsVector[static_cast<std::size_t>(MetricsVectorIndex::HardwareReservedMemory)].second),
           memoryInUse     = CTM_GB_TO_MB(std::get<double>(metricsVector[static_cast<std::size_t>(MetricsVectorIndex::MemoryUsage)].second)),
           memoryStandby   = CTM_GB_TO_MB(std::get<double>(metricsVector[static_cast<std::size_t>(MetricsVectorIndex::CachedData)].second)),
           memoryTotal     = CTM_GB_TO_MB(std::get<double>(metricsVector[static_cast<std::size_t>(MetricsVectorIndex::InstalledMemory)].second)),
           memoryAddedUp   = (memoryInUse + memoryStandby + memoryModified + memoryHardwareReserved),
           //If the memory goes negative, no free memory exists. Clamp its value at 0.0
           memoryRemaining = std::max((memoryTotal - memoryAddedUp), 0.0);

    //Needed this approach so i could do a loop over each value and stuff, instead of writing code a billion times for each value
    std::pair<double, const char*> memoryValues[] = {
        std::make_pair(memoryHardwareReserved, "Reserved Memory:\nMemory set aside for hardware devices and other system functions."),
        std::make_pair(memoryInUse, "In Use Memory:\nMemory actively used by the system, including running applications and services."),
        std::make_pair(memoryModified, "Modified Memory:\nMemory that can be reused after flushing its contents to disk."),
        std::make_pair(memoryStandby, "Standby Memory:\nMemory that holds cached data and code, ready to be used when needed."),
        std::make_pair(memoryRemaining, "Remaining Memory:\nUnused memory, available for allocation or future processes.")
    };

    //Some cool constants useful
    constexpr float        barHeight        = 80.0f;
    constexpr std::uint8_t memoryValuesSize = sizeof(memoryValues) / sizeof(memoryValues[0]);

    //Initialize cursorPos and drawList (this is what will be used to draw the graph)
    ImVec2      cursorPos     = ImGui::GetCursorScreenPos(),
                prevCursorPos = cursorPos;
    ImDrawList* drawList      = ImGui::GetWindowDrawList();

    //If the added up memory value somehow exceeds total memory due to my bad coding, normalize the values to make them fit in bar
    double normalizationFactor = (memoryAddedUp > memoryTotal) ? (memoryTotal / memoryAddedUp) : 1.0;

    for(std::uint8_t i = 0; i < memoryValuesSize; i++)
    {
        auto&&[value, info] = memoryValues[i];
        
        //Adjust the value based on the normalization factor
        double adjustedValue = value * normalizationFactor;

        //Calculate the width we need to plot for a specific block
        double memoryValueWidth = ((adjustedValue / memoryTotal) * ((double)windowSize.x));

        //For the last 2 blocks, just like task manager, display only border
        //Also not bothering changing any colors rn. Maybe in future
        if(i >= (memoryValuesSize - 2))
            RenderMemoryCompositionBarBorder(drawList, cursorPos, memoryValueWidth, barHeight, IM_COL32(145, 114, 232, 200), 1.0f);
        else
            RenderMemoryCompositionBarFilled(drawList, cursorPos, memoryValueWidth, barHeight,
                                            IM_COL32(145, 114, 232, 150), IM_COL32(145, 114, 232, 200), 1.0f);

        //Check if we hovered over a specific block of memory. If we are, display the info of that block
        if(ImGui::IsMouseHoveringRect(prevCursorPos, {prevCursorPos.x + (float)memoryValueWidth, prevCursorPos.y + barHeight}))
            ImGui::SetTooltip("%s\n\nUsage: %.2lfMB", info, value);
        
        //'cursorPos' is directly manipulated by 'RenderMemoryCompositionBarFilled'. Hence we need to keep track of previous position for hover check
        prevCursorPos = cursorPos;
    }

    //Drawing stuff via DrawList doesn't affect the space it takes. Hence we need to add a dummy node to create some space for it-
    //-so it doesn't overlap with other items
    //Also the -7.0f is just some random constant to adjust height so it doesn't add too much space at the bottom
    ImGui::Dummy({-1, barHeight - 7.0f});
}

void CTMPerformanceMEMScreen::RenderMemoryCompositionBarFilled(ImDrawList* drawList, ImVec2& cursorPos, float usageWidth,
                                float barHeight, ImU32 fillColor, ImU32 borderColor, float borderThickness)
{
    drawList->AddRectFilled(cursorPos, {cursorPos.x + usageWidth, cursorPos.y + barHeight}, fillColor);
    drawList->AddRect(cursorPos, {cursorPos.x + usageWidth, cursorPos.y + barHeight}, borderColor, 0.0f, 0, borderThickness);
    cursorPos.x += usageWidth;
}

void CTMPerformanceMEMScreen::RenderMemoryCompositionBarBorder(ImDrawList* drawList, ImVec2& cursorPos, float usageWidth,
                                float barHeight, ImU32 borderColor, float borderThickness)
{
    drawList->AddRect(cursorPos, {cursorPos.x + usageWidth, cursorPos.y + barHeight}, borderColor, 0.0f, 0, borderThickness);
    cursorPos.x += usageWidth;
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
    double totalAvailableMemoryInGB = CTM_BYTES_TO_GB(memStatus.ullAvailPhys);
    double totalMemoryInUseInGB     = totalOSUsableMemoryInGB - totalAvailableMemoryInGB;

    metricsVector[static_cast<std::size_t>(MetricsVectorIndex::MemoryUsage)].second     = totalMemoryInUseInGB;
    metricsVector[static_cast<std::size_t>(MetricsVectorIndex::AvailableMemory)].second = totalAvailableMemoryInGB;

    //Recalc the committed memory. Chances are, 'memStatus.ullTotalPageFile' may change due to some system settings
    totalPageFile = CTM_BYTES_TO_GB(memStatus.ullTotalPageFile);
    double committedPageFile = totalPageFile - CTM_BYTES_TO_GB(memStatus.ullAvailPageFile);
    
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
    double cachedData = CTM_BYTES_TO_GB(perfInfo.SystemCache * perfInfo.PageSize);
    metricsVector[static_cast<std::size_t>(MetricsVectorIndex::CachedData)].second = cachedData;

    //2) Paged pool in GB
    double pagedPool = CTM_BYTES_TO_GB(perfInfo.KernelPaged * perfInfo.PageSize);
    metricsVector[static_cast<std::size_t>(MetricsVectorIndex::PagedPool)].second = pagedPool;
    
    //3) Non-Paged pool in GB
    double nonPagedPool = CTM_BYTES_TO_GB(perfInfo.KernelNonpaged * perfInfo.PageSize);
    metricsVector[static_cast<std::size_t>(MetricsVectorIndex::NonPagedPool)].second = nonPagedPool;

    return true;
}

void CTMPerformanceMEMScreen::UpdateMemoryCompositionInfo()
{
    //Query the PDH again, this time in the hopes of collecting value
    //Ofc if you can't then just display an error
    PDH_STATUS status = PdhCollectQueryData(hQuery);
    if(status != ERROR_SUCCESS)
    {
        CTM_LOG_ERROR("Failed to collect data from PDH. Error code: ", status);
        return;
    }
    //Good... we queried the data. Now get the actual value as a 64bit integer. We will convert it to 'double' later anyways
    PDH_FMT_COUNTERVALUE modifiedMemoryValue;
    
    status = PdhGetFormattedCounterValue(hModifiedMemoryCounter, PDH_FMT_LARGE, nullptr, &modifiedMemoryValue);
    if(status != ERROR_SUCCESS)
    {
        CTM_LOG_ERROR("Failed to format the value queried from PDH. Error code: ", status);
        return;
    }

    //OK GOOD. Everything works. Convert the value to MB (currently its in bytes)
    memoryModified = CTM_BYTES_TO_MB(modifiedMemoryValue.largeValue);
}

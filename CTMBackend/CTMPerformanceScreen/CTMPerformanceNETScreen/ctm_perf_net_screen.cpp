#include "ctm_perf_net_screen.h"

//Equivalent to OnInit function
CTMPerformanceNETScreen::CTMPerformanceNETScreen()
{
    //Try initializing PDH. If we can't then no point in even initializing further for now
    if(!CTMConstructorInitPDH())
        return;
    
    SetInitialized(true);
}

//Equivalent to OnClean function
CTMPerformanceNETScreen::~CTMPerformanceNETScreen()
{
    CTMDestructorCleanupPDH();
    SetInitialized(false);
}

//--------------------MAIN RENDER AND UPDATE FUNCTIONS--------------------
void CTMPerformanceNETScreen::OnRender()
{
    //Decode the max y limit into value and type
    float decodedMaxLimit = 0.0f; std::uint8_t decodedType = 0;
    DecodeNetworkDataWithType(displayMaxYLimit, decodedType, decodedMaxLimit);

    //1) Graph
    ImGui::Text("%.2f %s", decodedMaxLimit, dataUnits[decodedType]);
    PlotMultiUsageGraph("Network Usage (Over 60 sec)", "Sent Bytes", "Recieved Bytes", 0, GetYAxisMaxValue(), {-1, 300}, graphColors);
    ImGui::TextUnformatted("0 KB");

    //Give some spacing vertically before displaying Network Info
    ImGui::Dummy({-1.0f, 15.0f});

    //Add some padding to the frame (see the blank space around the text of the collapsable header)
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 10.0f));

    //2) Network table with data
    isStatisticsHeaderExpanded = ImGui::CollapsingHeader("Network Statistics");
    if(isStatisticsHeaderExpanded)
        RenderNetworkStatistics();
    
    ImGui::PopStyleVar();
}

void CTMPerformanceNETScreen::OnUpdate()
{
    UpdateNetworkUsage();
    UpdateXAxis();
    
    //The base values itself are going to be represented in KB
    PlotYAxisAtIndex(static_cast<std::size_t>(CTMNetworkTypeIndex::NetworkSent), totalSentBytesKB);
    PlotYAxisAtIndex(static_cast<std::size_t>(CTMNetworkTypeIndex::NetworkRecieved), totalRecBytesKB);
    
    //Also since we need dynamically changing y-axis limits, update y-axis at the backend side to the graphs max limit
    UpdateYAxisToMaxValue();

    //Get the max y-axis limit and just convert it to some good readable unit
    displayMaxYLimit = EncodeNetworkDataWithType(GetYAxisMaxValue());
}

//--------------------CONSTRUCTOR AND DESTRUCTOR FUNCTIONS--------------------
bool CTMPerformanceNETScreen::CTMConstructorInitPDH()
{
    /* 
     * Using PDH cuz no need for ETW events.
     * I don't need per process statistics so yeah, no need for that complicated ahh stuff
     */
    PDH_STATUS status = PdhOpenQueryA(nullptr, 0, &hQuery);
    if(status != ERROR_SUCCESS)
    {
        CTM_LOG_ERROR("Failed to open query for Network Page. Error code: ", status);
        return false;
    }

    //Initialize resource guard incase of a crash (for PDH query)
    resourceGuard.RegisterCleanupFunction(pdhCleanupFunctionName, [this](){
        PdhCloseQuery(hQuery);
    });

    //Helper array to make life cleaner
    //Pair of query path and ref to counter
    std::pair<const char*, PDH_HCOUNTER*> pdhQueryCounters[] = {
        std::make_pair("\\Network Interface(*)\\Bytes Sent/sec"    , &hNetworkSent),
        std::make_pair("\\Network Interface(*)\\Bytes Received/sec", &hNetworkRecieved)
    };

    //Add stuff for both TCP and UDP, IPv4 and IPv6.
    for(auto &&[path, counter] : pdhQueryCounters)
    {
        status = PdhAddEnglishCounterA(hQuery, path, 0, counter);
        if(status != ERROR_SUCCESS)
        {
            CTM_LOG_ERROR("Failed to add counter for query path: ", path, ". Error code: ", status);
            //Close query and unregister resource guard cuz no longer needed ofc
            PdhCloseQuery(hQuery);
            resourceGuard.UnregisterCleanupFunction(pdhCleanupFunctionName);
            return false;
        }
    }
    
    //Before we end this function, query it once rn. Later we do it in update loop per second
    status = PdhCollectQueryData(hQuery);
    if(status != ERROR_SUCCESS)
    {
        CTM_LOG_ERROR("Failed to collect data for Network Usage initially. Error code: ", status);
        return false;
    }
    
    //After all that BS, finally return true. I should've just used ETW but who cares now amirite
    return true;
}

void CTMPerformanceNETScreen::CTMDestructorCleanupPDH()
{
    //Unregister the cleanup function as well, no point in keeping it alive
    PdhCloseQuery(hQuery);
    resourceGuard.UnregisterCleanupFunction(pdhCleanupFunctionName);
}

//--------------------RENDER FUNCTIONS--------------------
void CTMPerformanceNETScreen::RenderNetworkStatistics()
{
    if(ImGui::BeginTable("NETStatisticsTable", 2, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV))
    {
        ImGui::TableSetupColumn("Metrics");
        ImGui::TableSetupColumn("Values");
        ImGui::TableHeadersRow();

        //Basic Network Info
        for(std::size_t i = 0; i < metricsVector.size(); i++)
        {
            auto&&[metric, value] = metricsVector[i];

            ImGui::TableNextRow();
            //Metrics column
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(metric);
            //Value column
            ImGui::TableSetColumnIndex(1);
            std::visit([this](auto&& arg)
            {
                using T = std::decay_t<decltype(arg)>;
            
                //Network usage, 3 LSB will represent its type (KB, MB...) and rest represent the actual value
                if constexpr(std::is_same_v<T, float>)
                {
                    float decodedNetworkData = 0.0f; std::uint8_t decodedType = 0;

                    DecodeNetworkDataWithType(arg, decodedType, decodedNetworkData);
                    ImGui::Text("%.2f %s", decodedNetworkData, dataUnits[decodedType]);
                }

            }, value);
        }

        ImGui::EndTable();
    }
}

//--------------------UPDATE FUNCTIONS--------------------
void CTMPerformanceNETScreen::UpdateNetworkUsage()
{
    //FIRST: Collect the data
    PDH_STATUS status = PdhCollectQueryData(hQuery);
    if(status != ERROR_SUCCESS)
    {
        CTM_LOG_ERROR("Failed to collect data for Network Usage. Error code: ", status);
        return;
    }

    //SECOND: Get the data from all the counters and throw it straight in metricsVector
    //IMP: There maybe some data loss due to encoding, but its negligible in most cases so yeah

    //1) Recieved Bytes (Dynamically converted to some unit like KB, MB... and so on)
    totalRecBytesKB = GetPdhFormattedNetworkData(hNetworkRecieved);
    metricsVector[static_cast<std::size_t>(MetricsVectorIndex::RecievedData)].second = EncodeNetworkDataWithType(totalRecBytesKB);

    //2) Sent Bytes (Dynamically converted to some unit like KB, MB... and so on)
    totalSentBytesKB = GetPdhFormattedNetworkData(hNetworkSent);
    metricsVector[static_cast<std::size_t>(MetricsVectorIndex::SentData)].second = EncodeNetworkDataWithType(totalSentBytesKB);
    
    //Uhh yeah, what else am i supposed to do. We are done
}

//--------------------HELPER FUNCTIONS--------------------
double CTMPerformanceNETScreen::GetPdhFormattedNetworkData(PDH_HCOUNTER hNetworkCounter)
{
    PDH_STATUS status;
    //Chances are, we may not be having big enough buffer value. Check for that and resize accordingly
    do
    {
        status = PdhGetFormattedCounterArrayA(hNetworkCounter, PDH_FMT_LARGE, &networkInfoBufferSize, &networkInfoItemCount,
                                    reinterpret_cast<PPDH_FMT_COUNTERVALUE_ITEM_A>(networkInfoBuffer.data()));

        if(status == PDH_MORE_DATA)
            networkInfoBuffer.resize(networkInfoBufferSize);
    }
    while(status == PDH_MORE_DATA);

    //Check if it failed due to some other error or not
    if(status != ERROR_SUCCESS)
    {
        CTM_LOG_ERROR("Failed to get array data for network counter. Error code: ", status);
        return 0;
    }

    //Cast it to PPDH_FMT_COUNTERVALUE_ITEM_A and start using the data in array
    PPDH_FMT_COUNTERVALUE_ITEM_A networkDataArray = reinterpret_cast<PPDH_FMT_COUNTERVALUE_ITEM_A>(networkInfoBuffer.data());

    //Finally add the value of N entries to the retTotalBytes variable
    std::uint64_t retTotalBytes = 0;
    for(std::uint8_t i = 0; i < networkInfoItemCount; i++)
        retTotalBytes += networkDataArray[i].FmtValue.largeValue;
    
    return (retTotalBytes / 1024.0);
}

float CTMPerformanceNETScreen::EncodeNetworkDataWithType(double inData)
{
    /*
     * The converted value will range somewhere from 0 to 1023 until it gets converted again to higher type.
     * So it won't take that many bits in practice
     */
    //Base case
    if(inData <= 0)
        return 0;

    //End deduced data type
    std::uint8_t calcUnitIndex = 0;

    //Scale down the size using simple division. This outperforms log + pow implementation, atleast on my system
    while(inData >= 1024.0 && calcUnitIndex < dataUnitsSize - 1)
    {
        inData /= 1024.0;
        ++calcUnitIndex;
    }

    //Now that the value has been converted, the value can be casted to float without much data loss
    CTMFloatView fv;
    fv.floatView = static_cast<float>(inData);

    //Set the 3 bits in mantissa (furthest from exponent) to contain its data type (KB, MB... and so on)
    fv.bitView = (fv.bitView & ~0x7) | calcUnitIndex;

    return fv.floatView;
}

void CTMPerformanceNETScreen::DecodeNetworkDataWithType(float inData, std::uint8_t& outType, float& outData)
{
    //Create a float view of data
    CTMFloatView fv;
    fv.floatView = inData;
    
    //Extract the 3 least significant bits of mantissa
    outType = (fv.bitView & 0x7);

    //Clear the last three bits and retrive the data
    fv.bitView &= ~0x7;
    outData = fv.floatView;
}

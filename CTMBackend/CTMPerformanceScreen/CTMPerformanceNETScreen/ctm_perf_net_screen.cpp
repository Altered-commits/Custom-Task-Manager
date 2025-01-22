#include "ctm_perf_net_screen.h"

//Equivalent to OnInit function
CTMPerformanceNETScreen::CTMPerformanceNETScreen()
{
    //Try initializing PDH. If we can't then no point in even initializing further for now
    if(!CTMConstructorInitPDH())
        return;
    
    if(!CTMConstructorInitNetworkAdapterInfo())
        CTM_LOG_WARNING("Expect no information for network adapters on your system.");
    
    if(!CTMConstructorInitWLANInfo())
    {
        CTM_LOG_WARNING("Expect no information on which network the system is currently connected to.");
        CTM_LOG_INFO("TIP: If you see Error code as 5, chances are, you may not have given location permission to this app."
                    " Querying for network info required location permission.");
    }
    
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

bool CTMPerformanceNETScreen::CTMConstructorInitNetworkAdapterInfo()
{
    std::unique_ptr<BYTE[]> adapterInfoBuffer;
    ULONG adapterInfoBufferSize = 0;
    //First call, get the size of the buffer we need to store the data
    //Few stuff for ur info:
    //AF_UNSPEC:               Return both IPv4 and IPv6 addresses associated with adapters with IPv4 or IPv6 enabled.
    //GAA_FLAG_INCLUDE_PREFIX: When this flag is set, IP address prefixes are returned for both IPv6 and IPv4 addresses.
    ULONG status = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, nullptr, nullptr, &adapterInfoBufferSize);
    if(status != ERROR_BUFFER_OVERFLOW)
    {
        CTM_LOG_ERROR("Failed to query adapter information initially. Error code: ", status);
        return false;
    }

    //Great, we got the buffer size, now alloc enough memory for information
    adapterInfoBuffer = std::make_unique<BYTE[]>(adapterInfoBufferSize);

    status = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, nullptr,
                        reinterpret_cast<PIP_ADAPTER_ADDRESSES>(adapterInfoBuffer.get()), &adapterInfoBufferSize);
    if(status != ERROR_SUCCESS)
    {
        CTM_LOG_ERROR("Failed to query adapter information. Error code: ", status);
        return false;
    }

    //Cast it to PIP_ADAPTER_ADDRESSES-
    PIP_ADAPTER_ADDRESSES adapterInfoPtr = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(adapterInfoBuffer.get());
    //-and loop over all the adapters present in the system. Its pretty much linked list
    while(adapterInfoPtr)
    {
        CTMConstructorAdapterInfoHelper(adapterInfoPtr);
        //Go to the next entry if it exists
        adapterInfoPtr = adapterInfoPtr->Next;
    }

    //We are done, return true. adapterInfoBuffer RAII released
    return true;
}

void CTMPerformanceNETScreen::CTMConstructorAdapterInfoHelper(PIP_ADAPTER_ADDRESSES adapterInfoPtr)
{
    //Temp buffer for name and description
    std::string tempString;

    //If the string length is greater than 0, proceed. We don't want empty string as the key
    std::size_t wideStringLen = std::wcslen(adapterInfoPtr->FriendlyName);
    std::size_t tempStringLen = wideStringLen + 1; //+1 for null terminator
    if(wideStringLen <= 0)
        return;

    //Convert the string from wide chars to multi byte
    tempString.resize(tempStringLen);
    if(!CTMPerformanceCommon::WSToSWithEllipsisTruncation(tempString.data(), adapterInfoPtr->FriendlyName, tempStringLen, wideStringLen))
    {
        CTM_LOG_ERROR("Failed to convert network adapter's friendly name to multi byte string. Error code: ", GetLastError());
        return;
    }

    //Create an entry in AdapterInfoVector
    auto&&[_, adapterInfo] = adapterInfoVector.emplace_back(std::move(tempString), NetworkAdapterInfo{});

    //Time for adapter description
    wideStringLen = std::wcslen(adapterInfoPtr->Description);
    tempStringLen = wideStringLen + 1; //+1 for null terminator
    //Write N/A to description if failed
    if(wideStringLen <= 0)
        tempString = "N/A";
    else
    {
        tempString.resize(tempStringLen);
        if(!CTMPerformanceCommon::WSToSWithEllipsisTruncation(tempString.data(), adapterInfoPtr->Description, tempStringLen, wideStringLen))
            CTM_LOG_ERROR("Failed to convert network adapter's description to multi byte string. Error code: ", GetLastError());
    }

    //1) Adapter description
    adapterInfo.adapterDescription = std::move(tempString);

    //2) IP Addresses
    PIP_ADAPTER_UNICAST_ADDRESS unicast = adapterInfoPtr->FirstUnicastAddress;
    while(unicast)
    {
        auto& addressString = adapterInfo.adapterIPAddresses.emplace_back();
        int res = GetNameInfoA(unicast->Address.lpSockaddr, unicast->Address.iSockaddrLength,
                                addressString.data() + 1, INET6_ADDRSTRLEN, nullptr, 0, NI_NUMERICHOST);
        if(res != 0)
            CTM_LOG_ERROR("Failed to get ip address. Error: ", gai_strerrorA(res), ". Expect some jibberish IP addresses.");

        //Check for whether is IPv4 or IPv6
        if(unicast->Address.lpSockaddr->sa_family == AF_INET6)
            addressString[0] = static_cast<char>(CTMNetworkAddressType::NetworkIPv6); //Represents IPv6
        else if(unicast->Address.lpSockaddr->sa_family == AF_INET)
            addressString[0] = static_cast<char>(CTMNetworkAddressType::NetworkIPv4); //Represents IPv4
        else
            addressString[0] = static_cast<char>(CTMNetworkAddressType::NetworkUnknown); //Other stuff unhandled

        unicast = unicast->Next;
    }

    //3) MAC Address
    if(adapterInfoPtr->PhysicalAddressLength > 0)
    {
        std::size_t offset = 0;
        for(ULONG i = 0; i < adapterInfoPtr->PhysicalAddressLength; ++i)
        {
            //ChatGPT: Format each byte as two hexadecimal digits and append a dash (`-`) except for the last byte
            int written = snprintf(adapterInfo.adapterMACAddress + offset, sizeof(adapterInfo.adapterMACAddress) - offset, 
                                "%02X%s",
                                static_cast<int>(adapterInfoPtr->PhysicalAddress[i]),
                                (i < adapterInfoPtr->PhysicalAddressLength - 1 ? "-" : ""));
            
            if(written < 0 || offset + written >= sizeof(adapterInfo.adapterMACAddress))
            {
                //Truncate the result if buffer is exceeded
                adapterInfo.adapterMACAddress[sizeof(adapterInfo.adapterMACAddress) - 1] = '\0';
                break;
            }

            offset += written;
        }
    }
    else
        std::strcpy(adapterInfo.adapterMACAddress, "N/A");
    
    //4) Adapter Type
    //INFO: I only handle few stuff which i feel like common. This does not cover the whole range of types
    switch(adapterInfoPtr->IfType)
    {
        case IF_TYPE_ETHERNET_CSMACD:
            adapterInfo.adapterType = "Ethernet";
            break;
        case IF_TYPE_IEEE80211:
            adapterInfo.adapterType = "Wi-Fi";
            break;
        case IF_TYPE_PPP:
            adapterInfo.adapterType = "Point-to-Point Protocol (PPP)";
            break;
        case IF_TYPE_SOFTWARE_LOOPBACK:
            adapterInfo.adapterType = "Loopback";
            break;
        default:
            adapterInfo.adapterType = "Other/Unknown";
            break;
    }
    
    //5) Operational Status
    adapterInfo.adapterOperationalStatus = (adapterInfoPtr->OperStatus == IfOperStatusUp ? "Up" : "Down");
}

bool CTMPerformanceNETScreen::CTMConstructorInitWLANInfo()
{
    //WLAN Information for getting the network connected information (name and stuff)
    HANDLE wlanHandle        = nullptr;
    DWORD  clientVersion     = 2;
    DWORD  negotiatedVersion = 0;

    DWORD status = WlanOpenHandle(clientVersion, nullptr, &negotiatedVersion, &wlanHandle);
    if(status != ERROR_SUCCESS)
    {
        CTM_LOG_ERROR("Failed to open WLAN handle. Error code: ", status);
        return false;
    }

    PWLAN_INTERFACE_INFO_LIST interfaceInfoList;
    status = WlanEnumInterfaces(wlanHandle, nullptr, &interfaceInfoList);
    if(status != ERROR_SUCCESS)
    {
        CTM_LOG_ERROR("Failed to enum wlan interfaces. Error code: ", status);
        WlanCloseHandle(wlanHandle, nullptr);
        return false;
    }

    //Loop over all the interfaces
    bool gotAvailableNetwork = false;
    for(DWORD i = 0; i < interfaceInfoList->dwNumberOfItems; i++)
    {
        WLAN_INTERFACE_INFO& interfaceInfo = interfaceInfoList->InterfaceInfo[i];
        //We got what we needed, break outta loop
        if(CTMConstructorAvailNetworkList(wlanHandle, interfaceInfo))
        {
            gotAvailableNetwork = true;
            break;
        }
    }

    //Finally free the allocated memory and close wlan handle
    WlanFreeMemory(interfaceInfoList);
    WlanCloseHandle(wlanHandle, nullptr);

    return gotAvailableNetwork;
}

bool CTMPerformanceNETScreen::CTMConstructorAvailNetworkList(HANDLE wlanHandle, WLAN_INTERFACE_INFO& interfaceInfo)
{
    PWLAN_AVAILABLE_NETWORK_LIST networkList = nullptr;
    DWORD status = WlanGetAvailableNetworkList(wlanHandle, &interfaceInfo.InterfaceGuid, 0, nullptr, &networkList);
    
    if(status != ERROR_SUCCESS)
    {
        CTM_LOG_ERROR("Failed to get list of available network. Error code: ", status);
        return false;
    }

    //Loop over the list of networks. Also check if we got the connected network profile which we needed
    bool isNetworkConnected = false;
    for(DWORD i = 0; i < networkList->dwNumberOfItems; i++)
    {
        WLAN_AVAILABLE_NETWORK& network = networkList->Network[i];
        //Check if the network is connected to something. If it is, then get the name of the connected network and break out
        if(network.dwFlags & 1) //Having to do '1' because MinGW wasn't recognizing WLAN_AVAILABLE_NETWORK_CONNECTED macro
        {
            isNetworkConnected = true;
            
            //Get the length of the network
            std::size_t srcLen  = std::wcslen(network.strProfileName);
            std::size_t destLen = srcLen + 1;
            //Resize the string 'connectedNetwork' string to hold srcLen + 1 for null terminator
            connectedNetwork.resize(destLen);
            //Wide char to Multi byte conversion
            if(!CTMPerformanceCommon::WSToSWithEllipsisTruncation(connectedNetwork.data(), network.strProfileName, destLen, srcLen))
                CTM_LOG_ERROR("Failed to convert wide string network to multi byte. Expect jibberish info for connected network name.");
            
            break; //Currently only caring for one network
        }
    }

    //Finally free the allocated 'networkList'
    WlanFreeMemory(networkList);
    return isNetworkConnected;
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
    //Reset the adapterID before we display the table
    adapterID = 0;
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
        
        //Connected network
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Connected Network");
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(connectedNetwork.c_str());

        //Thought this looked nice before printing the actual stuff
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Available Network Adapters:");
        
        //Print out any network adapter which exists
        for(auto &&[adapterName, adapterInfo] : adapterInfoVector)
        {
            //Create a tree node while pushing the ID so even if some adapter names are same, ImGui won't complain
            ImGui::PushID(adapterID++);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if(ImGui::TreeNodeEx(adapterName.c_str(), ImGuiTreeNodeFlags_SpanAllColumns))
            {
                RenderNetworkAdapterInfo(adapterInfo);
                ImGui::TreePop();
            }

            ImGui::PopID();
        }

        ImGui::EndTable();
    }
}

void CTMPerformanceNETScreen::RenderNetworkAdapterInfo(NetworkAdapterInfo& adapterInfo)
{
    ImGui::Indent();
    
    ImGui::TableNextRow();
    
    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted("Description");
    ImGui::TableSetColumnIndex(1);
    ImGui::Text("%s", adapterInfo.adapterDescription.c_str());

    //IP Adresses (Both IPv4 and IPv6)
    for(auto &&ip : adapterInfo.adapterIPAddresses)
    {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        //Check for the address type. The first bit of it represents the address type
        if(ip[0] == static_cast<char>(CTMNetworkAddressType::NetworkIPv6))
            ImGui::TextUnformatted("IP Address (IPv6)");
        else if(ip[0] == static_cast<char>(CTMNetworkAddressType::NetworkIPv4))
            ImGui::TextUnformatted("IP Address (IPv4)");
        else
            ImGui::TextUnformatted("IP Address (Unknown)");
        
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%s", ip.data() + 1); //Data after the first bit represents the address itself
    }

    ImGui::TableNextRow();

    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted("MAC Address");
    ImGui::TableSetColumnIndex(1);
    ImGui::Text("%s", adapterInfo.adapterMACAddress);

    ImGui::TableNextRow();

    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted("Adapter Type");
    ImGui::TableSetColumnIndex(1);
    ImGui::Text("%s", adapterInfo.adapterType);

    ImGui::TableNextRow();

    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted("Operational Status");
    ImGui::TableSetColumnIndex(1);
    ImGui::Text("%s", adapterInfo.adapterOperationalStatus);

    ImGui::Unindent();
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

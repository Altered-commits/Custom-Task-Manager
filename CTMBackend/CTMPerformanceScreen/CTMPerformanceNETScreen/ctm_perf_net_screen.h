#ifndef CTM_PERFORMANCE_NET_SCREEN_HPP
#define CTM_PERFORMANCE_NET_SCREEN_HPP

//Using std::max, no need for macro
#undef max

//Windows stuff
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <wlanapi.h>
#include <pdh.h>
#include <pdhmsg.h>
//My stuff
#include "../ctm_perf_common.h"
#include "../ctm_perf_graph.h"
#include "../../ctm_constants.h"
#include "../../ctm_base_state.h"
#include "../../ctm_logger.h"
#include "../../CTMGlobalManagers/ctm_critical_resource_guard.h"
#include "../../CTMGlobalManagers/ctm_winsock_manager.h"
//Stdlib stuff
#include <cstring>
#include <memory>
#include <array>
#include <vector>

//'using' makes my life what?
using AdapterIPAddressVector = std::vector<std::array<char, INET6_ADDRSTRLEN + 1>>; // +1 to store the type of address at the beginning

//Union for float manipulation. Used for graph value representation (10.24KB, 1.42MB and so on)
//There is no point in using 'double' as the value isn't going to be that big
union CTMFloatView
{
    float         floatView;
    std::uint32_t bitView;  //32bit representation of float (1(sign) 11111111(exponent) 11111111111111111111111(mantissa))
};

//Stores the network adapter information
struct NetworkAdapterInfo
{
    std::string            adapterDescription;
    AdapterIPAddressVector adapterIPAddresses;
    char                   adapterMACAddress[24];
    const char*            adapterType;
    const char*            adapterOperationalStatus;
};
//'usi...
using AdapterInfoVector = std::vector<std::pair<std::string, NetworkAdapterInfo>>;

class CTMPerformanceNETScreen : public CTMBasePerformanceScreen, protected CTMPerformanceUsageGraph<2, double>
{
public:
    CTMPerformanceNETScreen();
    ~CTMPerformanceNETScreen() override;

protected:
    void OnRender() override;
    void OnUpdate() override;

private: //Constructor and Destructor functions
    bool CTMConstructorInitPDH();
    bool CTMConstructorInitNetworkAdapterInfo();
    void CTMConstructorAdapterInfoHelper(PIP_ADAPTER_ADDRESSES);
    bool CTMConstructorInitWLANInfo();
    bool CTMConstructorAvailNetworkList(HANDLE, WLAN_INTERFACE_INFO&);
    void CTMDestructorCleanupPDH();

private: //Render functions
    void RenderNetworkStatistics();
    void RenderNetworkAdapterInfo(NetworkAdapterInfo&);

private: //Update functions
    void UpdateNetworkUsage();

private: //Helper functions
    double GetPdhFormattedNetworkData(PDH_HCOUNTER);
    float  EncodeNetworkDataWithType(double);
    void   DecodeNetworkDataWithType(float, std::uint8_t&, float&);

private: //Global managers
    CTMCriticalResourceGuard& resourceGuard  = CTMCriticalResourceGuard::GetInstance();
    CTMWinsockManager&        winsockManager = CTMWinsockManager::GetInstance(); //This is all we have to do lmao, no need for anything else
    //Unique names for registering resource guard
    const char* pdhCleanupFunctionName = "CTMPerformanceNETScreen::CleanupPDH";
    
private: //PDH variables
    PDH_HQUERY   hQuery = nullptr;
    PDH_HCOUNTER hNetworkSent, hNetworkRecieved;

private: //Collapsable header variables
    bool isStatisticsHeaderExpanded = false;

private: //Network info variables
    //Used with 'PdhGetFormattedCounterArrayA'
    DWORD             networkInfoBufferSize = 0;
    DWORD             networkInfoItemCount  = 0;
    std::vector<BYTE> networkInfoBuffer;

private: //Stuff which cannot be in metricsVector (as i don't want to display them)
    //Value used for graph. Represented in KB
    double totalSentBytesKB = 0, totalRecBytesKB = 0;
    //Value used for displaying graph y limits. Dynamically changes its unit
    float displayMaxYLimit = 0;

private: //Misc
    //In multi graph, 0th index represents shaded plot, 1st represents line plot
    enum class CTMNetworkTypeIndex   { NetworkSent, NetworkRecieved };
    enum class CTMNetworkAddressType { NetworkUnknown, NetworkIPv4, NetworkIPv6 };
    //Colors for graphs
    constexpr static ImVec4 graphColors[2] = { {0.8f, 0.4f, 0.1f, 0.5f}, {0.9f, 0.6f, 0.3f, 1.0f} };

private: //Dynamically decide network values unit
    //With 64 bit unsigned int, max u can go is 16 Exabyte. Hence the 'EB'
    constexpr static const char*  dataUnits[]   = { "KB", "MB", "GB", "TB", "PB", "EB" };
    constexpr static std::uint8_t dataUnitsSize = sizeof(dataUnits) / sizeof(dataUnits[0]);

private: //The stuff to actually display
    enum class MetricsVectorIndex : std::uint8_t { SentData, RecievedData };

    MetricsVector metricsVector = {
                                    std::make_pair("Sent Data",              0.0),
                                    std::make_pair("Recieved Data",          0.0)
                                };
    //Connected network (only caring to display one network for now)
    std::string connectedNetwork;
    //Grouped adapter information
    AdapterInfoVector adapterInfoVector;
    std::uint8_t      adapterID = 0;
};

#endif
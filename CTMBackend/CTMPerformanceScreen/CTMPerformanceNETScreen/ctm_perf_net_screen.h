#ifndef CTM_PERFORMANCE_NET_SCREEN_HPP
#define CTM_PERFORMANCE_NET_SCREEN_HPP

//Using std::max, no need for macro
#undef max

//Windows stuff
#include <Windows.h>
#include <Pdh.h>
#include <PdhMsg.h>
//My stuff
#include "../ctm_perf_common.h"
#include "../ctm_perf_graph.h"
#include "../../ctm_constants.h"
#include "../../ctm_base_state.h"
#include "../../ctm_logger.h"
#include "../../CTMGlobalManagers/ctm_critical_resource_guard.h"
//Stdlib stuff
#include <vector>

//Union for float manipulation. Used for graph value representation (10.24KB, 1.42MB and so on)
//There is no point in using 'double' as the value isn't going to be that big
union CTMFloatView
{
    float         floatView;
    std::uint32_t bitView;  //32bit representation of float (1(sign) 11111111(exponent) 11111111111111111111111(mantissa))
};

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
    void CTMDestructorCleanupPDH();

private: //Render functions
    void RenderNetworkStatistics();

private: //Update functions
    void UpdateNetworkUsage();

private: //Helper functions
    double GetPdhFormattedNetworkData(PDH_HCOUNTER);
    float  EncodeNetworkDataWithType(double);
    void   DecodeNetworkDataWithType(float, std::uint8_t&, float&);

private: //Resource guard
    CTMCriticalResourceGuard& resourceGuard = CTMCriticalResourceGuard::GetInstance();
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
    enum class CTMNetworkTypeIndex { NetworkSent, NetworkRecieved };
    //Colors for graphs
    constexpr static ImVec4 graphColors[2] = { {0.8f, 0.4f, 0.1f, 0.5f}, {0.9f, 0.6f, 0.3f, 1.0f} };

private: //Dynamically decide network values unit
    //With 64 bit unsigned int, max u can go is 16 Exabyte. Hence the 'EB'
    constexpr static const char*  dataUnits[]   = { "KB", "MB", "GB", "TB", "PB", "EB" };
    constexpr static std::uint8_t dataUnitsSize = sizeof(dataUnits) / sizeof(dataUnits[0]);

private: //The stuff to actually display
    enum class MetricsVectorIndex : std::uint8_t 
    {
        SentData, RecievedData
    };

    MetricsVector metricsVector = {
                                    std::make_pair("Sent Data",              0.0),
                                    std::make_pair("Recieved Data",          0.0)
                                };
};

#endif
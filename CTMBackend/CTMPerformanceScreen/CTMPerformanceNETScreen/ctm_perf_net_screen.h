#ifndef CTM_PERFORMANCE_NET_SCREEN_HPP
#define CTM_PERFORMANCE_NET_SCREEN_HPP

//Using std::max, no need for macro
#undef max

//Windows stuff
#include <Windows.h>
#include <Pdh.h>
#include <PdhMsg.h>
//My stuff
#include "../ctm_perf_graph.h"
#include "../../ctm_constants.h"
#include "../../ctm_base_state.h"
#include "../../ctm_logger.h"
#include "../../CTMGlobalManagers/ctm_critical_resource_guard.h"
//Stdlib stuff
#include <vector>

//Bit mask for data units. Works for uint16 only (to embed network data unit in the value itself)
#define CTM_SET_UINT16_MSB_3_BITS(value, setterValue) value = ((value & 0b0001111111111111) | ((setterValue & 0x07) << 13))
#define CTM_GET_UINT16_MSB_3_BITS(value)              ((value >> 13) & 0x07)
#define CTM_GET_UINT16_LSB_13_BITS(value)             (value & 0b0001111111111111)

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

private: //Update functions
    void UpdateNetworkUsage();

private: //Helper functions
    double        GetPdhFormattedNetworkData(PDH_HCOUNTER);
    std::uint16_t ConvertNetworkDataUnit(double);

private: //Resource guard
    CTMCriticalResourceGuard& resourceGuard = CTMCriticalResourceGuard::GetInstance();
    //Unique names for registering resource guard
    const char* pdhCleanupFunctionName = "CTMPerformanceNETScreen::CleanupPDH";
    
private: //PDH variables
    PDH_HQUERY   hQuery = nullptr;
    PDH_HCOUNTER hNetworkSent, hNetworkRecieved;

private: //Network info variables
    //Used with 'PdhGetFormattedCounterArrayA'
    DWORD             networkInfoBufferSize = 0;
    DWORD             networkInfoItemCount  = 0;
    std::vector<BYTE> networkInfoBuffer;
    //Value used for graph. Represented in KB
    double totalSentBytesKB = 0, totalRecBytesKB = 0;
    //Value used for displaying. Dynamically changes its unit
    std::uint16_t displaySentBytes = 0, displayRecBytes = 0, displayMaxYLimit = 0;

private: //Misc
    //In multi graph, 0th index represents shaded plot, 1st represents line plot
    enum class CTMNetworkTypeIndex { NetworkSent, NetworkRecieved };
    //Colors for graphs
    constexpr static ImVec4 graphColors[2] = { {0.8f, 0.4f, 0.1f, 0.5f}, {0.9f, 0.6f, 0.3f, 1.0f} };

private: //Dynamically decide network values unit
    //With 64 bit unsigned int, max u can go is 16 Exabyte. Hence the 'EB'
    constexpr static const char*  dataUnits[]   = { "KB", "MB", "GB", "TB", "PB", "EB" };
    constexpr static std::uint8_t dataUnitsSize = sizeof(dataUnits) / sizeof(dataUnits[0]);
};

#endif
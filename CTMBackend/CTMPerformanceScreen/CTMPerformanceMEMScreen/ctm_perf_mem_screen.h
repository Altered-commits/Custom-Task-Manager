#ifndef CTM_PERFORMANCE_MEM_SCREEN
#define CTM_PERFORMANCE_MEM_SCREEN

//Windows stuff
#include <windows.h>
#include <Psapi.h>
//My stuff
#include "../ctm_perf_graph.h"
#include "../ctm_perf_common.h"
#include "../../ctm_base_state.h"
#include "../../ctm_logger.h"
#include "../../CTMGlobalManagers/ctm_wmi_manager.h"
//Stdlib stuff
#include <string>
#include <unordered_map>

//Used for grouping of RAM info
struct MemoryInfo
{
    char         manufacturer[32] = { 0 };
    double       capacity         = 0.0;
    int32_t      configClockSpeed = 0;
    const char*  formFactor       = nullptr;
};

//'using' makes my life alot easier
using MemoryInfoMap = std::unordered_map<std::string, MemoryInfo>;

class CTMPerformanceMEMScreen : public CTMBasePerformanceScreen, protected CTMPerformanceUsageGraph
{
public:
    CTMPerformanceMEMScreen();
    ~CTMPerformanceMEMScreen() override;

protected:
    void OnRender() override;
    void OnUpdate() override;

private: //Constructor functions
    bool CTMConstructorInitMemoryInfo();
    bool CTMConstructorInitPerformanceInfo();
    bool CTMConstructorQueryWMI();
    void CTMConstructorFormFactorStringify(MemoryInfo&, int);

private: //Render functions
    void RenderMemoryStatistics();
    void RenderPerRAMInfo(MemoryInfo&);

private: //Update functions
    double UpdateMemoryStatus();
    bool   UpdateMemoryPerfInfo(); //Bool used for constructor function

private: //WMI Querying stuff
    CTMWMIManager& wmiManager = CTMWMIManager::GetInstance();

private: //Collapsable header variables
    bool isStatisticsHeaderExpanded = false;

private: //Conversion ratios, as they are commonly used
    constexpr static double BToGB  = (1024.0 * 1024.0 * 1024.0);
    constexpr static double KBToGB = (1024.0 * 1024.0);

private: //Structs used by winapi to get memory data
    MEMORYSTATUSEX          memStatus = { 0 };
    PERFORMANCE_INFORMATION perfInfo  = { 0 };
    
private: //Memory Variables
    //Check ctm_perf_cpu_screen.h
    enum class MetricsVectorIndex : std::uint8_t 
    {
        MemoryUsage, AvailableMemory, CommittedMemory, CachedData,
        PagedPool, NonPagedPool, InstalledMemory, OSUsableMemory, HardwareReservedMemory,
        MemorySlots
    };

    //Committed memory is displayed as (committed / total), so for that i need to store total, committed is in metricsVector itself
    double totalPageFile    = 0.0;
    DWORD  totalMemorySlots = 0;

    //Stores all the metric value pairs
    MetricsVector metricsVector = {
                                    std::make_pair("Memory In Use",            0.0),
                                    std::make_pair("Available Memory",         0.0),
                                    std::make_pair("Committed Virtual Memory", 0.0),
                                    std::make_pair("Cached Data",              0.0),
                                    std::make_pair("Paged Pool",               0.0),
                                    std::make_pair("Non-Paged Pool",           0.0),
                                    std::make_pair("Installed Memory",         0.0),
                                    std::make_pair("OS Usable Memory",         0.0),
                                    std::make_pair("Hardware Reserved Memory", 0.0),
                                    std::make_pair("Memory Slots",             (DWORD)0)
                                };

    //Tree like structure for each RAM stick
    MemoryInfoMap memoryInfoMap;
};

#endif
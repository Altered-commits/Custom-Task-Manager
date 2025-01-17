#ifndef CTM_PERFORMANCE_MEM_SCREEN_HPP
#define CTM_PERFORMANCE_MEM_SCREEN_HPP

//Windows stuff
#include <windows.h>
#include <Psapi.h>
#include <Pdh.h>
//My stuff
#include "../ctm_perf_graph.h"
#include "../ctm_perf_common.h"
#include "../../ctm_constants.h"
#include "../../ctm_base_state.h"
#include "../../ctm_logger.h"
#include "../../CTMGlobalManagers/ctm_wmi_manager.h"
#include "../../CTMGlobalManagers/ctm_critical_resource_guard.h"
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

class CTMPerformanceMEMScreen : public CTMBasePerformanceScreen, protected CTMPerformanceUsageGraph<1, double>
{
public:
    CTMPerformanceMEMScreen();
    ~CTMPerformanceMEMScreen() override;

protected:
    void OnRender() override;
    void OnUpdate() override;

private: //Constructor and Destructor functions
    bool CTMConstructorInitPDH();
    bool CTMConstructorInitMemoryInfo();
    bool CTMConstructorInitPerformanceInfo();
    bool CTMConstructorQueryWMI();
    void CTMConstructorFormFactorStringify(MemoryInfo&, int);
    //
    void CTMDestructorCleanupPDH();

private: //Render functions
    void RenderMemoryStatistics();
    void RenderPerRAMInfo(MemoryInfo&);
    //
    void RenderMemoryComposition();
    void RenderMemoryCompositionBarFilled(ImDrawList*, ImVec2&, float, float, ImU32, ImU32, float);
    void RenderMemoryCompositionBarBorder(ImDrawList*, ImVec2&, float, float, ImU32, float);

private: //Update functions
    double UpdateMemoryStatus();
    bool   UpdateMemoryPerfInfo(); //Bool used for constructor function
    void   UpdateMemoryCompositionInfo();

private: //WMI Querying stuff and resource guard for PDH
    CTMWMIManager&            wmiManager    = CTMWMIManager::GetInstance();
    CTMCriticalResourceGuard& resourceGuard = CTMCriticalResourceGuard::GetInstance();

    //Unique name for resource guard cleanup function. Didn't really want to make another 'private' section
    const char* pdhCleanupFunctionName = "CTMPerformanceMEMScreen::ClosePDHQuery";

private: //Collapsable header variables
    bool isStatisticsHeaderExpanded  = false;
    bool isMemoryCompositionExpanded = false;

private: //PDH query variables
    PDH_HQUERY   hQuery                 = nullptr;
    PDH_HCOUNTER hModifiedMemoryCounter = nullptr;

private: //Structs used by winapi to get memory data
    MEMORYSTATUSEX          memStatus = { 0 };
    PERFORMANCE_INFORMATION perfInfo  = { 0 };

private: //Some stuff which cannot be in the metricsVector (as i don't want to display them)
    //
    SIZE_T totalPageSize    = 0;
    double totalPageFile    = 0.0;
    DWORD  totalMemorySlots = 0;
    //By default in MB
    double memoryModified = 0.0;

private: //Memory Variables
    //Check ctm_perf_cpu_screen.h
    enum class MetricsVectorIndex : std::uint8_t 
    {
        MemoryUsage, AvailableMemory, CommittedMemory, CachedData,
        PagedPool, NonPagedPool, InstalledMemory, OSUsableMemory, HardwareReservedMemory,
        MemorySlots
    };

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
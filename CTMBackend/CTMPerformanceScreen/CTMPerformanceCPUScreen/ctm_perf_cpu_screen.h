#ifndef CTM_PERFORMANCE_CPU_SCREEN
#define CTM_PERFORMANCE_CPU_SCREEN

//Windows stuff
#include <windows.h>
#include <winternl.h>
#include <ntstatus.h>
#include <wrl/client.h> //For ComPtr
//My stuff
#include "../ctm_perf_graph.h"
#include "../../ctm_base_state.h"
#include "../../ctm_logger.h"
#include "../../CTMGlobalManagers/ctm_wmi_manager.h"
//Stdlib stuff
#include <variant>
#include <vector>
#include <memory>
#include <cmath>
#include <cstring>

//MSVC warnings forcing me to use _s function, NOPE, i handle stuff in my own way so no thanks
#pragma warning(disable:4996)

//Simple VARIANT wrapper for RAII destruction as <atlcomcli.h> is not supported on MinGW
class CTMVariant : public tagVARIANT
{
public:
    CTMVariant()  { VariantInit(this); }
    ~CTMVariant() { VariantClear(this); }
};

//Few defines for WMI querying because the function was looking cluttered, an absolute mess
//Other files will just use proper functions
#define WMI_QUERYING_FAILED_BUFFER_ERROR(buffer, errorMsg) std::strncpy(buffer, "Failed", sizeof(buffer) - 1);\
                                                            CTM_LOG_ERROR(errorMsg);\
                                                            return false;

#define WMI_QUERYING_FAILED_PURE_ERROR(errorMsg) CTM_LOG_ERROR(errorMsg);\
                                                 return false;

#define WMI_QUERYING_TRY_WSTOS(outBuffer, inBuffer, errorMsg) \
                    if(std::wcstombs(outBuffer, inBuffer, sizeof(outBuffer) - 1) == static_cast<std::size_t>(-1))\
                    { WMI_QUERYING_FAILED_BUFFER_ERROR(outBuffer, errorMsg) }

#define WMI_QUERYING_START_CONDITION(cond)  if(cond) {
#define WMI_QUERYING_END_CONDITION()        }

//Pass this value to 'NtQuerySystemInformation' to get the required per logical processor information
#define SystemProcessorPerformanceInformation 8

//Function ptr, we retrieve this function from ntdll.dll file using GetProcAddress.
//Used for getting per logical processor information in this case (by passing the above 'SystemProcessorPerformanceInformation' value)
typedef NTSTATUS(WINAPI* NtQuerySystemInformation_t)
                (SYSTEM_INFORMATION_CLASS, PVOID, ULONG, PULONG);

//'using' makes my life alot easier
using MetricsValue               = std::variant<DWORD, double, const char*>;
using MetricsVector              = std::vector<std::pair<const char*, MetricsValue>>;
using ProcessorPerformanceVector = std::vector<SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION>;

class CTMPerformanceCPUScreen : public CTMBasePerformanceScreen, protected CTMPerformanceUsageGraph
{
public:
    CTMPerformanceCPUScreen();
    ~CTMPerformanceCPUScreen() override;

protected:
    void OnRender() override;
    void OnUpdate() override;

private: //Constructor functions
    bool CTMConstructorInitNTDLL();
    bool CTMConstructorQueryWMI();
    bool CTMConstructorGetCPUInfo();
    bool CTMConstructorGetCPULogicalInfo();

private: //Render functions
    void RenderCPUStatistics();
    void RenderLogicalProcessorHeatmap();

private: //Some helper functions
    double GetTotalCPUUsage();
    void   UpdatePerLogicalProcessorInfo();

private: //NT dll
    HMODULE                     hNtdll                    = nullptr;
    NtQuerySystemInformation_t  NtQuerySystemInformation  = nullptr;

private: //WMI manager
    CTMWMIManager& wmiManager = CTMWMIManager::GetInstance();

private: //CPU related variables
    ULARGE_INTEGER prevIdleTime         = { 0 };
    ULARGE_INTEGER prevKernelTime       = { 0 };
    ULARGE_INTEGER prevUserTime         = { 0 };
    CHAR           cpuNameBuffer[49]    = { 0 }; //48 bytes with 1 byte for null terminator in case the name takes the whole buffer
    CHAR           vendorNameBuffer[13] = { 0 }; //12 bytes with 1 byte for null terminator in case the name takes the whole buffer

private: //Logical processor variables
    //Related to per logical processor usage stuff. All of this is set inside 'CTMConstructorGetCPUInfo'
    ProcessorPerformanceVector currProcessorPerformanceInfo, prevProcessorPerformanceInfo;
    std::vector<double>        perLogicalProcessorUsage;
    DWORD                      processorPerformanceInfoSize = 0;
    //For heatmap, we pre calculate the grid rows and columns in the constructor itself
    DWORD                      heatmapGridRows = 0, heatmapGridColumns = 0;
    //Just a small optimization, we won't update information if collapsing header is not expanded
    bool                       isHeatmapHeaderExpanded = false;

private: //Statistics variables
    //Enum class to access vector indexes properly without hardcoding index.
    //These must be in the order matching that of the metrics vector. This approach is alot more maintainable than using hardcoded indexes
    enum class MetricsVectorIndex { Usage, CPUName, VendorName, Architecture, Sockets, Cores, LogicalProcessors, MaxClockSpeed };
    
    //Stores all the metric value pairs
    MetricsVector metricsVector = {
                                    std::make_pair("Usage",              0.0),
                                    std::make_pair("CPU Name",           cpuNameBuffer),
                                    std::make_pair("Vendor Name",        vendorNameBuffer),
                                    std::make_pair("Architecture",       nullptr),
                                    std::make_pair("Sockets",            (DWORD)0),
                                    std::make_pair("Cores",              (DWORD)0),
                                    std::make_pair("Logical Processors", (DWORD)0),
                                    std::make_pair("Max Clock Speed",    0.0)
                                    //Rest everything is added dynamically using emplace_back
                                };
};

#endif
#ifndef CTM_PERFORMANCE_CPU_SCREEN
#define CTM_PERFORMANCE_CPU_SCREEN

//Windows stuff
#include <Windows.h>
#include <winternl.h>
#include <ntstatus.h>
//My stuff
#include "../ctm_perf_graph.h"
#include "../../ctm_base_state.h"
//Stdlib stuff
#include <variant>
#include <vector>
#include <iostream>
#include <memory>
#include <cmath>
#include <cstring>

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
    bool CTMConstructorGetCPUInfo();
    bool CTMConstructorGetCPUInfoFromRegistry();
    bool CTMConstructorGetCPUName(HKEY);
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

private: //CPU related variables
    ULARGE_INTEGER prevIdleTime      = { 0 };
    ULARGE_INTEGER prevKernelTime    = { 0 };
    ULARGE_INTEGER prevUserTime      = { 0 };
    CHAR           cpuNameBuffer[64] = { 0 };

private: //Logical processor variables
    //Related to per logical processor usage stuff. All of this is set inside 'CTMConstructorGetCPUInfo'
    ProcessorPerformanceVector currProcessorPerformanceInfo, prevProcessorPerformanceInfo;
    std::vector<double>        perLogicalProcessorUsage;
    DWORD                      processorPerformanceInfoSize = 0;
    //For heatmap, we pre calculate the grid rows and columns in the constructor itself
    DWORD                      heatmapGridRows = 0, heatmapGridColumns = 0;

private: //Statistics variables
    //Enum class to access vector indexes properly without hardcoding index.
    //These must be in the order matching that of the metrics vector. This approach is alot more maintainable than using hardcoded indexes
    enum class MetricsVectorIndex { Usage, Name, Architecture, Sockets, Cores, LogicalProcessors, BaseSpeed };
    
    //Stores all the metric value pairs
    MetricsVector metricsVector = {
                                    std::make_pair("Usage",              0.0),
                                    std::make_pair("Name",               cpuNameBuffer),
                                    std::make_pair("Architecture",       nullptr),
                                    std::make_pair("Sockets",            (DWORD)0),
                                    std::make_pair("Cores",              (DWORD)0),
                                    std::make_pair("Logical Processors", (DWORD)0),
                                    std::make_pair("Base Speed",         0.0)
                                    //Rest everything is added dynamically using emplace_back
                                };
};

#endif
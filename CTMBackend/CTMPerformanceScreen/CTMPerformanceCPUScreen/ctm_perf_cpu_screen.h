#ifndef CTM_PERFORMANCE_CPU_SCREEN_HPP
#define CTM_PERFORMANCE_CPU_SCREEN_HPP

//Windows stuff
#include <windows.h>
#include <winternl.h>
#include <ntstatus.h>
#include <Psapi.h>
//My stuff
#include "../ctm_perf_graph.h"
#include "../ctm_perf_common.h"
#include "../../ctm_base_state.h"
#include "../../ctm_logger.h"
#include "../../CTMGlobalManagers/ctm_wmi_manager.h"
//Stdlib stuff
#include <memory>
#include <cmath>
#include <cstring>

//MSVC warnings forcing me to use _s function... NOPE, i handle stuff in my own way so no thanks
#pragma warning(disable:4996)

//Pass this value to 'NtQuerySystemInformation' to get the required per logical processor information
#define SystemProcessorPerformanceInformation 8

//Function ptr, we retrieve this function from ntdll.dll file using GetProcAddress.
//Used for getting per logical processor information in this case (by passing the above 'SystemProcessorPerformanceInformation' value)
typedef NTSTATUS(WINAPI* NtQuerySystemInformation_t)
                (SYSTEM_INFORMATION_CLASS, PVOID, ULONG, PULONG);

//'using' makes my life alot easier... alr ik this is getting annoying. BUT I WON'T STOP ;)
using ProcessorPerformanceVector = std::vector<SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION>;

class CTMPerformanceCPUScreen : public CTMBasePerformanceScreen, protected CTMPerformanceUsageGraph<1, double>
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
    void   UpdateProcessCounters();

private: //NT dll
    HMODULE                     hNtdll                    = nullptr;
    NtQuerySystemInformation_t  NtQuerySystemInformation  = nullptr;

private: //WMI manager
    CTMWMIManager& wmiManager = CTMWMIManager::GetInstance();

private: //Performance Info
    PERFORMANCE_INFORMATION performanceInfo = { sizeof(PERFORMANCE_INFORMATION) };

private: //CPU related variables
    ULARGE_INTEGER prevIdleTime         = { 0 };
    ULARGE_INTEGER prevKernelTime       = { 0 };
    ULARGE_INTEGER prevUserTime         = { 0 };
    CHAR           cpuNameBuffer[49]    = { 0 }; //48 bytes with 1 byte for null terminator in case the name takes the whole buffer
    CHAR           vendorNameBuffer[13] = { 0 }; //12 bytes with 1 byte for null terminator in case the name takes the whole buffer
    CHAR           virtEnabledBuffer[7] = { 0 }; //6 bytes with 1 byte for null terminator, True, False or Failed

private: //Logical processor variables
    //Related to per logical processor usage stuff. All of this is set inside 'CTMConstructorGetCPUInfo'
    ProcessorPerformanceVector currProcessorPerformanceInfo, prevProcessorPerformanceInfo;
    std::vector<double>        perLogicalProcessorUsage;
    DWORD                      processorPerformanceInfoSize = 0;
    //For heatmap, we pre calculate the grid rows and columns in the constructor itself
    DWORD                      heatmapGridRows = 0, heatmapGridColumns = 0;

private: //Real small optimization variables
    bool isHeatmapHeaderExpanded    = false;
    bool isStatisticsHeaderExpanded = false;

private: //Processes, Handles and Threads (count)
    ULONG             systemInformationBufferSize = 1024; //Some random initial value
    std::vector<BYTE> systemInformationBuffer;

private: //Statistics variables
    //Enum class to access vector indexes properly without hardcoding index.
    //These must be in the order matching that of the metrics vector. This approach is alot more maintainable than using hardcoded indexes
    enum class MetricsVectorIndex : std::uint8_t 
    {
        Usage, TotalProcesses, TotalHandles, TotalThreads,
        CPUName, VendorName, Architecture, Sockets, Cores,
        LogicalProcessors, VirtEnabled, MaxClockSpeed
    };
    
    //Stores all the metric value pairs
    MetricsVector metricsVector = {
                                    std::make_pair("Usage",                  0.0),
                                    std::make_pair("Total Processes",        (DWORD)0),
                                    std::make_pair("Total Handles",          (DWORD)0),
                                    std::make_pair("Total Threads",          (DWORD)0),
                                    std::make_pair("CPU Name",               cpuNameBuffer),
                                    std::make_pair("Vendor Name",            vendorNameBuffer),
                                    std::make_pair("Architecture",           nullptr),
                                    std::make_pair("Sockets",                (DWORD)0),
                                    std::make_pair("Cores",                  (DWORD)0),
                                    std::make_pair("Logical Processors",     (DWORD)0),
                                    std::make_pair("Virtualization Enabled", virtEnabledBuffer),
                                    std::make_pair("Max Clock Speed",        0.0)
                                    //Rest everything is added dynamically using emplace_back
                                };
};

#endif
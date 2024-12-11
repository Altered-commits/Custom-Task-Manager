#ifndef CTM_PROCESS_MENU_HPP
#define CTM_PROCESS_MENU_HPP

//ImGui stuff
#include "../../ImGUI/imgui.h"
//My stuff
#include "../ctm_base_state.h"
#include "ctm_process_menu_etw.h"
//Stdlib stuff
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <thread>
#include <atomic>
#include <algorithm>
#include <iostream> //For debugging
//Winapi stuff
#include <windows.h>
#include <Psapi.h>
#include <winternl.h>
#include <ntstatus.h>

//These functions belong to NT DLL, useful for getting process information like memory usage, etc.
typedef NTSTATUS(NTAPI* NtQueryInformationProcess_t)
                (HANDLE, PROCESSINFOCLASS, PVOID, ULONG, PULONG);

typedef NTSTATUS(NTAPI* NtQuerySystemInformation_t)
                (SYSTEM_INFORMATION_CLASS, PVOID, ULONG, PULONG);

//--------------------Some useful structs--------------------
/*
 * This is the hacky stuff we are doing to get details for processes which can't be accessed with 'OpenProcess'
 * Base CTM_SYSTEM_PROCESS_INFORMATION structure
 * For more info, follow this link: https://www.geoffchappell.com/studies/windows/km/ntoskrnl/api/ex/sysinfo/process.htm
 */
typedef struct _CTM_SYSTEM_PROCESS_INFORMATION
{
    ULONG NextEntryOffset;
    ULONG NumberOfThreads;
    LARGE_INTEGER WorkingSetPrivateSize; // Available in 6.0 and higher
    ULONG HardFaultCount;                // Available in 6.1 and higher
    ULONG NumberOfThreadsHighWatermark;  // Available in 6.1 and higher
    ULONGLONG CycleTime;                 // Available in 6.1 and higher
    LARGE_INTEGER CreateTime;
    LARGE_INTEGER UserTime;
    LARGE_INTEGER KernelTime;
    UNICODE_STRING ImageName;
    ULONG BasePriority;
    HANDLE UniqueProcessId;
    HANDLE InheritedFromUniqueProcessId;
    ULONG HandleCount;
    ULONG SessionId;
    ULONG_PTR UniqueProcessKey;
    SIZE_T PeakVirtualSize;
    SIZE_T VirtualSize;
    ULONG PageFaultCount;
    SIZE_T PeakWorkingSetSize;
    SIZE_T WorkingSetSize;
    SIZE_T QuotaPeakPagedPoolUsage;
    SIZE_T QuotaPagedPoolUsage;
    SIZE_T QuotaPeakNonPagedPoolUsage;
    SIZE_T QuotaNonPagedPoolUsage;
    SIZE_T PagefileUsage;
    SIZE_T PeakPagefileUsage;
    SIZE_T PrivatePageCount;
    LARGE_INTEGER ReadOperationCount;
    LARGE_INTEGER WriteOperationCount;
    LARGE_INTEGER OtherOperationCount;
    LARGE_INTEGER ReadTransferCount;
    LARGE_INTEGER WriteTransferCount;
    LARGE_INTEGER OtherTransferCount;
} CTM_SYSTEM_PROCESS_INFORMATION, *PCTM_SYSTEM_PROCESS_INFORMATION;

//struct _VM_COUNTERS_EX2 and VM_COUNTERS_EX, i did not find these in the header files hence i just declared them myself
typedef struct _VM_COUNTERS_EX
{
    SIZE_T PeakVirtualSize;
    SIZE_T VirtualSize;
    ULONG PageFaultCount;
    SIZE_T PeakWorkingSetSize;
    SIZE_T WorkingSetSize;
    SIZE_T QuotaPeakPagedPoolUsage;
    SIZE_T QuotaPagedPoolUsage;
    SIZE_T QuotaPeakNonPagedPoolUsage;
    SIZE_T QuotaNonPagedPoolUsage;
    SIZE_T PagefileUsage;
    SIZE_T PeakPagefileUsage;
    SIZE_T PrivateUsage;
} VM_COUNTERS_EX, *PVM_COUNTERS_EX;

typedef struct _VM_COUNTERS_EX2
{
    VM_COUNTERS_EX CountersEx;
    SIZE_T PrivateWorkingSetSize;
    ULONGLONG SharedCommitUsage;
} VM_COUNTERS_EX2, *PVM_COUNTERS_EX2;

struct ProcessInfo
{
    //Perfect 8 byte alignment
    double      memoryUsage;
    double      cpuUsage;
    double      networkUsage;
    DWORD       processId;
    BOOL        isStaleEntry = FALSE; //Every entry is not stale by default

    ProcessInfo(DWORD processId, double memoryUsage, double cpuUsage, double networkUsage)
        : processId(processId), memoryUsage(memoryUsage), cpuUsage(cpuUsage), networkUsage(networkUsage)
    {}
};

struct PreviousUpdateInformation
{
    //CPU
    FILETIME prevProcKernelTime;
    FILETIME prevProcUserTime;
};

//'using' makes my life much easier instead of writing this horrendously long type everywhere
using ProcessMap                = std::unordered_map<std::string, std::vector<ProcessInfo>>;
using ProcessHandleMap          = std::unordered_map<DWORD, HANDLE>;
using ProcessExcludedHandleSet  = std::unordered_set<DWORD>;
using PreviousInformationMap    = std::unordered_map<DWORD, PreviousUpdateInformation>;
using ProcessInfoBuffer         = std::vector<BYTE>;

class CTMProcessScreen : public CTMBaseState
{
public:
    CTMProcessScreen();
    ~CTMProcessScreen() override;

protected:
    void OnRender() override;
    void OnUpdate() override;

private: //Constructor and destructor functions
    bool CTMConstructorInitNTDLL();
    bool CTMConstructorInitEventTracingThread();

    void CTMDestructorCleanNTDLL();
    void CTMDestructorCleanEventTracingThread();
    void CTMDestructorCleanMappedHandles();

private: //Helper function
    void   RenderProcessVector(const std::vector<ProcessInfo>&, const std::string&);
    //
    void   UpdateProcessInfo();
    void   UpdateProcessMapWithProcessHandle(HANDLE, DWORD, const std::string&, FILETIME, FILETIME);
    void   UpdateProcessMapWithoutProcessHandle(DWORD, const std::string&, PCTM_SYSTEM_PROCESS_INFORMATION, FILETIME, FILETIME);
    void   UpdateProcessMap(DWORD, const std::string&, double, double, double);
    //
    HANDLE GetProcessHandleFromId(DWORD);
    void   RemoveStaleEntries();

private: //Helper function as well, just wanted to keep them seperate
    double CalculateMemoryUsage(HANDLE);
    double CalculateCpuUsage(HANDLE, DWORD, FILETIME, FILETIME);
    double CalculateCpuUsageDelta(DWORD, FILETIME, FILETIME, LARGE_INTEGER, LARGE_INTEGER); //Didn't really have a better name honestly

private: //NT dll
    HMODULE                     hNtdll                    = nullptr;
    NtQueryInformationProcess_t NtQueryInformationProcess = nullptr;
    NtQuerySystemInformation_t  NtQuerySystemInformation  = nullptr;

private: //Event Tracing for process usage (Like network usage, etc)
    CTMProcessScreenEventTracing processUsageEventTracing;
    std::thread                  processUsageEventTracingThread;

private:
    //Mapping process id to its handle to use 'OpenProcess' as less as possible
    ProcessHandleMap         processIdToHandleMap;
    ProcessExcludedHandleSet processExcludedHandleSet = {0, 4, 172, 212, 776, 1204, 1352, 1424, 1452}; //Can't be opened using 'OpenProcess'
    //Always group the processes together (app name as the key)
    ProcessMap groupedProcessesMap;
    //Get the process information directly to this buffer
    ULONG             processInfoBufferSize = 1024;
    ProcessInfoBuffer processInfoBuffer;
    //Stores previous values for CPU process times
    PreviousInformationMap perProcessPreviousInformationMap;
    FILETIME               ftPrevSysKernelTime = {},
                           ftPrevSysUserTime   = {};
};

#endif
#ifndef CTM_PROCESS_MENU_HPP
#define CTM_PROCESS_MENU_HPP

#include "../ImGUI/imgui.h"
#include "ctm_base_state.h"
#include "ctm_udp_event_listener.h"
//Stdlib stuff
#include <vector>
#include <string>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <algorithm>
#include <iostream> //For debugging
//Winapi stuff
#include <windows.h>
#include <Psapi.h>
#include <winternl.h>
#include <ntstatus.h>

//To get the same result as task manager we use NtQueryInformationProcess function (part of ntdll.dll) for:
//  -Memory usage
//Along with the struct _VM_COUNTERS_EX2 and VM_COUNTERS_EX
typedef struct _VM_COUNTERS_EX {
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

typedef struct _VM_COUNTERS_EX2 {
    VM_COUNTERS_EX CountersEx;
    SIZE_T PrivateWorkingSetSize;
    ULONGLONG SharedCommitUsage;
} VM_COUNTERS_EX2, *PVM_COUNTERS_EX2;

//These functions belong to NT DLL, which will be linked during runtime
typedef NTSTATUS(NTAPI* NtQueryInformationProcess_t)
                (HANDLE, PROCESSINFOCLASS, PVOID, ULONG, PULONG);

typedef NTSTATUS(NTAPI* NtQuerySystemInformation_t)
                (SYSTEM_INFORMATION_CLASS, PVOID, ULONG, PULONG);


//--------------------Some useful structs--------------------
struct ProcessInfo
{
    //These are in a specific order for the sake of 8-byte alignment
    //Also it doesn't matter if i use ('bool' keyword) or ('BOOL' typedef) when it is right below DWORD cuz alignment
    //This version gives me 80 bytes total, but when i push BOOL below std::string, it gives me 88 bytes, so yeah
    double      memoryUsage;
    double      diskUsage;
    double      cpuUsage;
    ULONGLONG   udpUsage;
    DWORD       processId;
    BOOL        isStaleEntry = FALSE; //Every entry is not stale by default
    std::string name;

    ProcessInfo(DWORD processId, const std::string& name, double memoryUsage, double diskUsage, double cpuUsage, ULONGLONG udpUsage)
        : processId(processId), name(name), memoryUsage(memoryUsage), diskUsage(diskUsage), cpuUsage(cpuUsage), udpUsage(udpUsage)
    {}
};

struct PreviousUpdateInformation
{
    //CPU
    FILETIME    prevProcKernelTime;
    FILETIME    prevProcUserTime;
    //Disk
    IO_COUNTERS prevIOCounter;
};

//'using' makes my life much easier instead of writing this horrendously long type everywhere
using ProcessMap             = std::unordered_map<std::string, std::vector<ProcessInfo>>;
using PreviousInformationMap = std::unordered_map<DWORD, PreviousUpdateInformation>;
using ProcessInfoBuffer      = std::vector<BYTE>;

class CTMProcessScreen : public CTMBaseState
{
public:
    CTMProcessScreen();
    ~CTMProcessScreen() override;

protected:
    void OnRender() override;
    void OnUpdate() override;

private: //Constructor initialization and destructor functions
    bool CTMConstructorInitNTDLL();
    bool CTMConstructorInitUDPUsageThread();

    void CTMDestructorCleanNTDLL();
    void CTMDestructorCleanUDPUsageThread();

private: //Helper function
    void RenderProcessInstances(const std::vector<ProcessInfo>&);
    void UpdateProcessInfo();
    void UpdateMapWithProcessInfo(DWORD, const std::string&, double, double, double, ULONGLONG);
    void RemoveStaleEntries();

private: //Helper function as well, just wanted to keep them seperate
    double CalculateMemoryUsage(HANDLE);
    double CalculateDiskUsage(HANDLE, DWORD);
    double CalculateCpuUsage(HANDLE, FILETIME, FILETIME, FILETIME, FILETIME);

private: //NT dll
    HMODULE hNtdll = nullptr;
    NtQueryInformationProcess_t NtQueryInformationProcess = nullptr;
    NtQuerySystemInformation_t  NtQuerySystemInformation  = nullptr;

private: //ETW listener for udp usage
    UDPEventListener udpUsageEventListener;
    std::thread      udpUsageEventListenerThread;

private:
    //Always group the processes together (app name as the key)
    ProcessMap groupedProcesses;
    //Get the process information directly to this buffer
    ULONG             processInfoBufferSize = 1024;
    ProcessInfoBuffer processInfoBuffer;
    
    //Consolidated map for per-process previous update information
    PreviousInformationMap perProcessPreviousInformation;
    FILETIME prevSysKernel = {}, prevSysUser = {};
};

#endif
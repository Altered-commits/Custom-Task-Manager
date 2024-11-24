#ifndef CTM_PROCESS_MENU_HPP
#define CTM_PROCESS_MENU_HPP

#include "../ImGUI/imgui.h"
#include "ctm_base_state.h"
//Stdlib stuff
#include <vector>
#include <string>
#include <unordered_map>
#include <iostream> //For debugging
//Winapi stuff
#include <windows.h>
#include <Psapi.h>
#include <winternl.h>

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

//We use dynamic loading of libraries
typedef NTSTATUS(NTAPI* NtQueryInformationProcess_t)
                (HANDLE, PROCESSINFOCLASS, PVOID, ULONG, PULONG);

//Store process information
struct ProcessInfo
{
    DWORD       processId;
    std::string name;
    double      memoryUsage;
    double      diskUsage;
    double      cpuUsage;

    ProcessInfo(DWORD processId, const std::string& name, double memoryUsage, double diskUsage, double cpuUsage)
        : processId(processId), name(name), memoryUsage(memoryUsage), diskUsage(diskUsage), cpuUsage(cpuUsage)
    {}
};

//Store previous update information
struct PreviousUpdateInformation
{
    //CPU
    FILETIME    prevProcKernelTime;
    FILETIME    prevProcUserTime;
    //Disk
    IO_COUNTERS prevIOCounter;
};

//Using statement to make our life easier
using ProcessMap             = std::unordered_map<std::string, std::vector<ProcessInfo>>;
using PreviousInformationMap = std::unordered_map<DWORD, PreviousUpdateInformation>;

class CTMProcessScreen : public CTMBaseState
{
public:
    CTMProcessScreen();
    ~CTMProcessScreen() override;

protected:
    void OnRender() override;
    void OnUpdate() override;

private: //Helper function
    void RenderProcessInstances(const std::vector<ProcessInfo>&);
    void UpdateProcessInfo();

private: //Helper function as well, just wanted to keep them seperate
    double CalculateMemoryUsage(HANDLE);
    double CalculateDiskUsage(HANDLE, DWORD);
    double CalculateCpuUsage(HANDLE, FILETIME, FILETIME, FILETIME, FILETIME);

private: //NT dll
    HMODULE hNtdll = nullptr;
    NtQueryInformationProcess_t NtQueryInformationProcess = nullptr;

private:
    //Always group the processes together (app name as the key)
    ProcessMap groupedProcesses;
    
    //Consolidated map for per-process previous update information
    PreviousInformationMap perProcessPreviousInformation;
    FILETIME prevSysKernel = {}, prevSysUser = {};
};

#endif
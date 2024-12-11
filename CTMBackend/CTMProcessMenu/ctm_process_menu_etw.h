#ifndef CTM_PROCESS_MENU_ETW_HPP
#define CTM_PROCESS_MENU_ETW_HPP

//Windows and etw stuff
#include <windows.h>
#include <evntrace.h>
#include <tdh.h>
//Stdlib stuff
#include <unordered_map>
#include <memory>
#include <mutex>
#include <iostream> //For debugging
//My stuff
#include "../ctm_constants.h"

//Just for better understanding, also we want total network usage across TCP and UDP (Both IPv4 and IPv6)
using NetworkUsage           = ULONGLONG;
using UniquePtrToByteArray   = std::unique_ptr<BYTE[]>;
using ProcessNetworkUsageMap = std::unordered_map<DWORD, NetworkUsage>;

//Mutex to ensure thread safety
extern std::mutex globalPsEtwMutex; //It stands for Global Process Screen Event Tracing Mutex

//Used by pretty much everything
extern ProcessNetworkUsageMap globalProcessNetworkUsageMap; 

//To differentiate between different GUID's properties, like Kernel Network has TCP and UDP, etc.
enum class HandlePropertyForEventType
{
    KERNEL_NETWORK_TCP,
    KERNEL_NETWORK_UDP
};

class CTMProcessScreenEventTracing
{
public:
    CTMProcessScreenEventTracing() = default;
    
    //No need for copy or move operations
    CTMProcessScreenEventTracing(const CTMProcessScreenEventTracing&)            = delete;
    CTMProcessScreenEventTracing& operator=(const CTMProcessScreenEventTracing&) = delete;
    CTMProcessScreenEventTracing(CTMProcessScreenEventTracing&&)                 = delete;
    CTMProcessScreenEventTracing& operator=(CTMProcessScreenEventTracing&&)      = delete;

    ~CTMProcessScreenEventTracing()
    {
        // //Stop tracing events
        // Stop();
        //Reset event buffers manually as these are static and wont really get destructed automatically after object gets destructed
        eventInfoBuffer.reset();
        eventInfoBufferSize = 0;
    }

public: //Main functions
    bool Start();
    bool ProcessEvents();
    void Stop();

private: //Helper functions
    bool StartTraceSession();
    bool EnableProvider();
    bool OpenTraceSession();

private:
    static void        WritePropInfoToMap(PEVENT_RECORD, HandlePropertyForEventType);
    static void WINAPI EventCallback(PEVENT_RECORD);

private: //ETW stuff
    //Custom session name as we use specific providers for our work
    LPCWSTR              sessionName       = L"CTM_ProcessScreen_ETWSession";
    GUID                 krnlNetworkGuid   = MICROSOFT_WINDOWS_KERNEL_NETWORK_GUID;
    TRACEHANDLE          sessionHandle;
    TRACEHANDLE          traceHandle;
    UniquePtrToByteArray tracePropsBuffer;

private: //Used in WritePropsToMap, and because it's static, we need to make these static
    static UniquePtrToByteArray eventInfoBuffer; //Containing trace event information
    static ULONG                eventInfoBufferSize;
};

#endif
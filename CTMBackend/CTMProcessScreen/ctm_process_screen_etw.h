#ifndef CTM_PROCESS_MENU_ETW_HPP
#define CTM_PROCESS_MENU_ETW_HPP

//Windows stuff
#include <windows.h>
#include <evntrace.h>
#include <tdh.h>
//Stdlib stuff
#include <unordered_map>
#include <memory>
#include <mutex>
//My stuff
#include "../ctm_constants.h"
#include "../ctm_logger.h"

//Just for better understanding, also we want total network usage across TCP and UDP (Both IPv4 and IPv6)
using ProcessUsageType        = ULONGLONG;
using UniquePtrToByteArray    = std::unique_ptr<BYTE[]>;
using ProcessResourceUsageMap = std::unordered_map<DWORD, ProcessUsageType>;

//Mutex to ensure thread safety
extern std::mutex globalPsEtwMutex; //It stands for Global Process Screen Event Tracing Mutex

//Used by pretty much everything but bound to the scope of 'CTMProcessScreenEventTracing' class
extern ProcessResourceUsageMap globalProcessNetworkUsageMap;
extern ProcessResourceUsageMap globalProcessFileUsageMap;

//To differentiate between different GUID's properties, like Kernel Network has different properties (TCP and UDP), etc.
enum class HandlePropertyForEventType : std::uint8_t
{
    KernelNetworkTcpUdp,
    KernelFileRW
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

        //Also its better to clear up the global maps as they won't do it themselves (while they don't add as much memory but still)
        globalProcessFileUsageMap.clear();
        globalProcessNetworkUsageMap.clear();
    }

public: //Main functions
    bool Start();
    bool ProcessEvents();
    void Stop();

private: //Helper functions
    void Cleanup();
    bool StartTraceSession();
    bool ConfigureProvider(const GUID&, ULONG);
    bool EnableProvider(bool);
    bool OpenTraceSession();

private: //Static functions
    static void        WritePropInfoToMap(PEVENT_RECORD, HandlePropertyForEventType);
    static void WINAPI EventCallback(PEVENT_RECORD);

private: //ETW stuff
    //Custom session name as we use specific providers for our work
    LPCWSTR               sessionName              = L"CTM_ProcessScreen_ETWSession";
    TRACEHANDLE           sessionHandle;
    TRACEHANDLE           traceHandle;
    UniquePtrToByteArray  tracePropsBuffer;
    bool                  isKrnlNetworkInitialized = false,
                          isKrnlFileInitialized    = false;

private: //ETW Stuff but static (as these are used in static functions).
    //Used in WritePropsToMap
    static UniquePtrToByteArray eventInfoBuffer; //Containing trace event information
    static ULONG                eventInfoBufferSize;
    //Used in EventCallback
    constexpr static GUID krnlNetworkGuid = MICROSOFT_WINDOWS_KERNEL_NETWORK_GUID,
                          krnlFileGuid    = MICROSOFT_WINDOWS_KERNEL_FILE_GUID;
};

#endif
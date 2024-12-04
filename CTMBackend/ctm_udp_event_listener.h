#ifndef CTM_UDP_EVENT_LISTENER_HPP
#define CTM_UDP_EVENT_LISTENER_HPP

//Windows and etw tracing stuff
#include <windows.h>
#include <evntrace.h>
#include <tdh.h>
//Stdlib stuff
#include <unordered_map>
#include <memory>
#include <mutex>
#include <iostream> //For debugging
//CTM stuff
#include "ctm_constants.h"

using UniquePtrToByteArray = std::unique_ptr<BYTE[]>;
using UDPUsageProcessMap   = std::unordered_map<DWORD, ULONGLONG>;

//Mutex to ensure thread safety
extern std::mutex udpUsageMapMutex;

//Used by pretty much everything
extern UDPUsageProcessMap globalProcessUDPUsageMap;

class UDPEventListener
{
public:
    UDPEventListener() = default;
    
    UDPEventListener(const UDPEventListener&) = delete;
    UDPEventListener(UDPEventListener&&)      = delete;

    ~UDPEventListener()
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
    static void        WriteUDPUsageToMap(PEVENT_RECORD);
    static void WINAPI EventCallback(PEVENT_RECORD);

private: //ETW stuff
    //Custom session name as we use specific providers for our work
    LPCWSTR              sessionName       = L"CTM_UDPUsage_TraceSession";
    GUID                 tcpIpProviderGuid = TCPIP_PROVIDER_GUID;
    TRACEHANDLE          sessionHandle;
    TRACEHANDLE          traceHandle;
    UniquePtrToByteArray tracePropsBuffer;

private: //Used in WriteUDPUsageToMap, and because it's static, we need to make these static
    static UniquePtrToByteArray eventInfoBuffer; //Containing trace event information
    static ULONG                eventInfoBufferSize;
};

#endif
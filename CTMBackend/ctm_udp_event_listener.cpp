#include "ctm_udp_event_listener.h"

//Init global variables
std::mutex         udpUsageMapMutex;
UDPUsageProcessMap globalProcessUDPUsageMap;

//Init static data members
ULONG                UDPEventListener::eventInfoBufferSize = 0;
UniquePtrToByteArray UDPEventListener::eventInfoBuffer     = nullptr;

//--------------------PUBLIC FUNCTIONS-------------------- 
bool UDPEventListener::Start()
{
    if (!StartTraceSession())
            return false;

    if (!EnableProvider())
        return false;

    if (!OpenTraceSession())
        return false;

    return true;
}

bool UDPEventListener::ProcessEvents()
{
    ULONG status = ProcessTrace(&traceHandle, 1, nullptr, nullptr);
    return status == ERROR_SUCCESS;
}

void UDPEventListener::Stop()
{
    if (traceHandle) {
        CloseTrace(traceHandle);
        traceHandle = 0;
    }

    if (sessionHandle) {
        StopTraceW(sessionHandle, nullptr, reinterpret_cast<EVENT_TRACE_PROPERTIES*>(tracePropsBuffer.get()));
        sessionHandle = 0;
    }
}

//--------------------HELPER FUNCTIONS--------------------
bool UDPEventListener::StartTraceSession()
{
    ULONG bufferSize = sizeof(EVENT_TRACE_PROPERTIES) + sizeof(WCHAR) * 1024;
    tracePropsBuffer = std::make_unique<BYTE[]>(bufferSize);
    ZeroMemory(tracePropsBuffer.get(), bufferSize);

    auto properties = reinterpret_cast<EVENT_TRACE_PROPERTIES*>(tracePropsBuffer.get());
    properties->Wnode.BufferSize = bufferSize;
    properties->Wnode.ClientContext = 1; //Use QueryPerformanceCounter for timestamps
    properties->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    properties->LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
    properties->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);

    ULONG status = StartTraceW(&sessionHandle, sessionName, properties);
    if (status != ERROR_SUCCESS)
        return false;

    return true;
}

bool UDPEventListener::EnableProvider()
{
    ULONG status = EnableTraceEx2(sessionHandle, &tcpIpProviderGuid, EVENT_CONTROL_CODE_ENABLE_PROVIDER,
                                  TRACE_LEVEL_INFORMATION, 0, 0, 0, nullptr);
    if (status != ERROR_SUCCESS)
    {
        Stop();
        return false;
    }

    return true;
}

bool UDPEventListener::OpenTraceSession()
{
    EVENT_TRACE_LOGFILEW traceLog = {};
    traceLog.LoggerName = const_cast<PWSTR>(sessionName);
    traceLog.ProcessTraceMode = PROCESS_TRACE_MODE_REAL_TIME | PROCESS_TRACE_MODE_EVENT_RECORD;
    traceLog.EventRecordCallback = EventCallback;

    traceHandle = OpenTraceW(&traceLog);

    if (traceHandle == INVALID_PROCESSTRACE_HANDLE)
    {
        Stop();
        return false;
    }

    return true;
}

//--------------------STATIC FUNCTIONS--------------------
void UDPEventListener::WriteUDPUsageToMap(PEVENT_RECORD eventRecord)
{
    UINT32 numBytes = 0;
    UINT32 pid      = 0;
    
    ULONG status = TdhGetEventInformation(eventRecord, 0, nullptr,
                    reinterpret_cast<PTRACE_EVENT_INFO>(eventInfoBuffer.get()), &eventInfoBufferSize);
    
    //If buffer isn't long enough, reallocate
    if(status == ERROR_INSUFFICIENT_BUFFER)
    {
        eventInfoBuffer = std::make_unique<BYTE[]>(eventInfoBufferSize);

        status = TdhGetEventInformation(eventRecord, 0, nullptr, 
                    reinterpret_cast<PTRACE_EVENT_INFO>(eventInfoBuffer.get()), &eventInfoBufferSize);

        //Failed to get information, print some error and move on for now
        if(status != ERROR_SUCCESS)
        {
            std::cerr << "Failed to get Event Information. Error: " << status << '\n';
            return;
        }
    }
    //The buffer is long enough, just use it for now
    PTRACE_EVENT_INFO        eventInfo = reinterpret_cast<PTRACE_EVENT_INFO>(eventInfoBuffer.get());
    PROPERTY_DATA_DESCRIPTOR propertyData = {};

    for (ULONG i = 0; i < eventInfo->PropertyCount; ++i) {
        const wchar_t* propNameW = reinterpret_cast<const wchar_t*>(
            reinterpret_cast<const BYTE*>(eventInfo) + eventInfo->EventPropertyInfoArray[i].NameOffset);

        propertyData.PropertyName = reinterpret_cast<ULONGLONG>(propNameW);
        propertyData.ArrayIndex = 0;

        if(wcscmp(propNameW, L"NumBytes") == 0)
        {
            ULONG status = TdhGetProperty(eventRecord, 0, nullptr, 1, &propertyData, sizeof(numBytes), reinterpret_cast<PBYTE>(&numBytes));
            if (status != ERROR_SUCCESS)
                std::cerr << "Failed to get NumBytes property. Error: " << status << '\n';
        }

        else if(wcscmp(propNameW, L"Pid") == 0)
        {
            ULONG status = TdhGetProperty(eventRecord, 0, nullptr, 1, &propertyData, sizeof(pid), reinterpret_cast<PBYTE>(&pid));
            if (status != ERROR_SUCCESS)
                std::cerr << "Failed to get NumBytes property. Error: " << status << '\n';
        }
    }

    //As globalProcessUDPUsageMap will be used in threaded environment, use locks
    std::lock_guard<std::mutex> lock(udpUsageMapMutex);

    //Finally write the UDP usage to map
    globalProcessUDPUsageMap[pid] += numBytes;
}

void WINAPI UDPEventListener::EventCallback(PEVENT_RECORD eventRecord)
{
    const ULONG  processId = eventRecord->EventHeader.ProcessId;
    const USHORT eventId   = eventRecord->EventHeader.EventDescriptor.Id;
    
    switch (eventId)
    {
        //UDP Events
        case 1169: //UdpEndpointSendMessages
        case 1170: //UdpEndpointReceiveMessages
            WriteUDPUsageToMap(eventRecord);
            break;
    }
}
#include "ctm_process_menu_etw.h"

//Init global variables
std::mutex              globalPsEtwMutex;
ProcessResourceUsageMap globalProcessNetworkUsageMap;
ProcessResourceUsageMap globalProcessFileUsageMap;

//Init static data members
ULONG                CTMProcessScreenEventTracing::eventInfoBufferSize = 0;
UniquePtrToByteArray CTMProcessScreenEventTracing::eventInfoBuffer     = nullptr;

//--------------------PUBLIC FUNCTIONS-------------------- 
bool CTMProcessScreenEventTracing::Start()
{
    if(!StartTraceSession())
            return false;

    if(!EnableProvider(true))
        return false;

    if(!OpenTraceSession())
        return false;

    return true;
}

bool CTMProcessScreenEventTracing::ProcessEvents()
{
    ULONG status = ProcessTrace(&traceHandle, 1, nullptr, nullptr);
    return status == ERROR_SUCCESS;
}

void CTMProcessScreenEventTracing::Stop()
{
    //Disable providers before cleaning up resources
    EnableProvider(false);
    Cleanup();
}

//--------------------HELPER FUNCTIONS--------------------
void CTMProcessScreenEventTracing::Cleanup()
{
    if(traceHandle)
    {
        CloseTrace(traceHandle);
        traceHandle = 0;
    }
    if(sessionHandle)
    {
        StopTraceW(sessionHandle, nullptr, reinterpret_cast<EVENT_TRACE_PROPERTIES*>(tracePropsBuffer.get()));
        sessionHandle = 0;
    }
}

bool CTMProcessScreenEventTracing::StartTraceSession()
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
    if(status != ERROR_SUCCESS)
        return false;

    return true;
}

bool CTMProcessScreenEventTracing::ConfigureProvider(const GUID& providerGuid, ULONG controlCode)
{
    ULONG status = EnableTraceEx2(sessionHandle, &providerGuid, controlCode, TRACE_LEVEL_INFORMATION, 0, 0, 0, nullptr);
    if(status != ERROR_SUCCESS)
    {
        std::cerr << "Failed to " << (controlCode == EVENT_CONTROL_CODE_ENABLE_PROVIDER ? "enable" : "disable")
                  << " provider(Data1): " << providerGuid.Data1 << ". Error: " << status << '\n';
        return false;
    }
    return true;
}

bool CTMProcessScreenEventTracing::EnableProvider(bool shouldEnable)
{
    ULONG controlCode = shouldEnable ? EVENT_CONTROL_CODE_ENABLE_PROVIDER : EVENT_CONTROL_CODE_DISABLE_PROVIDER;

    if(shouldEnable)
    {
        isKrnlNetworkInitialized = ConfigureProvider(krnlNetworkGuid, controlCode);
        if(!isKrnlNetworkInitialized)
        {
            Cleanup();
            return false;
        }
        isKrnlFileInitialized = ConfigureProvider(krnlFileGuid, controlCode);
        if(!isKrnlFileInitialized)
        {
            Cleanup();
            return false;
        }
    }
    else
    {
        if(isKrnlNetworkInitialized) { ConfigureProvider(krnlNetworkGuid, controlCode); isKrnlNetworkInitialized = false; }
        if(isKrnlFileInitialized)    { ConfigureProvider(krnlFileGuid, controlCode);    isKrnlFileInitialized    = false; }
    }

    return true;
}

bool CTMProcessScreenEventTracing::OpenTraceSession()
{
    EVENT_TRACE_LOGFILEW traceLog = {};
    traceLog.LoggerName = const_cast<PWSTR>(sessionName);
    traceLog.ProcessTraceMode = PROCESS_TRACE_MODE_REAL_TIME | PROCESS_TRACE_MODE_EVENT_RECORD;
    traceLog.EventRecordCallback = EventCallback;

    traceHandle = OpenTraceW(&traceLog);

    if(traceHandle == INVALID_PROCESSTRACE_HANDLE)
    {
        Cleanup();
        return false;
    }

    return true;
}

//--------------------STATIC FUNCTIONS--------------------
void CTMProcessScreenEventTracing::WritePropInfoToMap(PEVENT_RECORD eventRecord, HandlePropertyForEventType eventType)
{
    //For both Kernel Network and Kernel File
    //By default let the processId have a value (If its Kernel Network, it will be changed below)
    UINT32 processId    = eventRecord->EventHeader.ProcessId,
           processUsage = 0;
    //Also i'm using ULONG instead of TDHSTATUS cuz g++ is not recognizing it for some odd reason
    ULONG status        = 0;

    //Try to get information with the existing buffer
    status = TdhGetEventInformation(eventRecord, 0, nullptr,
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
    PTRACE_EVENT_INFO        eventInfo    = reinterpret_cast<PTRACE_EVENT_INFO>(eventInfoBuffer.get());
    PROPERTY_DATA_DESCRIPTOR propertyData = {};

    //Loop through properties and find the one we require
    for (ULONG i = 0; i < eventInfo->PropertyCount; ++i)
    {
        LPCWSTR propNameW = reinterpret_cast<LPCWSTR>(reinterpret_cast<PBYTE>(eventInfo) + eventInfo->EventPropertyInfoArray[i].NameOffset);

        propertyData.PropertyName = reinterpret_cast<ULONGLONG>(propNameW);
        propertyData.ArrayIndex = 0;

        switch (eventType)
        {
            case HandlePropertyForEventType::KERNEL_NETWORK_TCPUDP:
            {
                //Both TCP and UDP have properties named 'PID' and 'size'.
                //'PID' is process id and 'size' is the packet size sent over network
                if(wcscmp(propNameW, L"PID") == 0)
                {
                    status = TdhGetProperty(eventRecord, 0, nullptr, 1, &propertyData, sizeof(processId), reinterpret_cast<PBYTE>(&processId));
                    if (status != ERROR_SUCCESS)
                        std::cerr << "Failed to get PID property from Kernel-Network. Error: " << status << '\n';
                }
                else if(wcscmp(propNameW, L"size") == 0)
                {
                    status = TdhGetProperty(eventRecord, 0, nullptr, 1, &propertyData, sizeof(processUsage), reinterpret_cast<PBYTE>(&processUsage));
                    if (status != ERROR_SUCCESS)
                        std::cerr << "Failed to get size property from Kernel-Network. Error: " << status << '\n';
                }
            }
            break;

            case HandlePropertyForEventType::KERNEL_FILE_RW:
            {
                if(wcscmp(propNameW, L"IOSize") == 0)
                {
                    status = TdhGetProperty(eventRecord, 0, nullptr, 1, &propertyData, sizeof(processUsage), reinterpret_cast<PBYTE>(&processUsage));
                    if (status != ERROR_SUCCESS)
                        std::cerr << "Failed to get IOSize property from Kernel-File. Error: " << status << '\n';
                }
            }
            break;
        }
    }

    //As 'usage maps' will be used in threaded environment, use locks
    std::lock_guard<std::mutex> lock(globalPsEtwMutex);

    //Finally write the property information to the desired map
    switch(eventType)
    {
        case HandlePropertyForEventType::KERNEL_NETWORK_TCPUDP:
            globalProcessNetworkUsageMap[processId] += processUsage;
            break;
        case HandlePropertyForEventType::KERNEL_FILE_RW:
            globalProcessFileUsageMap[processId] += processUsage;
            break;
    }
}

void WINAPI CTMProcessScreenEventTracing::EventCallback(PEVENT_RECORD eventRecord)
{
    const USHORT eventId   = eventRecord->EventHeader.EventDescriptor.Id;
    const GUID&  eventGuid = eventRecord->EventHeader.ProviderId;

    //Check if the provider is Kernel Network
    if(InlineIsEqualGUID(eventGuid, krnlNetworkGuid))
    {
        switch (eventId)
        {
            //TCPIPDatasent
            case 10: // IPv4
            case 26: // IPv6
            //TCPIPDatareceived
            case 11: // IPv4
            case 27: // IPv6
            //UDPIPDatasentoverUDPprotocol
            case 42: // IPv4
            case 58: // IPv6
            //UDPIPDatareceivedoverUDPprotocol
            case 43: // IPv4
            case 59: // IPv6
                WritePropInfoToMap(eventRecord, HandlePropertyForEventType::KERNEL_NETWORK_TCPUDP);
                break;
        }
    }
    //The provider is Kernel File
    else
    {
        switch (eventId)
        {
            //Read
            case 15:
            //Write
            case 16:
                WritePropInfoToMap(eventRecord, HandlePropertyForEventType::KERNEL_FILE_RW);
                break;
        }
    }
}
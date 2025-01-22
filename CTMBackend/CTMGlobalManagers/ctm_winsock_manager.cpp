#include "ctm_winsock_manager.h"

CTMWinsockManager::CTMWinsockManager()
{
    //Current WSA version is 2.2
    if(WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        CTM_LOG_ERROR("Failed to call 'WSAStartup'. Winsock functions may not work as expected. Error code: ", WSAGetLastError());
        return;
    }
    
    CTM_LOG_SUCCESS("Successfully called 'WSAStartup'. Expect winsock functions to work.");    
    //Register a resource guard just in case it crashes or something idk
    resourceGuard.RegisterCleanupFunction(wsaCleanupFunctionName, [](){
        WSACleanup();
    });
}

CTMWinsockManager::~CTMWinsockManager()
{
    //Destructor being called = no resource guard needed anymore
    //Call WSACleanup and unregister resource guard
    WSACleanup();
    resourceGuard.UnregisterCleanupFunction(wsaCleanupFunctionName);
}
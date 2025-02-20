#ifndef CTM_WINSOCK_MANAGER_HPP
#define CTM_WINSOCK_MANAGER_HPP

//A very simple class initializing winsock when we want it (aka a singleton)
//Windows stuff
#include <WinSock2.h>
//My stuff
#include "ctm_critical_resource_guard.h"
#include "../CTMPureHeaderFiles/ctm_logger.h"

class CTMWinsockManager
{
public:
    static CTMWinsockManager& GetInstance()
    {
        static CTMWinsockManager winsockManager;
        return winsockManager;
    }

private: //Constructors and Destructors
    CTMWinsockManager();
    ~CTMWinsockManager();

    //No need for copy or move operations
    CTMWinsockManager(const CTMWinsockManager&)            = delete;
    CTMWinsockManager& operator=(const CTMWinsockManager&) = delete;
    CTMWinsockManager(CTMWinsockManager&&)                 = delete;
    CTMWinsockManager& operator=(CTMWinsockManager&&)      = delete;

private: //Resource guard
    CTMCriticalResourceGuard& resourceGuard          = CTMCriticalResourceGuard::GetInstance();
    const char*               wsaCleanupFunctionName = "CTMWinsockManager::CleanupWSA";

private: //Useless
    WSADATA wsaData;
};

#endif
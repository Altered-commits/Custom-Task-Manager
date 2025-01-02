#ifndef CTM_CRITICAL_RESOURCE_GUARD_HPP
#define CTM_CRITICAL_RESOURCE_GUARD_HPP

//Windows stuff
#include <Windows.h>
//My stuff
#include "../ctm_logger.h"
//Stdlib stuff
#include <unordered_map>
#include <functional>

/*
 * A bit of explanation. This isn't some RAII resource cleanup helper class.
 * Its sole purpose is to cleanup stuff when something goes wrong and the app terminates without RAII.
 * Example: Memory Access Violation, etc.
 * In most cases where the app was forcefully terminated, chances are this may not even work. But it can still work as a safe guard in other conditions.
 * Most of the times, the class using this guard will use its own destructor to cleanup resources (When everythings normal).
 */

class CTMCriticalResourceGuard
{
public:
    static CTMCriticalResourceGuard& GetInstance()
    {
        static CTMCriticalResourceGuard criticalResourceGuard;
        return criticalResourceGuard;
    }
    
    //--------------------FUNCTION REGISTERING AND UNREGISTERING--------------------
    void RegisterCleanupFunction(const char*, const std::function<void(void)>&);
    void UnregisterCleanupFunction(const char* fnName);

private: //Constructors and Destructors
    CTMCriticalResourceGuard();
    ~CTMCriticalResourceGuard() = default;

    //No need for copy or move operations
    CTMCriticalResourceGuard(const CTMCriticalResourceGuard&)            = delete;
    CTMCriticalResourceGuard& operator=(const CTMCriticalResourceGuard&) = delete;
    CTMCriticalResourceGuard(CTMCriticalResourceGuard&&)                 = delete;
    CTMCriticalResourceGuard& operator=(CTMCriticalResourceGuard&&)      = delete;

private:
    //--------------------CALLBACK FUNCTIONS--------------------
    static LONG CTMUnhandledExceptionFilter(EXCEPTION_POINTERS* exceptionInfo);
    static BOOL CTMCtrlHandler(DWORD ctrlType);

    //--------------------MAIN CLEANUP FUNCTION--------------------
    void CallCleanupFunctions();

private: //Variables to hold our cleanup functions and check for multiple cleanup calls
    std::unordered_map<const char*, std::function<void(void)>> cleanupFunctionMap;
    bool alreadyCalledCleanup = false;
};

#endif
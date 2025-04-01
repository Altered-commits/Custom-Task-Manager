#include "ctm_critical_resource_guard.h"

CTMCriticalResourceGuard::CTMCriticalResourceGuard()
{
    SetUnhandledExceptionFilter(CTMUnhandledExceptionFilter);
    SetConsoleCtrlHandler(CTMCtrlHandler, TRUE);
}

//--------------------FUNCTION REGISTERING AND UNREGISTERING--------------------
void CTMCriticalResourceGuard::RegisterCleanupFunction(const char *fnName, const std::function<void(void)> &cleanupFn)
{
    if(cleanupFn && fnName)
        cleanupFunctionMap.emplace(fnName, cleanupFn);
    else
        CTM_LOG_ERROR("fnName or cleanupFn was nullptr while registering cleanup function.");
}

void CTMCriticalResourceGuard::UnregisterCleanupFunction(const char *fnName)
{
    if(fnName)
        cleanupFunctionMap.erase(fnName);
    else
        CTM_LOG_ERROR("fnName was nullptr while unregistering cleanup function.");
}

//--------------------CALLBACK FUNCTIONS--------------------
LONG CTMCriticalResourceGuard::CTMUnhandledExceptionFilter(EXCEPTION_POINTERS *exceptionInfo)
{
    CTM_LOG_WARNING_NONL("Unhandled exception caught.");
    
    if(exceptionInfo && exceptionInfo->ExceptionRecord)
        CTM_LOG_TEXT(" Exception code: 0x", exceptionInfo->ExceptionRecord->ExceptionCode);
    else
        CTM_LOG_TEXT(" No exception code provided.");
 
    GetInstance().CallCleanupFunctions();

    return EXCEPTION_CONTINUE_SEARCH;
}

BOOL CTMCriticalResourceGuard::CTMCtrlHandler(DWORD ctrlType)
{
    switch(ctrlType)
    {
        case CTRL_C_EVENT:        //CTRL + C pressed in Console
        case CTRL_CLOSE_EVENT:    //Console window being closed
        case CTRL_SHUTDOWN_EVENT: //System is shutting down
        case CTRL_BREAK_EVENT:    //CTRL + Break pressed in Console
            CTM_LOG_WARNING("Console signal recieved. Signal code: ", ctrlType);
            GetInstance().CallCleanupFunctions();
            ExitProcess(0);
            return TRUE; //We handled the event ourselves, tell windows no need to do anything else

        default:
            return FALSE; //Tell windows to handle what we couldn't handle ourselves
    }
}

//--------------------MAIN CLEANUP FUNCTION--------------------
void CTMCriticalResourceGuard::CallCleanupFunctions()
{
    //Just to make sure
    if(alreadyCalledCleanup)
    {
        CTM_LOG_WARNING("Cleanup was done previously, ignoring the call to cleanup this time.");
        return;
    }

    alreadyCalledCleanup = true;
    //Call all the cleanup functions available, and because we checked for functions to be not nullptr, we can safely call it here
    for(auto&&[fnName, cleanupFn] : cleanupFunctionMap)
    {
        CTM_LOG_INFO("Calling the cleanup function: ", fnName);
        cleanupFn();
    }

    //Clear the cleanup map
    cleanupFunctionMap.clear();
}
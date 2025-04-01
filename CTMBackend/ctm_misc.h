/*
 * I did not want to clutter main.cpp file, hence i created this file.
 * This just stores all the functions / classes / defines which were once written in main.cpp and will further be written.
 * BUT...
 * This just happened to also store stuff used by other classes, which i wasn't able to seperate to other files.
 * Hence this file, in reality, contains all the miscellaneous stuff.
 */
#ifndef CTM_MISCELLANEOUS_HPP
#define CTM_MISCELLANEOUS_HPP

//Windows stuff
#include <windows.h>
//My stuff
#include "CTMPureHeaderFiles/ctm_logger.h"
//Stdlib stuff
#include <cstring>

//Moved this define from 'ctm_constants.h'
//Mutex constant (Not the most secure way to do it but for now lets do it like this).
//Also added a uuid at the end to sort of make it unique????? i have no idea how these work. (Generated using python uuid)
#define CTM_APP_MUTEX_NAME L"Global\\CTM_ImGui_App_Single_Instance_Mutex_88bac14c-67fe-44e9-a29a-071b22a95104"

//CTMMutexGuard needed to clean up the mutex automatically
class CTMMutexGuard
{
public:
    CTMMutexGuard(HANDLE hMutex) : hMutex(hMutex) {}
    ~CTMMutexGuard()
    {
        if(hMutex)
        {
            ReleaseMutex(hMutex);
            CloseHandle(hMutex);
            hMutex = nullptr;
        }
    }

    //We dont want these
    CTMMutexGuard(const CTMMutexGuard&) = delete;
    CTMMutexGuard(CTMMutexGuard&&)      = delete;
    //Nor these
    CTMMutexGuard& operator=(const CTMMutexGuard&) = delete;
    CTMMutexGuard& operator=(CTMMutexGuard&&)      = delete;

public:
    //Used when we need to close handle in another instance of the app (not in the main instance)
    void OnlyCloseHandle()
    {
        if(hMutex)
        {
            CloseHandle(hMutex);
            hMutex = nullptr;
        }
    }

private:
    HANDLE hMutex = nullptr;
};

//Using this as a namespace lmao
class CTMMisc
{
public: //Wide String to String conversion
    static bool  WSToSWithEllipsisTruncation(PSTR, PWSTR, int, std::size_t = 0);

public: //Admin stuff
    //Enable cool privilages to app because why not, ofc there is nothing wrong with giving app one of the highest privilage amirite
    static bool EnableOrDisablePrivilege(LPCWSTR, bool = true);
    static bool PromptUserForAdministratorAccess();
    static bool IsUserAdmin();

public: //Mutex stuff
    //Trying to terminate hung process and regaining mutex ownership
    static bool TerminateAndAcquireMutexOwnership(HWND hWnd, HANDLE hMutex);

public: //Windows and Terminal stuff
    static bool IsWindows10OrGreater();
    static void EnableVirtualTerminalProcessing();
};

#endif
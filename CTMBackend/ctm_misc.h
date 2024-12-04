#ifndef CTM_MISCELLANEOUS_HPP
#define CTM_MISCELLANEOUS_HPP

/*
 * I did not want to clutter main.cpp file, hence i created this file.
 * This just stores all the functions which were once written in main.cpp and will further be written.
 */

#include <windows.h>
#include <iostream> //For debugging

//MutexGuard needed to clean up the mutex automatically
class MutexGuard
{
public:
    MutexGuard(HANDLE hMutex) : hMutex(hMutex) {}
    ~MutexGuard()
    {
        if(hMutex)
        {
            ReleaseMutex(hMutex);
            CloseHandle(hMutex);
            hMutex = nullptr;
        }
    }

    //We dont want these
    MutexGuard(const MutexGuard&) = delete;
    MutexGuard(MutexGuard&&)      = delete;
    //Nor these
    MutexGuard& operator=(const MutexGuard&) = delete;
    MutexGuard& operator=(MutexGuard&&)      = delete;

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

//Enable cool privilages to app because why not, ofc there is nothing wrong with giving app one of the highest privilage amirite
bool EnableOrDisablePrivilege(LPCWSTR privilegeName, bool shouldEnablePrivilege = true)
{
    HANDLE           hToken;
    LUID             luid;
    TOKEN_PRIVILEGES tokenPrivilege;

    if(!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
    {
        std::cerr << "Failed to OpenProcessToken. Error code: " << GetLastError() << '\n';
        return false;
    }

    if(!LookupPrivilegeValueW(NULL, privilegeName, &luid))
    {
        std::cerr << "Failed to LookupPrivilegeValueW. Error code: " << GetLastError() << '\n';
        CloseHandle(hToken);
        return false;
    }

    //Settings for token privilege
    tokenPrivilege.PrivilegeCount           = 1;
    tokenPrivilege.Privileges[0].Luid       = luid;
    tokenPrivilege.Privileges[0].Attributes = shouldEnablePrivilege ? SE_PRIVILEGE_ENABLED : 0;

    if(!AdjustTokenPrivileges(hToken, FALSE, &tokenPrivilege, sizeof(TOKEN_PRIVILEGES), NULL, NULL))
    {
        std::cerr << "Failed to AdjustTokenPrivileges. Error code: " << GetLastError() << '\n';
        CloseHandle(hToken);
        return false;
    }

    if(GetLastError() == ERROR_NOT_ALL_ASSIGNED)
    {
        std::cerr << "The privilege is not assigned to client.\n";
        CloseHandle(hToken);
        return false;
    }

    CloseHandle(hToken);
    return true;
}

bool PromptUserForAdministratorAccess()
{
    //Get the file name of current executable
    WCHAR filePath[MAX_PATH];

    if (!GetModuleFileNameW(NULL, filePath, MAX_PATH)) {
        std::cerr << "Failed to get executable path. Error: " << GetLastError() << "\n";
        return false;
    }

    SHELLEXECUTEINFOW shellExecInfo = { 0 };

    shellExecInfo.cbSize = sizeof(SHELLEXECUTEINFOW);
    shellExecInfo.lpVerb = L"runas";
    shellExecInfo.lpFile = filePath;
    shellExecInfo.nShow  = SW_SHOWNORMAL;

    if (!ShellExecuteExW(&shellExecInfo))
    {
        DWORD err = GetLastError();
        if (err == ERROR_CANCELLED)
            std::cerr << "User refused to grant administrator privileges.\n";
        else
            std::cerr << "Failed to relaunch as administrator. Error: " << GetLastError() << "\n";
        
        return false;
    }

    return true;
}

bool IsUserAdmin()
{
    BOOL isAdmin = FALSE;
    PSID administratorsGroup = NULL;

    //Create a SID for the Administrators group
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    if (!AllocateAndInitializeSid(&ntAuthority, 2, 
                                   SECURITY_BUILTIN_DOMAIN_RID, 
                                   DOMAIN_ALIAS_RID_ADMINS, 
                                   0, 0, 0, 0, 0, 0, 
                                   &administratorsGroup))
    {
        std::cerr << "Failed to allocate and initialize SID. Error: " << GetLastError() << '\n';
        return false;
    }

    //Check if the token of the current process is part of the Administrators group
    if (!CheckTokenMembership(NULL, administratorsGroup, &isAdmin))
    {
        std::cerr << "Failed to check token membership. Error: " << GetLastError() << '\n';
        isAdmin = FALSE;
    }

    FreeSid(administratorsGroup);
    return isAdmin;
}

//Trying to terminate hung process and regaining mutex ownership
bool TerminateAndAcquireMutexOwnership(HWND hWnd, HANDLE hMutex)
{
    DWORD  processId;
    HANDLE hProcess;

    GetWindowThreadProcessId(hWnd, &processId);
    if(processId == 0)
    {
        std::cerr << "Failed to get process ID of the hung process.\n";
        return false;
    }

    hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, processId);
    if (!hProcess)
    {
        std::cerr << "Failed to open the hung process. Error: " << GetLastError() << '\n';
        return false;
    }

    if (!TerminateProcess(hProcess, 1))
    {
        std::cerr << "Hung process failed to be terminated. Error: " << GetLastError() << '\n';
        CloseHandle(hProcess);
        return false;
    }
    
    std::cout << "Hung process successfully terminated, trying to gain access to mutex ownership.";

    //Try to acquire ownership of the mutex
    DWORD waitResult = WaitForSingleObject(hMutex, 500);

    if (waitResult == WAIT_OBJECT_0)
    {
        std::cout << "Successfully gained the ownership of mutex.\n";
        CloseHandle(hProcess);
        return true;
    }

    std::cerr << "Failed to acquire the mutex after terminating the hung process. Error: " << GetLastError() << '\n';
    CloseHandle(hProcess);
    return false;
}

#endif
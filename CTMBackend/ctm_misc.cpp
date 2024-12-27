#include "ctm_misc.h"

/*
 * NOTE: All of them are static functions
 */

//--------------------ADMIN STUFF--------------------
bool CTMMisc::EnableOrDisablePrivilege(LPCWSTR privilegeName, bool shouldEnablePrivilege)
{
    HANDLE           hToken;
    LUID             luid;
    TOKEN_PRIVILEGES tokenPrivilege;

    if(!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
    {
        CTM_LOG_ERROR("Failed to 'OpenProcessToken' for adjusting privileges. Error code: ", GetLastError());
        return false;
    }

    if(!LookupPrivilegeValueW(NULL, privilegeName, &luid))
    {
        CTM_LOG_ERROR("Failed to 'LookupPrivilegeValueW'. Error code: ", GetLastError());
        CloseHandle(hToken);
        return false;
    }

    //Settings for token privilege
    tokenPrivilege.PrivilegeCount           = 1;
    tokenPrivilege.Privileges[0].Luid       = luid;
    tokenPrivilege.Privileges[0].Attributes = shouldEnablePrivilege ? SE_PRIVILEGE_ENABLED : 0;

    if(!AdjustTokenPrivileges(hToken, FALSE, &tokenPrivilege, sizeof(TOKEN_PRIVILEGES), NULL, NULL))
    {
        CTM_LOG_ERROR("Failed to 'AdjustTokenPrivileges'. Error code: ", GetLastError());
        CloseHandle(hToken);
        return false;
    }

    if(GetLastError() == ERROR_NOT_ALL_ASSIGNED)
    {
        CTM_LOG_ERROR("The privilege was not assigned to the client.");
        CloseHandle(hToken);
        return false;
    }

    CloseHandle(hToken);
    return true;
}

bool CTMMisc::PromptUserForAdministratorAccess()
{
    //Get the file name of current executable
    WCHAR filePath[MAX_PATH];

    if (!GetModuleFileNameW(NULL, filePath, MAX_PATH))
    {
        CTM_LOG_ERROR("Failed to get executable path. Error code: ", GetLastError());
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
            CTM_LOG_ERROR("User refused to grant administrator privileges.");
        else
            CTM_LOG_ERROR("Failed to launch application as administrator. Error code: ", GetLastError());
        
        return false;
    }

    return true;
}

bool CTMMisc::IsUserAdmin()
{
    BOOL isAdmin = FALSE;
    PSID administratorsGroup = NULL;

    //Create a SID for the Administrators group
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    if(!AllocateAndInitializeSid(&ntAuthority, 2, 
                                SECURITY_BUILTIN_DOMAIN_RID, 
                                DOMAIN_ALIAS_RID_ADMINS, 
                                0, 0, 0, 0, 0, 0, 
                                &administratorsGroup))
    {
        CTM_LOG_ERROR("Failed to allocate and initialize SID. Error code: ", GetLastError());
        return false;
    }

    //Check if the token of the current process is part of the Administrators group
    if(!CheckTokenMembership(NULL, administratorsGroup, &isAdmin))
    {
        CTM_LOG_ERROR("Failed to check token membership. Error code: ", GetLastError());
        isAdmin = FALSE;
    }

    FreeSid(administratorsGroup);
    return isAdmin;
}

//--------------------MUTEX STUFF--------------------
//Trying to terminate hung process and regaining mutex ownership
bool CTMMisc::TerminateAndAcquireMutexOwnership(HWND hWnd, HANDLE hMutex)
{
    DWORD  processId;
    HANDLE hProcess;

    GetWindowThreadProcessId(hWnd, &processId);
    if(processId == 0)
    {
        CTM_LOG_ERROR("Failed to get process id of the unresponsive application instance.");
        return false;
    }

    hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, processId);
    if(!hProcess)
    {
        CTM_LOG_ERROR("Failed to open the unresponsive application instance. Error code: ", GetLastError());
        return false;
    }

    if(!TerminateProcess(hProcess, 1))
    {
        CTM_LOG_ERROR("Failed to terminate unresponsive application instance. Error code: ", GetLastError());
        CloseHandle(hProcess);
        return false;
    }
    
    CTM_LOG_SUCCESS("Unresponsive application instance successfully terminated, trying to gain access to Mutex ownership.");

    //Try to acquire ownership of the mutex
    DWORD waitResult = WaitForSingleObject(hMutex, 2500);

    if(waitResult == WAIT_OBJECT_0)
    {
        CTM_LOG_SUCCESS("Successfully gained the ownership of Mutex.");
        CloseHandle(hProcess);
        return true;
    }

    CTM_LOG_ERROR("Failed to acquire the ownership of Mutex. Error code: ", GetLastError());
    CloseHandle(hProcess);
    return false;
}

//--------------------WINDOWS AND TERMINAL STUFF--------------------
void CTMMisc::EnableVirtualTerminalProcessing()
{
    //Before we even try to enable virtual terminal, check if its windows 10 or greater, as this feature is only available beyond 10
    if(!IsWindows10OrGreater())
    {
        CTM_LOG_WARNING("ANSI escape codes are not supported on this version of Windows.");
        return;
    }

    HANDLE stdHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    if(stdHandle == INVALID_HANDLE_VALUE)
    {
        CTM_LOG_WARNING("Failed to 'GetStdHandle', proceeding with normal output.");
        return;
    }

    DWORD dwMode = 0;
    if(!GetConsoleMode(stdHandle, &dwMode))
    {
        CTM_LOG_WARNING("Failed to 'GetConsoleMode', proceeding with normal output.");
        return;
    }

    //Enable virtual terminal processing for our color coded text output
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if(!SetConsoleMode(stdHandle, dwMode))
    {
        CTM_LOG_WARNING("Failed to 'SetConsoleMode', proceeding with normal output.");
        return;
    }
    CTM_LOG_SUCCESS("Successfully enabled virtual terminal processing, expect color coded outputs.");
}

bool CTMMisc::IsWindows10OrGreater()
{
    typedef LONG(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
    
    HMODULE hNtdll = ::GetModuleHandleW(L"ntdll.dll");
    if(!hNtdll)
        return false;

    auto RtlGetVersion = (RtlGetVersionPtr)::GetProcAddress(hNtdll, "RtlGetVersion");
    if(!RtlGetVersion)
        return false;

    RTL_OSVERSIONINFOW osvi  = { 0 };
    osvi.dwOSVersionInfoSize = sizeof(RTL_OSVERSIONINFOW);

    if(RtlGetVersion(&osvi) == 0)
        //Windows 10 or higher and build 10586 or later (1511 or later)
        return (osvi.dwMajorVersion > 10) || 
               (osvi.dwMajorVersion == 10 && osvi.dwBuildNumber >= 10586);

    return false;
}
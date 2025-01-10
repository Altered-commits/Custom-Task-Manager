/* 
 * I'm going to be using higher privileges for this app just so i can use 'OpenProcess' to collect information.
 * If i can't use 'OpenProcess', i will fallback to 'NtQuerySystemInformation' hoping it gives me information.
 * Atleast in my current system, the struct (SYSTEM_PROCESS_INFORMATION) returned by the 'NtQuerySystemInformation' has a lot of values as 'reserved'.
 * And those 'reserved' values are exactly what i want.
 * What i'm thinking is, let's just use some version of struct which has values i can use and are not 'reserved'.
 * I dont really care if it breaks compatibility stuff.
 * But the reason why i'm still using 'OpenProcess' is to avoid using 'reserved' values as much as possible.
 * Rest idk if i'm doing stuff right, i am still new to winapi and windows app programming stuff.
 * 
 * 
 * PS: I tried my absolute best to make sure this works in MSVC and MinGW.
 * 
 * 
 */

//Doing this makes 'SE_DEBUG_NAME' macro work without MinGW OR MSVC crying cuz it makes the 'TEXT' macro work, idk why
#define UNICODE

//My stuff
#include "CTMBackend/ctm_app.h"
#include "CTMBackend/ctm_misc.h"

int main(void)
{
    //Prompt user to run this process as Administrator if it isn't running as Administrator already
    if(!CTMMisc::IsUserAdmin())
    {
        if(!CTMMisc::PromptUserForAdministratorAccess())
        {
            CTM_LOG_ERROR("Failed to relaunch application with administrator privileges.");
            return 1;
        }

        return 0; //The elevated instance will take over
    }

    //Before we go ahead, try to enable virtual terminal for color coded outputs
    CTMMisc::EnableVirtualTerminalProcessing();

    //Create mutex so that i can prevent multiple instances of app
    HANDLE hMutex = CreateMutexW(NULL, TRUE, CTM_APP_MUTEX_NAME);
    
    if(hMutex == NULL)
    {
        CTM_LOG_ERROR("Failed to initialize Mutex. Error code: ", GetLastError());
        return 1;
    }

    //I need RAII
    CTMMutexGuard mutexGuard(hMutex);
    
    //Mutex already exists, check for responsiveness
    if(GetLastError() == ERROR_ALREADY_EXISTS)
    {
        CTM_LOG_INFO("An instance of this application already exists, checking for its responsiveness.");
        
        HWND hWnd = FindWindowW(CTM_APP_CLASS_NAME, NULL);

        //Window exists, we check for it's responsiveness, if it's hung, let us open another process
        if(hWnd)
        {
            //Window is responsive
            if(SendMessageTimeoutW(hWnd, WM_NULL, 0, 0, SMTO_ABORTIFHUNG, 2000, NULL))
            {
                CTM_LOG_SUCCESS("The application instance is responsive, brining it to foreground.");

                ShowWindow(hWnd, SW_RESTORE);
                SetForegroundWindow(hWnd);

                //Close the reference of mutex, OnlyCloseHandle sets hMutex to nullptr so the mutexGuard no longer has the ref to it
                mutexGuard.OnlyCloseHandle();
                return 1;
            }
            //Window is unresponsive, try to terminate the hung process and gain access to mutex ownership
            else
            {
                CTM_LOG_INFO("The application instance is unresponsive, attempting to terminate that app and acquire its Mutex.");

                //YAY WE GOT LUCKY
                if(CTMMisc::TerminateAndAcquireMutexOwnership(hWnd, hMutex))
                    CTM_LOG_SUCCESS("Acquired Mutex and terminated the hung application, proceeding with normal initialization.");
                //Just give up at this point, cuz i give up, nothing works, this apps trash beyond trash then
                else
                {
                    CTM_LOG_ERROR("Critical Error, nothing seems to work. I or Windows can no longer help you.");
                    mutexGuard.OnlyCloseHandle();
                    return 1;
                }
            }
        }
        else
            CTM_LOG_WARNING("Failed to find an application instance, allowing this instance to initialize UNDER the assumption that no other instance exists.");
    }

    //Let's get the good stuff
    if(!CTMMisc::EnableOrDisablePrivilege(SE_DEBUG_NAME))
        CTM_LOG_WARNING("Failed to enable 'SeDebugPrivilege'. Proceeding with less information.");
    else
        CTM_LOG_SUCCESS("Successfully enabled 'SeDebugPrivilege' :)");

    //The actual application
    CTMApp app;
    CTMAppErrorCodes code = app.Initialize();

    if(code != CTMAppErrorCodes::InitSuccess)
    {
        CTM_LOG_ERROR("Failed to initialize CTM (Custom Task Manager). Error code: ", static_cast<int>(code));
        //Failed to launch the app, CTMMutexGuard automatically releases mutex due to RAII
        return 1;
    }
    
    app.Run();

    //Successfully closed the app, CTMMutexGuard automatically releases mutex due to RAII
    return 0;
}
/* 
 * I'm going to be using higher privileges for this app just so i can use 'OpenProcess' to collect information.
 * If i can't use 'OpenProcess', i will fallback to 'NtQuerySystemInformation' hoping it gives me information.
 * Atleast in my current system, the struct (SYSTEM_PROCESS_INFORMATION) returned by the 'NtQuerySystemInformation' has a lot of values as 'reserved'.
 * And those 'reserved' values are exactly what i want.
 * What i'm thinking is, let's just use some version of struct which has values i can use and are not 'reserved'.
 * I dont really care if it breaks compatibility stuff.
 * But the reason why i'm still using 'OpenProcess' is to avoid using 'reserved' values as much as possible.
 * Rest idk if i'm doing stuff right, i am still new to winapi and windows app programming stuff.
 */

#include <iostream> //For debugging
#include "CTMBackend/ctm_app.h"
#include "CTMBackend/ctm_misc.h"

int main(void)
{
    //Prompt user to run this process as Administrator if it isn't running as Administrator already
    if(!IsUserAdmin())
    {
        if(!PromptUserForAdministratorAccess())
        {
            std::cerr << "Failed to relaunch with administrator privileges. Exiting.\n";
            return 1;
        }

        return 0; //The elevated instance will take over
    }

    //Create mutex so that i can prevent multiple instances of app
    HANDLE hMutex = CreateMutexW(NULL, TRUE, CTM_APP_MUTEX_NAME);
    
    if(hMutex == NULL)
    {
        std::cerr << "Failed to initialize mutex. Error code: " << GetLastError() << '\n';
        return 1;
    }

    //I need RAII
    MutexGuard mutexGuard(hMutex);
    
    //Mutex already exists, check for responsiveness
    if(GetLastError() == ERROR_ALREADY_EXISTS)
    {
        std::cerr << "An instance of this app already exists, checking for responsiveness.\n";
        
        HWND hWnd = FindWindowW(CTM_APP_CLASS_NAME, NULL);

        //Window exists, we check for it's responsiveness, if it's hung, let us open another process
        if(hWnd)
        {
            //Window is responsive
            if(SendMessageTimeoutW(hWnd, WM_NULL, 0, 0, SMTO_ABORTIFHUNG, 2000, NULL))
            {
                ShowWindow(hWnd, SW_RESTORE);
                SetForegroundWindow(hWnd);    

                //Close the reference of mutex, OnlyCloseHandle sets hMutex to nullptr so the mutexGuard no longer has the ref to it
                mutexGuard.OnlyCloseHandle();
                return 1;
            }
            //Window is unresponsive, try to terminate the hung process and gain access to mutex ownership
            else
            {
                std::cout << "Window is unresponsive, attempting to terminate the process and acquire mutex.\n";

                //YAY WE GOT LUCKY
                if(TerminateAndAcquireMutexOwnership(hWnd, hMutex))
                    std::cout << "Proceeding with app initialization.\n";
                //Just give up at this point, cuz i give up, nothing works, this apps trash beyond trash then
                else
                {
                    std::cerr << "Just give up at this point. I or Windows can no longer help you.\n";
                    mutexGuard.OnlyCloseHandle();
                    return 1;
                }
            }
        }
        else
            std::cerr << "Failed to find the window, allowing this instance to run, assuming no other instance exists.\n";
    }

    //Let's get the good stuff (top privileges)
    if(!EnableOrDisablePrivilege(L"SeDebugPrivilege")) //Having to type it instead of using SE_DEBUG_NAME cuz GCC is crying
        std::cerr << "Failed to enable 'SeDebugPrivilege'. Proceeding with less information.\n";
    else
        std::cout << "Succeeded in enabling 'SeDebugPrivilege'. I am not sure how safe this truly is, good luck.\n";

    //The actual application
    CTMApp app;
    CTMAppErrorCodes code = app.Initialize();

    if(code != CTMAppErrorCodes::INIT_SUCCESS) {
        std::cerr << "Failed to intialize custom task manager. Error code: " << static_cast<int>(code) << '\n';
        //Failed to launch the app, MutexGuard automatically releases mutex due to RAII
        return 1;
    }

    app.Run();

    //Successfully closed the app, MutexGuard automatically releases mutex due to RAII
    return 0;
}
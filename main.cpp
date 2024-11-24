#include <iostream> //For debugging
#include "CTMBackend/ctm_app.h"

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

int main(void)
{
    HANDLE hMutex = CreateMutexW(NULL, TRUE, CTM_APP_MUTEX_NAME);
    
    if(hMutex == NULL)
    {
        std::cerr << "Failed to initialize mutex. Error code: " << GetLastError() << '\n';
        return 1;
    }

    //Create a custom MutexGuard
    MutexGuard mutexGuard(hMutex);
    
    //Mutex already exists, 
    if(GetLastError() == ERROR_ALREADY_EXISTS)
    {
        std::cerr << "An instance of this app already exists, bringing it to foreground.\n";
        
        HWND hwnd = FindWindowW(CTM_APP_CLASS_NAME, NULL);

        if(!hwnd)
            std::cerr << "Failed to bring app to foreground.\n";
        else {
            ShowWindow(hwnd, SW_RESTORE); //If minimized, restore it
            SetForegroundWindow(hwnd);
        }

        //Close the reference of mutex, OnlyCloseHandle sets hMutex to nullptr so the mutexGuard no longer has the ref to it
        mutexGuard.OnlyCloseHandle();
        return 1;
    }

    CTMApp app;
    CTMAppErrorCodes code = app.Initialize();

    if(code != CTMAppErrorCodes::INIT_SUCCESS) {
        std::cerr << "Failed to intialize custom task manager. Error code: " << (int)code << '\n';
        //Failed to launch the app, MutexGuard automatically releases mutex due to raii
        return 1;
    }

    app.Run();

    //Successfully closed the app, MutexGuard automatically releases mutex due to raii
    return 0;
}
#ifndef CTM_STARTUP_APPS_SCREEN_HPP
#define CTM_STARTUP_APPS_SCREEN_HPP

//Windows stuff
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
//ImGui stuff
#include "../../ImGUI/imgui.h"
//My stuff
#include "../ctm_misc.h"
#include "../CTMPureHeaderFiles/ctm_base_state.h"
#include "../CTMPureHeaderFiles/ctm_logger.h"
#include "../CTMGlobalManagers/ctm_state_manager.h"
//Stdlib stuff
#include <string>
#include <vector>
#include <memory>
#include <cstring>

//
struct FileHandle
{
    HANDLE handle;
    FileHandle(HANDLE handle)
        : handle(handle)
    {}
    ~FileHandle()
    {
        if(handle != INVALID_HANDLE_VALUE)
            FindClose(handle);
    }
};
//
enum class StartupAppType { StartupRegistry, StartupFolder };
//
struct StartupAppInfo
{
    char           startupAppName[32]; //Any name greater than 32 bytes will be truncated (... will be added at the end)
    StartupAppType startupAppType;
    std::string    startupAppPath;     //Path to the app itself

    StartupAppInfo(StartupAppType type, std::string&& path)
        : startupAppType(type), startupAppPath(std::move(path))
    {}
};

/*
 * NOTE: I am not considering all the possible scenarios (where startup apps might be stored)
 * I'm only considering these things:
 *  1) Registry Startup (That too i am missing two stuff)
 *  2) Folder Startup (That too only from C:\ProgramData\Microsoft\Windows\Start Menu\Programs\Startup)
 */

class CTMStartupAppsScreen : public CTMBaseScreen
{
public:
    CTMStartupAppsScreen();
    ~CTMStartupAppsScreen() override;

protected:
    void OnRender() override;
    void OnUpdate() override;

private: //Constructor functions
    bool CTMConstructorGetLastBIOSTime();
    bool CTMConstructorGetStartupApps();
    bool CTMConstructorGetStartupAppForKey(HKEY, const char*);
    bool CTMConstructorGetStartupAppFromFolder();

private: //Helper functions
    constexpr const char* StartupAppStringRepr(StartupAppType);
    void                  CopyStringToBufferTruncated(const char*, std::size_t, char*, std::size_t);

private: //Last BIOS time stuff
    float lastBIOSTime = 0.0f;

private: //Variables storing startup info
    std::vector<StartupAppInfo> startupAppVector;
};

#endif
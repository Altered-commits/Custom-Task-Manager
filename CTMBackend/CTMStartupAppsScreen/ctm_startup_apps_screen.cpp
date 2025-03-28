#include "ctm_startup_apps_screen.h"

//Equivalent to OnInit function
CTMStartupAppsScreen::CTMStartupAppsScreen()
{
    //We can be a bit lenient uk..
    if(!CTMConstructorGetLastBIOSTime())
        CTM_LOG_WARNING("Failed to get last BIOS time. Expect value to be 0!");

    if(!CTMConstructorGetStartupApps())
        return;
    
    if(!CTMConstructorGetStartupAppFromFolder())
        return;
    
    SetInitialized(true);
}

//Equivalent to OnClean function
CTMStartupAppsScreen::~CTMStartupAppsScreen()
{
    SetInitialized(false);
}

//--------------------CONSTRUCTOR INIT FUNCTIONS--------------------
bool CTMStartupAppsScreen::CTMConstructorGetLastBIOSTime()
{
    HKEY key;
    const char* subKey = "SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Power";
    LSTATUS status = RegOpenKeyExA(HKEY_LOCAL_MACHINE, subKey,
                        0, KEY_READ, &key);

    if(status != ERROR_SUCCESS)
    {
        CTM_LOG_ERROR("Failed to open key: ", subKey, " for reading last BIOS time. Error code: ", status);
        return false;
    }

    //Data required to get last BIOS time
    DWORD lastBIOSTimeMs     = 0;
    DWORD lastBIOSTimeMsSize = sizeof(lastBIOSTimeMs);
    DWORD lastBIOSTimeMsType = REG_DWORD;

    //The BIOS time value is in key 'FwPOSTTime'
    status = RegQueryValueExA(key, "FwPOSTTime", nullptr, &lastBIOSTimeMsType,
                    reinterpret_cast<LPBYTE>(&lastBIOSTimeMs), &lastBIOSTimeMsSize);

    if(status != ERROR_SUCCESS)
    {
        CTM_LOG_ERROR("Failed to get last BIOS time value. Error code: ", status);
        return false;
    }

    //Convert the value to seconds, its in milliseconds by default
    lastBIOSTime = static_cast<double>(lastBIOSTimeMs) / 1000.0;
    return true;
}

bool CTMStartupAppsScreen::CTMConstructorGetStartupApps()
{
    //Query both user and system-wide startup entries
    if(!CTMConstructorGetStartupAppForKey(HKEY_CURRENT_USER,  "Software\\Microsoft\\Windows\\CurrentVersion\\Run"))
        return false;
    
    if(!CTMConstructorGetStartupAppForKey(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows\\CurrentVersion\\Run"))
        return false;

    return true;
}

bool CTMStartupAppsScreen::CTMConstructorGetStartupAppForKey(HKEY hKey, const char* subKey)
{
    HKEY key;
    LSTATUS status = RegOpenKeyExA(hKey, subKey, 0, KEY_READ, &key);
    if(status != ERROR_SUCCESS)
    {
        CTM_LOG_ERROR("Failed to open key: ", subKey, " for reading startup app info. Error code: ", status);
        return false;
    }

    DWORD index = 0;
    CHAR  valueName[64]; //For storing the startup app name
    BYTE  data[128];     //For storing the startup app data

    //Loop thru all the entries
    while(true)
    {
        DWORD valueNameSize = sizeof(valueName) / sizeof(CHAR), //Size for storing startup app name
              dataSize      = sizeof(data),                     //Size for storing startup app data
              type          = 0;                                //Returned type

        //Get the details        
        if(RegEnumValueA(key, index, valueName, &valueNameSize, NULL, &type, data, &dataSize) != ERROR_SUCCESS)
            break;

        //If the type is a string, then only we add it to our vector
        if(type == REG_SZ)
        {
            //Could make this outside of loop but for now yeah (unoptimized but works for now)
            StartupAppInfo startupAppInfo{StartupAppType::StartupRegistry, reinterpret_cast<char*>(data)};
            
            //Copy the name (truncated if needed) directly to the member of StartupAppInfo
            CopyStringToBufferTruncated(valueName, valueNameSize, startupAppInfo.startupAppName, 32);

            startupAppVector.emplace_back(std::move(startupAppInfo));
        }

        index++;
    }

    //Close the key, we are done
    RegCloseKey(key);
    return true;
}

bool CTMStartupAppsScreen::CTMConstructorGetStartupAppFromFolder()
{
    //Store the path of the startup folder
    PWSTR   path = nullptr;
    HRESULT hr   = SHGetKnownFolderPath(FOLDERID_CommonStartup, 0, NULL, &path);

    if(FAILED(hr))
    {
        CTM_LOG_ERROR("Failed to get known folder path for common startup applications. Error code: ", hr);
        return false;
    }

    //Convert path (WString) to String
    char multiBytePath[128];
    if(!CTMMisc::WSToSWithEllipsisTruncation(multiBytePath, path, //Quite a bad, HORRIBLE IDEA TO TRUNCATE PATH WITH ...
        sizeof(multiBytePath), wcslen(path)))                     //If anyone gets some stupid error its due to this ...
    {
        CTM_LOG_ERROR("Failed to convert path (Wide String) to (String). No Startup Type of folder will be shown.");
        return false;
    }

    //Cool, we no longer need the path as we copied it to our buffer. Free it
    CoTaskMemFree(path);

    //Build search path (append "\*") to enumerate all files (can be optimized, yeah)
    std::string searchPath{multiBytePath};
    searchPath.append("\\*");

    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
    if(hFind == INVALID_HANDLE_VALUE)
    {
        CTM_LOG_ERROR("Failed to enumerate through the common startup folder.");
        CTM_LOG_TEXT("Path (If malformed, its completely my fault): ", searchPath);
        return false;
    }
    FileHandle handle{hFind};

    //Enumerate thru all the files within the folder
    do {
        //Skip "." and ".."
        if(
            strcmp(findData.cFileName, ".") == 0 ||
            strcmp(findData.cFileName, "..") == 0
        )
            continue;
        
        //Use std::wstring_view for efficient substring operations
        std::string_view fileName(static_cast<PCHAR>(findData.cFileName));
        if(fileName.size() >= 4 && fileName.substr(fileName.size() - 4) == ".lnk")
        {
            //Create StartupInfo
            StartupAppInfo startupAppInfo{StartupAppType::StartupFolder, searchPath.c_str()};
            strncpy(startupAppInfo.startupAppName, findData.cFileName,
                sizeof(startupAppInfo.startupAppName));
            
            startupAppVector.emplace_back(std::move(startupAppInfo));
        }
    }
    while(FindNextFileA(handle.handle, &findData));

    //We are done
    return true;
}

//--------------------MAIN RENDER AND UPDATE FUNCTIONS--------------------
void CTMStartupAppsScreen::OnRender()
{
    //Print the last boot time like that of Task Manager
    ImGui::Text("Last BIOS Time: %.2f sec", lastBIOSTime);
    ImGui::Dummy({0, 12});

    //Table repr of startup apps
    if(ImGui::BeginTable("StartupAppTable", 3, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollX))
    {
        ImGui::TableSetupColumn("Startup App");
        ImGui::TableSetupColumn("Startup Type");
        ImGui::TableSetupColumn("Startup Path");
        ImGui::TableHeadersRow();
        
        //Loop thru all the apps and display them
        for(auto&& startupApp : startupAppVector)
        {
            //New row
            ImGui::TableNextRow();

            //Startup App Name Column
            ImGui::TableSetColumnIndex(0);
            ImGui::Text(startupApp.startupAppName);
            
            //Startup App Type Column
            ImGui::TableSetColumnIndex(1);
            ImGui::Text(StartupAppStringRepr(startupApp.startupAppType));

            //Startup App Path Column
            ImGui::TableSetColumnIndex(2);
            ImGui::Text(startupApp.startupAppPath.c_str());
        }

        ImGui::EndTable();
    }
}

//Not needed
void CTMStartupAppsScreen::OnUpdate() {}

//--------------------HELPER FUNCTIONS--------------------
constexpr const char* CTMStartupAppsScreen::StartupAppStringRepr(StartupAppType type)
{
    switch(type)
    {
        case StartupAppType::StartupRegistry:
            return "Registry";
        
        case StartupAppType::StartupFolder:
            return "Folder";
        
        default:
            return "Unknown";
    }
}

void CTMStartupAppsScreen::CopyStringToBufferTruncated(const char* string, std::size_t stringLength,
    char* outputBuffer, std::size_t maxOutputBufferLength)
{
    /* IMP: No error checking */

    //If the stringLength is >= the size of maxOutputBufferLength-
    //-that means that u cannot even put a null terminator at the end. Truncate it (...)

    //If we are truncating, then the last 4 bytes of the outputBuffer should be reserved for (...\0)
    //Else we can copy it as is appending the null terminator ourseleves

    bool        shouldTruncate   = stringLength >= maxOutputBufferLength;
    std::size_t maxCopyableBytes = shouldTruncate ? maxOutputBufferLength - 4 : stringLength;

    //Copy whatever string we can copy
    std::strncpy(outputBuffer, string, maxCopyableBytes);

    if(shouldTruncate)
        std::strcpy(outputBuffer + maxCopyableBytes, "...\0");
    else
        outputBuffer[maxCopyableBytes] = '\0';
}

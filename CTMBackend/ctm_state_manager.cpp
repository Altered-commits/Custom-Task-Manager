#include "ctm_state_manager.h"

//On constructor load all settings
CTMStateManager::CTMStateManager()
{
    LoadSettings();
}
//On destructor save all settings
CTMStateManager::~CTMStateManager()
{
    SaveSettings();
}

//--------------------MISC MANAGER--------------------
void CTMStateManager::SetWindowHandle(HWND externalWndHandle)
{
    windowHandle = externalWndHandle;
}

HWND CTMStateManager::GetWindowHandle()
{
    return windowHandle;
}

void CTMStateManager::SetIsPerfScreen(bool externalIsPerfScreen)
{
    isPerfScreen = externalIsPerfScreen;
}
    
bool CTMStateManager::GetIsPerfScreen()
{
    return isPerfScreen;
}

//--------------------FONT MANAGER--------------------
bool CTMStateManager::AddFont(const char *fontPath, float fontSize)
{
    //Having to do ImGui::GetIO instead of storing it as a ref by initializing it in constructor cuz-
    //-chances of 'this' constructor getting called before setup ImGui are high. Example: Look in ctm_app_content.h constructor
    //Also it won't be much of a overhead honestly
    ImFont* loadedFont = ImGui::GetIO().Fonts->AddFontFromFileTTF(fontPath, fontSize);
    if(!loadedFont)
        return false;

    fonts.emplace_back(loadedFont);
    return true;
}

ImFont *CTMStateManager::GetFont(size_t index)
{
    if(index < fonts.size())
        return fonts[index];
    return nullptr;
}

//--------------------SETTINGS MANAGER HELPER FUNCTIONS--------------------
void CTMStateManager::LoadSettings()
{
    //Open the file
    std::ifstream inFile(iniFileName);
    //Check if were able to open the file, if failed, return
    if(!inFile.is_open())
        return;
    
    //We opened the file, read all the stuff
    std::string line;
    while(std::getline(inFile, line))
    {
        auto eqIndex = line.find('=');
        //We found an eq sign, left side is the key, right side is the value
        if(eqIndex != std::string::npos)
        {
            std::string key = line.substr(0, eqIndex);
            std::string val = line.substr(eqIndex + 1);

            settingsMap[std::move(key)] = std::move(val);
        }
    }
}

void CTMStateManager::SaveSettings()
{
    //Open the file to write
    std::ofstream outFile(iniFileName);
    //We failed to open the file, simply return
    if(!outFile.is_open())
        return;
    
    //We opened the file, write the unordered map to it with a newline at the end
    for (auto &&[key, value] : settingsMap)
        outFile << key << '=' << value << '\n';
}

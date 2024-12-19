#ifndef CTM_STATE_MANAGER_HPP
#define CTM_STATE_MANAGER_HPP

/*
 * This class is a 'Singleton'. This will manage all the states (Settings, Fonts, etc) across the whole app.
 * To be strictly used after initializing ImGui (NOT BEFORE)
 */

//ImGui stuff
#include "../ImGUI/imgui.h"
//Stdlib stuff
#include <vector>
#include <unordered_map>
#include <string>
#include <fstream>
#include <sstream>
#include <charconv>
//My stuff
#include "ctm_constants.h"

//So i don't make errors in getting values
enum class CTMSettingKey
{
    //ScreenState is basically, which screen did u last visit before shutting down (Settings, Processes, etc)
    ScreenState,

    //Display related settings
    DisplayTheme
};

//Makes my life EASIER
using SettingsMap = std::unordered_map<std::string, std::string>;

class CTMStateManager
{
public:
    //Straight forward way to get a singleton instance
    static CTMStateManager& getInstance()
    {
        static CTMStateManager instance;
        return instance;
    }

    //--------------------FONT MANAGER--------------------
    bool    AddFont(const char* fontPath, float fontSize);
    ImFont* GetFont(size_t index);

    //--------------------SETTINGS MANAGER--------------------
    template<typename T> T    getSetting(CTMSettingKey, T);
    template<typename T> void setSetting(CTMSettingKey, T);

    void ApplySettings();
    void ApplyDisplayTheme(int);
    
private: //Settings manager helper functions
    void LoadSettings();
    void SaveSettings();

private: //Constructors and Destructors
    CTMStateManager();
    ~CTMStateManager();

    //No need for copy or move operations
    CTMStateManager(const CTMStateManager&)            = delete;
    CTMStateManager& operator=(const CTMStateManager&) = delete;
    CTMStateManager(CTMStateManager&&)                 = delete;
    CTMStateManager& operator=(CTMStateManager&&)      = delete;

private: //Font variable
    std::vector<ImFont*> fonts;
private: //Settings variables
    const char* iniFileName = "CTMSettings.ini";
    SettingsMap settingsMap;
    //String repr of 'CTMSettingKey' enum, internal to this class
    constexpr static const char* CTMSettingKeyStringRepr[] = { "CTMScreenState", "CTMDisplayTheme" };
};

//--------------------SETTINGS MANAGER (TEMPLATED FUNCTIONS)--------------------
template<typename T>
T CTMStateManager::getSetting(CTMSettingKey settingKeyEnum, T defaultSettingValue)
{
    //Get the string repr of setting key
    const char* settingKey = CTMSettingKeyStringRepr[static_cast<int>(settingKeyEnum)];

    //Try to find the key in the settings map, if found, convert it to 'T' data type and return it
    if(auto it = settingsMap.find(settingKey); it != settingsMap.end())
    {
        const std::string& settingValueString = it->second;
        T settingValue{};
        //If its an arithmetic type, simply use std::from_chars
        if constexpr(std::is_arithmetic_v<T>)
        {
            auto[ptr, err] = std::from_chars(settingValueString.data(), settingValueString.data() + settingValueString.size(), settingValue);
            if(err == std::errc())
                return settingValue;
        }
        else //Fallback to using streams
            if(std::istringstream(settingValueString) >> settingValue)
                return settingValue; 
    }
    return defaultSettingValue;
}

template<typename T>
void CTMStateManager::setSetting(CTMSettingKey settingKeyEnum, T settingValue)
{
    //Get the string repr of setting key
    const char* settingKey = CTMSettingKeyStringRepr[static_cast<int>(settingKeyEnum)];
    //Use streams to convert 'T' data type to string
    std::ostringstream oss;
    oss << settingValue;
    //std::move conveys the intent alot more clearer
    settingsMap[settingKey] = std::move(oss.str());
}

#endif
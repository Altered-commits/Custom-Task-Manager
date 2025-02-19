#ifndef CTM_SETTINGS_MENU_HPP
#define CTM_SETTINGS_MENU_HPP

//Windows stuff
#include <windows.h>
//ImGui & ImPlot stuff
#include "../../ImGUI/imgui.h"
#include "../../ImPlot/implot.h"
//My stuff
#include "../ctm_base_state.h"
#include "../CTMGlobalManagers/ctm_state_manager.h"
//Stdlib stuff
#include <functional>

//'using' stuff makes my life easier
using ComboBoxOnChangeFuncPtr = std::function<void(int)>;

class CTMSettingsScreen : public CTMBaseScreen
{
public:
    CTMSettingsScreen();
    ~CTMSettingsScreen() override;

public: //Static functions (to be/can be) used internally or externally by other classes
        //Not all classes need functions to handle stuff, some classes can simply just use settings value directly
        //To be strictly used after initializing ImGui
    static void ApplyDisplaySettings();
    static void ApplyDisplayThemeSetting(int);
    static void ApplyDisplayModeSetting(int);

protected:
    void OnRender() override;
    void OnUpdate() override {} //No need for this

private: //Helper functions
    void RenderSectionTitle(const char*, ImVec2&);
    void RenderComboBox(const char*, const char*, const char**, int, int&, const ImVec2&, float, float, ComboBoxOnChangeFuncPtr = nullptr);

private: //Pointer to the State Manager singleton
    CTMStateManager& stateManager = CTMStateManager::GetInstance();

private: //Settings variables
    //----------Theme section----------
    int         currentThemeIndex  = 0;
    const char* themes[3]          = { "Dark", "Light", "Classic" };
    int         currentDisplayMode = 0;
    const char* dispModes[2]       = { "Normal", "Fullscreen" };
    
    //----------Page section----------
    constexpr static int mainPageCount    = static_cast<int>(CTMScreenState::PageCount);
    constexpr static int perfPageCount    = static_cast<int>(CTMPerformanceScreenState::PageCount);
    //
    int                  currentPageIndex = static_cast<int>(CTMScreenState::Settings);
    int                  currentPerfIndex = static_cast<int>(CTMPerformanceScreenState::CpuInfo);
    //
    const char*          mainPages[mainPageCount] = { "Processes", "Performance", "Apps", "Services", "Settings" };
    const char*          perfPages[perfPageCount] = { "CPU", "Memory", "Network", "Disk" };

private: //Common variables
    const float comboBoxWidth     = 230.0f;
};

#endif
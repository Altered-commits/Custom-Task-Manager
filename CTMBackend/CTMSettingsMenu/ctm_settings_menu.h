#ifndef CTM_SETTINGS_MENU_HPP
#define CTM_SETTINGS_MENU_HPP

//ImGui stuff
#include "../../ImGUI/imgui.h"
//My stuff
#include "../ctm_base_state.h"
#include "../ctm_state_manager.h"
//Stdlib stuff
#include <functional>

//'using' stuff makes my life easier
using ComboBoxOnChangeFuncPtr = std::function<void(int)>;

class CTMSettingsMenu : public CTMBaseState
{
public:
    CTMSettingsMenu();
    ~CTMSettingsMenu() override;

protected:
    void OnRender() override;
    void OnUpdate() override {} //No need for this

private: //Helper functions
    void RenderSectionTitle(const char*, ImVec2&);
    void RenderComboBox(const char*, const char*, const char**, int, int&, const ImVec2&, float, float, ComboBoxOnChangeFuncPtr);

private: //Pointer to the state manager singleton
    CTMStateManager& stateManager = CTMStateManager::getInstance();

private: //Settings variables
    //----------Theme section----------
    int         currentThemeIndex = 0; //Loaded in constructor
    const char* themes[3]         = { "Dark", "Light", "Classic" };
    
    //----------Page section----------
    int         currentPageIndex   = 0;
    const char* pages[4]           = { "Processes", "Apps", "Services", "Settings" };

private: //Common variables
    const float comboBoxWidth     = 200.0f;
};

#endif
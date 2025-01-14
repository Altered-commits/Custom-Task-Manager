#include "ctm_settings_screen.h"

//Equivalent to OnInit function
CTMSettingsScreen::CTMSettingsScreen()
{
    //Get the values saved in settings (if the settings ini exists)
    //Display Settings
    currentThemeIndex  = stateManager.getSetting(CTMSettingKey::DisplayTheme, currentThemeIndex);
    currentDisplayMode = stateManager.getSetting(CTMSettingKey::DisplayMode, currentDisplayMode);
    //Page Settings
    currentPageIndex   = stateManager.getSetting(CTMSettingKey::ScreenState, currentPageIndex);
    currentPerfIndex   = stateManager.getSetting(CTMSettingKey::PerfState, currentPerfIndex);

    SetInitialized(true);
}

//Equivalent to OnClean function
CTMSettingsScreen::~CTMSettingsScreen()
{
    //Save all the settings before exiting this menu
    //Display Settings
    stateManager.setSetting(CTMSettingKey::DisplayTheme, currentThemeIndex);
    stateManager.setSetting(CTMSettingKey::DisplayMode, currentDisplayMode);
    //Page Settings
    stateManager.setSetting(CTMSettingKey::ScreenState, currentPageIndex);
    stateManager.setSetting(CTMSettingKey::PerfState, currentPerfIndex);

    SetInitialized(false);
}

//--------------------MAIN RENDER AND UPDATE FUNCTIONS--------------------
void CTMSettingsScreen::OnRender()
{
    ImVec2 screenSize    = ImGui::GetWindowSize();
    ImVec2 screenPadding = ImGui::GetStyle().WindowPadding;
    
    //Section 1: Theme
    RenderSectionTitle("Default Display Settings", screenSize);
    RenderComboBox("Display Theme", "##DefaultThemeDropdown", themes, IM_ARRAYSIZE(themes), currentThemeIndex, screenSize,
                    comboBoxWidth, screenPadding.x, ApplyDisplayThemeSetting);
    RenderComboBox("Display Mode", "##DefaultDisplayMode", dispModes, IM_ARRAYSIZE(dispModes), currentDisplayMode, screenSize,
                    comboBoxWidth, screenPadding.x, ApplyDisplayModeSetting);
    
    //Bit of spacing between above section and below section
    ImGui::Dummy({0, 20.0f});
    //Section 2: Page settings
    RenderSectionTitle("Default Page Settings", screenSize);
    RenderComboBox("Startup Page", "##DefaultStartupPage", mainPages, mainPageCount, currentPageIndex, screenSize,
                    comboBoxWidth, screenPadding.x);
    RenderComboBox("Performance Page", "##DefaultPerformancePage", perfPages, perfPageCount, currentPerfIndex, screenSize,
                    comboBoxWidth, screenPadding.x);
}

//--------------------HELPER FUNCTIONS--------------------
void CTMSettingsScreen::RenderSectionTitle(const char* sectionTitle, ImVec2& screenSize)
{
    float textWidth = ImGui::CalcTextSize(sectionTitle).x;
    //Center align the text6
    ImGui::SetCursorPosX((screenSize.x - textWidth) * 0.5f);
    ImGui::Text(sectionTitle);
    
    //Draw a line below the title
    ImGui::Separator();
}

void CTMSettingsScreen::RenderComboBox(const char* text, const char* label, const char** items, int itemCount, int& currentIndex,
        const ImVec2& screenSize, float comboBoxWidth, float comboBoxPadding, ComboBoxOnChangeFuncPtr onChange)
{
    //Calculate the X position to align the combo box to the right
    float comboBoxXPos = screenSize.x - comboBoxWidth - comboBoxPadding;

    ImGui::Text(text);
    ImGui::SameLine(comboBoxXPos);
    //Set the width of the combo box
    ImGui::PushItemWidth(comboBoxWidth);

    if(ImGui::BeginCombo(label, items[currentIndex]))
    {
        for(int i = 0; i < itemCount; i++)
        {
            bool isSelected = (currentIndex == i);
            if(ImGui::Selectable(items[i], isSelected))
            {
                currentIndex = i;

                if (onChange && onChange)
                    onChange(i);
            }
            if(isSelected)
                ImGui::SetItemDefaultFocus();
            
            //Spacing between each 'Selectable'
            ImGui::Spacing();
        }
        ImGui::EndCombo();
    }
    ImGui::PopItemWidth();
}

//--------------------STATIC FUNCTIONS--------------------
void CTMSettingsScreen::ApplyDisplaySettings()
{
    //Get the State Manager again because this is static function, can't access the one inside the class cuz it ain't static
    CTMStateManager& stateManager = CTMStateManager::GetInstance();
    //We only set Display Settings, Page Settings are handled by 'CTMAppContent' class itself in its constructor
    ApplyDisplayThemeSetting(stateManager.getSetting(CTMSettingKey::DisplayTheme, 2));
    ApplyDisplayModeSetting(stateManager.getSetting(CTMSettingKey::DisplayMode, 0));
}

void CTMSettingsScreen::ApplyDisplayThemeSetting(int themeMode)
{
    switch (themeMode)
    {
        case 0:
            ImGui::StyleColorsDark();
            ImPlot::StyleColorsDark();
            break;
        case 1:
            ImGui::StyleColorsLight();
            ImPlot::StyleColorsLight();
            break;
        case 2:
            ImGui::StyleColorsClassic();
            ImPlot::StyleColorsClassic();
            break;
    }
}

void CTMSettingsScreen::ApplyDisplayModeSetting(int displayMode)
{
    //Get HWND from State Manager. We set it in [ctm_app.cpp -> 'SetupCTMSettings()']
    //According to display mode (0 -> Normal, 1 -> Maximize)
    ShowWindow(CTMStateManager::GetInstance().GetWindowHandle(), displayMode == 0 ? SW_NORMAL : SW_MAXIMIZE);
}
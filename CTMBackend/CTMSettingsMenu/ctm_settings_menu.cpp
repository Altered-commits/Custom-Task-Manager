#include "ctm_settings_menu.h"

//Equivalent to OnInit function
CTMSettingsMenu::CTMSettingsMenu()
{
    //Get the values saved in settings (if the settings ini exists)
    currentThemeIndex = stateManager.getSetting(CTMSettingKey::DisplayTheme, 0);
    currentPageIndex  = stateManager.getSetting(CTMSettingKey::ScreenState, 3);
    SetInitialized(true);
}

//Equivalent to OnClean function
CTMSettingsMenu::~CTMSettingsMenu()
{
    //Save all the settings before exiting this menu
    stateManager.setSetting(CTMSettingKey::DisplayTheme, currentThemeIndex);
    stateManager.setSetting(CTMSettingKey::ScreenState, currentPageIndex);
    SetInitialized(false);
}

//--------------------MAIN RENDER AND UPDATE FUNCTIONS--------------------
void CTMSettingsMenu::OnRender()
{
    ImVec2 screenSize    = ImGui::GetWindowSize();
    ImVec2 screenPadding = ImGui::GetStyle().WindowPadding;
    
    //Section 1: Theme
    RenderSectionTitle("Theme Settings", screenSize);
    ImGui::Spacing();
    RenderComboBox("Display Theme", "##ThemeDropdown", themes, IM_ARRAYSIZE(themes), currentThemeIndex, screenSize,
                    comboBoxWidth, screenPadding.x, [this](int themeIndex){ stateManager.ApplyDisplayTheme(themeIndex); });
    
    //Bit of spacing between above section and below section
    ImGui::Dummy({0, 10.0f});
    //Section 2: Page settings
    RenderSectionTitle("Page Settings", screenSize);
    ImGui::Spacing();
    RenderComboBox("Default startup page", "##DefaultStartupPage", pages, IM_ARRAYSIZE(pages), currentPageIndex, screenSize,
                    comboBoxWidth, screenPadding.x);
}

//--------------------HELPER FUNCTIONS--------------------
void CTMSettingsMenu::RenderSectionTitle(const char* sectionTitle, ImVec2& screenSize)
{
    float textWidth = ImGui::CalcTextSize(sectionTitle).x;
    //Center align the text6
    ImGui::SetCursorPosX((screenSize.x - textWidth) * 0.5f);
    ImGui::Text(sectionTitle);
    
    //Draw a line below the title
    ImGui::Separator();
}

void CTMSettingsMenu::RenderComboBox(const char* text, const char* label, const char** items, int itemCount, int& currentIndex,
        const ImVec2& screenSize, float comboBoxWidth, float comboBoxPadding, ComboBoxOnChangeFuncPtr onChange = nullptr)
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

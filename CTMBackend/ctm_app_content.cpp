#include "ctm_app_content.h"

CTMAppContent::CTMAppContent()
{
    //Initialize a default screen on startup
    switchScreen(static_cast<CTMScreenState>(
        CTMStateManager::getInstance().getSetting(CTMSettingKey::ScreenState, static_cast<int>(CTMScreenState::Settings))
    ));
}

//--------------------SCREEN STATE SWITCHER--------------------
void CTMAppContent::switchScreen(CTMScreenState newState)
{
    //Same screen, no need to change it
    if (currentScreenState == newState)
        return;

    //Prepare to change to new screen
    currentScreenState = newState;

    switch (newState)
    {
        case CTMScreenState::Processes:
            currentScreen = std::make_unique<CTMProcessScreen>();
            break;

        case CTMScreenState::Apps:
        case CTMScreenState::Services:
        case CTMScreenState::Performance:
            currentScreen = nullptr;
            break;

        case CTMScreenState::Settings:
            currentScreen = std::make_unique<CTMSettingsMenu>();
            break;

        default:
            currentScreen = nullptr;
            break;
    }
}

//--------------------RENDER FUNCTIONS  --------------------
void CTMAppContent::RenderSidebarButton(const char* buttonLabel, const char* fullLabel, const ImVec2& buttonSize, const ImVec4& hoveredColor,
                                        const ImVec4& activeColor, std::function<void(void)> onClick)
{
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoveredColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, activeColor);

    if(ImGui::Button(buttonLabel, buttonSize))
        onClick();

    if(ImGui::IsItemHovered())
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

        //Use tooltip to tell what the button does
        ImGui::BeginTooltip();        
        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
        ImGui::TextUnformatted(fullLabel);
        ImGui::PopStyleColor(2);
        ImGui::EndTooltip();
    }
    
    ImGui::PopStyleColor(3);
}

void CTMAppContent::RenderSidebar(const ImVec2& contentRegion)
{
    //Options sidebar
    if(ImGui::BeginChild("CTMSidebarArea", {CREGION_SIDEBAR_WIDTH, contentRegion.y}))
    {
        ImVec2 sidebarButtonSize = {CREGION_SIDEBAR_WIDTH, CREGION_SIDEBAR_WIDTH};
        //Taskbar content menus
        RenderSidebarButton("Proc", "Processes", sidebarButtonSize,
                            ImVec4(0.3f, 0.6f, 0.8f, 1.0f), ImVec4(0.2f, 0.4f, 0.6f, 1.0f), [this](){ switchScreen(CTMScreenState::Processes); });
        RenderSidebarButton("Perf", "Performance", sidebarButtonSize,
                            ImVec4(0.2f, 0.8f, 0.6f, 1.0f), ImVec4(0.1f, 0.6f, 0.4f, 1.0f), [this](){ switchScreen(CTMScreenState::Performance); });
        RenderSidebarButton("Apps", "Applications", sidebarButtonSize,
                            ImVec4(0.4f, 0.7f, 0.3f, 1.0f), ImVec4(0.3f, 0.5f, 0.2f, 1.0f), [this](){ switchScreen(CTMScreenState::Apps); });
        RenderSidebarButton("Srvc", "Services", sidebarButtonSize,
                            ImVec4(0.9f, 0.7f, 0.2f, 1.0f), ImVec4(0.7f, 0.5f, 0.1f, 1.0f), [this](){ switchScreen(CTMScreenState::Services); });
        //Taskbar settings menu
        ImGui::SetCursorPos({0, contentRegion.y - CREGION_SIDEBAR_WIDTH});
        RenderSidebarButton("Stgs", "Settings", sidebarButtonSize,
                            ImVec4(0.4f, 0.4f, 0.8f, 1.0f), ImVec4(0.3f, 0.3f, 0.6f, 1.0f), [this](){ switchScreen(CTMScreenState::Settings); });
        
    }
    ImGui::EndChild();
}

void CTMAppContent::RenderCTMContent(const ImVec2& contentRegion)
{
    ImGui::BeginChild("CTMContentArea", {contentRegion.x - CREGION_SIDEBAR_WIDTH, contentRegion.y}, ImGuiChildFlags_Borders);
    {
        //Example rendering based on the state
        if(currentScreen)
            currentScreen->Render();
        else
            ImGui::Text("Click on a screen to start seeing data.");
        
        ImGui::EndChild();
    }
}

void CTMAppContent::RenderContent()
{
    //Place the cursor below the title bar
    ImGui::SetCursorPos({0, NCREGION_HEIGHT});
    //Available area for client region
    ImVec2 contentRegion = ImGui::GetContentRegionAvail();

    //Child windows padding as main window removes padding
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));

    //Sidebar
    RenderSidebar(contentRegion);

    //Align the content next to the sidebar
    ImGui::SetCursorPos({CREGION_SIDEBAR_WIDTH, NCREGION_HEIGHT});

    //Main content of the entire task manager
    RenderCTMContent(contentRegion);

    //Window padding
    ImGui::PopStyleVar();
}
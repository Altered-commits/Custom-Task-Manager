#include "ctm_perf_screen.h"

//Equivalent to OnInit function
CTMPerformanceScreen::CTMPerformanceScreen()
{
    //Tell the CTMAppContent that the current screen is CTMPerformanceScreen, so remove the default padding which it adds
    stateManager.SetIsPerfScreen(true);
    SetInitialized(true);
}

//Equivalent to OnClean function
CTMPerformanceScreen::~CTMPerformanceScreen()
{
    //Tell the CTMAppContent that the current screen is no longer CTMPerformanceScreen, so add the default padding
    stateManager.SetIsPerfScreen(false);
    SetInitialized(false);
}

//--------------------MAIN RENDER AND UPDATE FUNCTIONS--------------------
void CTMPerformanceScreen::OnRender()
{
    RenderPerformanceScreen();
}

void CTMPerformanceScreen::OnUpdate()
{
    //Call the current screen's update as it won't call itself :D
    if(currentScreen)
        currentScreen->Update();
}

//--------------------SCREEN SWITCHER FUNCTIONS--------------------
void CTMPerformanceScreen::SwitchScreen(CTMPerformanceScreenState newState)
{
    //Same screen, no need to change it
    if (currentScreenState == newState)
        return;

    //Prepare to change to new screen
    currentScreenState = newState;

    switch (newState)
    {
        case CTMPerformanceScreenState::CpuInfo:
            currentScreen = std::make_unique<CTMPerformanceCPUScreen>();
            break;
        
        case CTMPerformanceScreenState::MemoryInfo:
        default:
            currentScreen = nullptr;
            break;
    }
}

//--------------------RENDER FUNCTIONS--------------------
void CTMPerformanceScreen::RenderPerformanceScreen()
{
    //Render the title 'Performance Screen' cuz it looks good
    ImVec2 windowSize = ImGui::GetWindowSize();
    ImVec2 titleSize  = ImGui::CalcTextSize(perfScrTitle);
    float  titleX     = (windowSize.x - titleSize.x) * 0.5f;
    float  titleY     = (NCREGION_HEIGHT - titleSize.y) * 0.5f;

    //Position it in the center and write out the title
    ImGui::SetCursorPos({titleX, titleY});
    ImGui::TextUnformatted(perfScrTitle);
    
    //Place the cursor below the title bar
    ImGui::SetCursorPos({0, NCREGION_HEIGHT});
    //Available area for client region
    ImVec2 contentRegion = ImGui::GetContentRegionAvail();

    //Child windows padding as main window removes padding
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));

    //Sidebar
    RenderSidebar(contentRegion);

    //Align the content next to the sidebar
    ImGui::SetCursorPos({CREGION_PERFSCR_SIDEBAR_WIDTH, NCREGION_HEIGHT});

    //Main content of the entire performance screen
    RenderPerformanceContent(contentRegion);

    //Window padding
    ImGui::PopStyleVar();
}

void CTMPerformanceScreen::RenderSidebarButton(const char* buttonLabel, const char* fullLabel, const ImVec2& buttonSize,
                                        const ImVec4& hoveredColor, const ImVec4& activeColor, CTMPerformanceScreenState newPerfState)
{
    ImGui::PushStyleColor(ImGuiCol_Button, {0, 0, 0, 0});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoveredColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, activeColor);

    if(ImGui::Button(buttonLabel, buttonSize))
        SwitchScreen(newPerfState);

    if(ImGui::IsItemHovered())
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

        //Use tooltip to tell what the button does
        ImGui::BeginTooltip();        
        ImGui::PushStyleColor(ImGuiCol_PopupBg, {0.15f, 0.15f, 0.15f, 1.0f});
        ImGui::PushStyleColor(ImGuiCol_Border, {0.4f, 0.4f, 0.4f, 1.0f});
        ImGui::TextUnformatted(fullLabel);
        ImGui::PopStyleColor(2);
        ImGui::EndTooltip();
    }
    
    ImGui::PopStyleColor(3);
}

void CTMPerformanceScreen::RenderSidebar(const ImVec2& contentRegion)
{
    //Options sidebar
    if(ImGui::BeginChild("CTMPerformanceSidebarArea", {CREGION_PERFSCR_SIDEBAR_WIDTH, contentRegion.y}))
    {
        ImVec2 sidebarButtonSize{CREGION_PERFSCR_SIDEBAR_WIDTH, CREGION_PERFSCR_SIDEBAR_WIDTH};
        
        RenderSidebarButton("CPU", "CPU Info", sidebarButtonSize,
                        {0.2f, 0.5f, 0.8f, 1.0f}, {0.1f, 0.3f, 0.6f, 1.0f}, CTMPerformanceScreenState::CpuInfo);
        RenderSidebarButton("MEM", "Memory Info", sidebarButtonSize,
                        {0.3f, 0.7f, 0.3f, 1.0f}, {0.2f, 0.5f, 0.2f, 1.0f}, CTMPerformanceScreenState::MemoryInfo);
    }
    ImGui::EndChild();
}

void CTMPerformanceScreen::RenderPerformanceContent(const ImVec2& contentRegion)
{
    if(ImGui::BeginChild("CTMPerformanceContentArea", {contentRegion.x - CREGION_PERFSCR_SIDEBAR_WIDTH, contentRegion.y}, ImGuiChildFlags_Borders))
    {
        if(currentScreen)
            currentScreen->Render();
        else
            ImGui::Text("<- Select a page to start showing data.");
    }
    ImGui::EndChild();
}

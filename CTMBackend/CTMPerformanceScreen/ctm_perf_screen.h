#ifndef CTM_PERF_MENU_HPP
#define CTM_PERF_MENU_HPP

//Windows stuff
#include <windows.h>
//ImGui stuff
#include "../../ImGUI/imgui.h"
#include "../../ImPlot/implot.h"
//My stuff
#include "CTMPerformanceCPUScreen/ctm_perf_cpu_screen.h"
#include "../ctm_base_state.h"
#include "../ctm_constants.h"
#include "../CTMGlobalManagers/ctm_state_manager.h"
//Stdlib stuff
#include <memory>

class CTMPerformanceScreen : public CTMBaseScreen
{
public:
    CTMPerformanceScreen();
    ~CTMPerformanceScreen() override;

protected:
    void OnRender() override;
    void OnUpdate() override;

//The logic is same as the one used in 'ctm_app_content.h'. I should abstract away this logic someday
private: //Screen switcher
    void SwitchScreen(CTMPerformanceScreenState);

private: //Main renderer
    void RenderPerformanceScreen();

private: //Render functions
    void RenderSidebarButton(const char*, const char*, const ImVec2&, const ImVec4&, const ImVec4&, CTMPerformanceScreenState);
    void RenderSidebar(const ImVec2&);
    void RenderPerformanceContent(const ImVec2&);

private: //Misc variables
    const char*      perfScrTitle = CTM_PERFSCR_TITLE;
    CTMStateManager& stateManager = CTMStateManager::GetInstance();

private: //Screen states
    std::unique_ptr<CTMBasePerformanceScreen> currentScreen;
    CTMPerformanceScreenState currentScreenState = CTMPerformanceScreenState::None;
};

#endif
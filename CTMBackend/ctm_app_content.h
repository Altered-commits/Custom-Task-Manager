#ifndef CTM_APP_CONTENT_HPP
#define CTM_APP_CONTENT_HPP

//Windows stuff
#include <WinSock2.h>
#include <windows.h>
//ImGui stuff
#include "../ImGUI/imgui.h"
//Stdlib stuff
#include <functional>
#include <memory>
//My stuff
#include "CTMGlobalManagers/ctm_state_manager.h"
#include "CTMPerformanceScreen/ctm_perf_screen.h"
#include "CTMProcessScreen/ctm_process_screen.h"
#include "CTMSettingsScreen/ctm_settings_screen.h"
#include "ctm_constants.h"

class CTMAppContent
{
    public:
        CTMAppContent();
        ~CTMAppContent() = default;

        //No need for copy or move operations
        CTMAppContent(const CTMAppContent&)            = delete;
        CTMAppContent& operator=(const CTMAppContent&) = delete;
        CTMAppContent(CTMAppContent&&)                 = delete;
        CTMAppContent& operator=(CTMAppContent&&)      = delete;

    public: //Client region renderer
        void RenderContent();

    private: //Render helper functions
        void RenderSidebarButton(const char*, const char*, const ImVec2&, const ImVec4&, const ImVec4&, std::function<void(void)>);
        void RenderSidebar(const ImVec2&);
        void RenderCTMContent(const ImVec2&);
    
    private: //Screen state switching
        void SwitchScreen(CTMScreenState);

    private: //Screen state variables
        std::unique_ptr<CTMBaseScreen> currentScreen;
        //Load this value from settings in constructor
        CTMScreenState currentScreenState = CTMScreenState::None;
    
    private: //State Manager
        CTMStateManager& stateManager = CTMStateManager::GetInstance();
};


#endif
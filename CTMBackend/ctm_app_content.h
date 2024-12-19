#ifndef CTM_APP_CONTENT_HPP
#define CTM_APP_CONTENT_HPP

//ImGui stuff
#include "../ImGUI/imgui.h"
//My stuff
#include "ctm_constants.h"
#include "ctm_state_manager.h"
#include "CTMProcessMenu/ctm_process_menu.h"
#include "CTMSettingsMenu/ctm_settings_menu.h"
//Stdlib stuff
#include <functional>
#include <memory>

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
        void switchScreen(CTMScreenState);

    private: //Variables
        std::unique_ptr<CTMBaseState> currentScreen;
        //Load this value from settings in constructor
        CTMScreenState currentScreenState = CTMScreenState::None;
};


#endif
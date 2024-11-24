#ifndef CTM_APP_CONTENT_HPP
#define CTM_APP_CONTENT_HPP

#include "../ImGUI/imgui.h"
#include "ctm_constants.h"
#include "ctm_process_menu.h"
//Std lib stuff (std::function<>)
#include <functional>
#include <memory>

class CTMAppContent
{
    public:
        CTMAppContent() = default;
        CTMAppContent(const CTMAppContent&) = delete;
        CTMAppContent(CTMAppContent&&)      = delete;

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
        CTMScreenState currentScreenState = CTMScreenState::NONE;
};


#endif
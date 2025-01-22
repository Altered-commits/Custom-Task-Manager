#ifndef CTM_APP_HPP
#define CTM_APP_HPP

//Windows stuff
#include <WinSock2.h>
#include <windows.h>
#include <d3d11.h>
#include <windowsx.h>
#include <dwmapi.h>
//ImGui stuff
#include "../ImGUI/imgui.h"
#include "../ImGUIBackend/imgui_impl_win32.h"
#include "../ImGUIBackend/imgui_impl_dx11.h"
//My stuff
#include "ctm_constants.h"
#include "ctm_app_content.h"
#include "CTMGlobalManagers/ctm_state_manager.h"
#include "ctm_misc.h"

//Forward declare as told by imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

enum class CTMAppErrorCodes : std::uint8_t
{
    D3DCreationFailed,
    ImGuiFontInitFailed,
    InitSuccess,
};

enum class CTMAppNCButtonState : std::uint8_t
{
    NoButtonHovered,
    CloseButtonHovered,
    MaximizeButtonHovered,
    MinimizeButtonHovered
};

class CTMApp {
    public:
        CTMApp() = default;
        ~CTMApp();

        //No need for copy or move operations
        CTMApp(const CTMApp&)            = delete;
        CTMApp& operator=(const CTMApp&) = delete;
        CTMApp(CTMApp&&)                 = delete;
        CTMApp& operator=(CTMApp&&)      = delete;

        //Main stuff
        CTMAppErrorCodes Initialize();
        void             Run();
    
    private: //Render functions
        void RenderFrameContent();
        void RenderFrame();
        void RenderNCRegionButton(const char*, const ImVec2&, const ImVec2&, const ImVec4&, const ImVec4&, CTMAppNCButtonState);
        void PresentFrame();
    
    private: //Main loop helper functions
        void HandleMessages();
        bool HandleOcclusion();
        void HandleResizing();

    private: //CTM variables
        bool isMainWindowMaximized = false;
        //NC Region variables
        const char*         NCRegionAppTitle    = CTM_APP_WINDOW_TITLE;
        ImVec2              NCRegionButtonSize  = {NCREGION_HEIGHT, NCREGION_HEIGHT};
        CTMAppNCButtonState NCRegionButtonState = CTMAppNCButtonState::NoButtonHovered;
    
    private: //CTM variables
        CTMAppContent    appContent;
        CTMStateManager& stateManager = CTMStateManager::GetInstance();

    private: //Font Variable
        ImFont* pressStartFont = nullptr;

    private: //Outcast ;-;
        static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
        LRESULT WINAPI        HandleWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    private: //Window Helper functions
        void InitializeMainWindow();
        void SetupImGuiAndPlot();
        bool SetupCTMSettings();
        bool IsWindowMaximized(HWND);
        void TryRemoveWindowsRoundedCorners();

        bool CreateDeviceD3D();
        void CleanupDeviceD3D();
        void CreateRenderTarget();
        void CleanupRenderTarget();

    private: //Idk Variables
        int windowWidth     = CTM_APP_WINDOW_WIDTH;
        int windowHeight    = CTM_APP_WINDOW_HEIGHT;
        int done            = false; //Quite an important variable

    private: //OS Window creation Variables
        WNDCLASSEXW              wc           = {};          
        HWND                     windowHandle = nullptr;

    private: //D3D && DXGI Variables
        ID3D11Device*            g_pd3dDevice           = nullptr;
        ID3D11DeviceContext*     g_pd3dDeviceContext    = nullptr;
        IDXGISwapChain*          g_pSwapChain           = nullptr;
        bool                     g_SwapChainOccluded    = false;
        UINT                     g_ResizeWidth          = 0
                               , g_ResizeHeight         = 0;
        ID3D11RenderTargetView*  g_mainRenderTargetView = nullptr;
};

#endif
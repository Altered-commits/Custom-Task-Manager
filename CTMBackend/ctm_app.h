#ifndef CTM_APP_HPP
#define CTM_APP_HPP

//ImGui stuff
#include "../ImGUI/imgui.h"
#include "../ImGUIBackend/imgui_impl_win32.h"
#include "../ImGUIBackend/imgui_impl_dx11.h"
//My stuff
#include "ctm_constants.h"
#include "ctm_app_content.h"
//Windows stuff
#include <d3d11.h>
#include <windowsx.h>
//Std lib stuff
#include <iostream> //For debugging purposes

//Forward declare as told by imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

//App error codes
enum class CTMAppErrorCodes
{
    D3D_CREATION_FAILED,
    IMGUI_FONT_INIT_FAILED,
    INIT_SUCCESS,
};

enum class CTMAppNCButtonState
{
    NO_BUTTON_HOVERED,
    CLOSE_BUTTON_HOVERED,
    MAXIMIZE_BUTTON_HOVERED,
    MINIMIZE_BUTTON_HOVERED
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
        void RenderFrame(ImGuiIO&);
        void RenderNCRegionButton(const char*, const ImVec2&, const ImVec2&, const ImVec4&, const ImVec4&, CTMAppNCButtonState);
        void PresentFrame();
    
    private: //Main loop helper functions
        void HandleMessages();
        bool HandleOcclusion();
        void HandleResizing();

    private: //CTM variables
        bool isMainWindowMaximized = false;
        //NC Region variables
        const char*         NCRegionAppTitle    = "Walmart Task Manager";
        ImVec2              NCRegionButtonSize  = {NCREGION_HEIGHT, NCREGION_HEIGHT};
        CTMAppNCButtonState NCRegionButtonState = CTMAppNCButtonState::NO_BUTTON_HOVERED;
    
    private: //CTM Content variables
        CTMAppContent appContent;

    private: //Font Variables
        ImFont* pressStartFont = nullptr;

    private: //Outcast ;-;
        static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
        LRESULT WINAPI        HandleWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    private: //Window Helper functions
        void InitializeMainWindow();
        void SetupImGui();
        bool IsWindowMaximized(HWND);
        bool IsWindows10OrGreater();
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
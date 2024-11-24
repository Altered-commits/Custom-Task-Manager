#include "ctm_app.h"
#include <iostream> //For debugging purposes

CTMApp::CTMApp() {}

CTMApp::~CTMApp()
{
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    
    ::DestroyWindow(windowHandle);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
}

CTMAppErrorCodes CTMApp::Initialize()
{
    ImGui_ImplWin32_EnableDpiAwareness();
    //Set window size to max resolution supported by users screen
    SetMaxResWindowSize();
    
    InitializeMainWindow();
    
    //Initialize Direct3D
    if (!CreateDeviceD3D())
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return CTMAppErrorCodes::D3D_CREATION_FAILED;
    }

    //Show the window and update it
    ::ShowWindow(windowHandle, SW_SHOWDEFAULT);
    ::UpdateWindow(windowHandle);

    //Initialize ImGui
    SetupImGui();
    
    //Initialize ImGui fonts
    ImGuiIO& io = ImGui::GetIO();
    pressStartFont = io.Fonts->AddFontFromFileTTF(FONT_PRESS_START_PATH, 16.0f);
    if(!pressStartFont) return CTMAppErrorCodes::IMGUI_FONT_INIT_FAILED;

    return CTMAppErrorCodes::INIT_SUCCESS;
}

void CTMApp::Run()
{
    ImGuiIO& io = ImGui::GetIO();
    
    while (!done) {
        HandleMessages();

        //If the user wants to quit, QUIT
        if (done)
            break;

        if(HandleOcclusion())
            continue;
        
        HandleResizing();

        RenderFrame(io);
        PresentFrame();
    }
}

//----------Render Functions----------
void CTMApp::RenderFrameContent()
{
    ImGui::SetNextWindowSizeConstraints(minMainWindowSize, maxMainWindowSize);

    ImGui::PushFont(pressStartFont);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0)); //Other windows will add their own padding

    if (ImGui::Begin("CTMMainWindow", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse)) {
        ImVec2 windowSize = ImGui::GetWindowSize();
        
        if (isMainWindowMaximized && (windowSize.x != maxMainWindowSize.x || windowSize.y != maxMainWindowSize.y))
            isMainWindowMaximized = false; //Reset maximize state

        //Non-Client Region (NC Region)
        ImGui::SetCursorPos({0, 0});
        if (ImGui::BeginChild("CTMNCRegion", {windowSize.x, NCREGION_HEIGHT}, false, ImGuiWindowFlags_NoScrollbar)) {
            //Title
            ImVec2 titleSize = ImGui::CalcTextSize(NCRegionAppTitle);
            float  titleX    = (windowSize.x - titleSize.x) / 2.0f;
            float  titleY    = (NCREGION_HEIGHT - titleSize.y) / 2.0f;

            ImGui::SetCursorPos(ImVec2(titleX, titleY)); //Vertically and Horizontally center text in NC region
            ImGui::Text("%s", NCRegionAppTitle);

            //Close, Maximize, Minimize Button
            const char* crossButton       = "X",
                      * maximizeButton    = isMainWindowMaximized ? "-" : "+",
                      * minimizeButton    = "_";
            ImVec2      crossButtonPos    = {windowSize.x - NCREGION_HEIGHT      , 0},
                        maximizeButtonPos = {windowSize.x - (2 * NCREGION_HEIGHT), 0},
                        minimizeButtonPos = {windowSize.x - (3 * NCREGION_HEIGHT), 0};
            
            //Close button
            RenderNCRegionButton(crossButton, NCRegionButtonSize, crossButtonPos,
                                ImVec4(0.8f, 0.2f, 0.2f, 0.8f), ImVec4(1.0f, 0.0f, 0.0f, 1.0f), [this](){ done = true; });

            //Maximize button
            RenderNCRegionButton(maximizeButton, NCRegionButtonSize, maximizeButtonPos,
                                ImVec4(0.7f, 0.7f, 0.7f, 0.8f), ImVec4(0.5f, 0.5f, 0.5f, 1.0f), 
                                [this](){
                                    if(!isMainWindowMaximized) {
                                        //Save the original position and window size
                                        originalWindowPos  = ImGui::GetWindowPos();
                                        originalWindowSize = ImGui::GetWindowSize();
                                        //Maximize the window
                                        ImGui::SetWindowPos("CTMMainWindow", {0, 0});
                                        ImGui::SetWindowSize("CTMMainWindow", maxMainWindowSize);
                                    }
                                    else {
                                        //Restore window
                                        ImGui::SetWindowPos("CTMMainWindow", originalWindowPos);
                                        ImGui::SetWindowSize("CTMMainWindow", originalWindowSize);
                                    }

                                    isMainWindowMaximized = !isMainWindowMaximized;
                                });

            //Minimize button
            RenderNCRegionButton(minimizeButton, NCRegionButtonSize, minimizeButtonPos,
                                ImVec4(0.7f, 0.7f, 0.7f, 0.8f), ImVec4(0.5f, 0.5f, 0.5f, 1.0f), [this](){ ShowWindow(windowHandle, SW_MINIMIZE); });
        }
        ImGui::EndChild();

        //Main CTM Content begins here
        appContent.RenderContent();
        
    }
    ImGui::End();

    ImGui::PopStyleVar();
    //Press Start font
    ImGui::PopFont();
}

void CTMApp::RenderFrame(ImGuiIO& io)
{
    // Start the Dear ImGui frame
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    RenderFrameContent();

    // Rendering
    ImGui::Render();
    const float clear_color_with_alpha[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
    g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void CTMApp::PresentFrame()
{
    //Present
    HRESULT hr = g_pSwapChain->Present(1, 0);   // Present with vsync
    //HRESULT hr = g_pSwapChain->Present(0, 0); // Present without vsync
    g_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
}

//Helper render functions
void CTMApp::RenderNCRegionButton(const char* buttonLabel, const ImVec2& buttonSize, const ImVec2& buttonPos,
                                    const ImVec4& hoveredColor, const ImVec4& activeColor, std::function<void(void)> onClick)
{
    ImGui::SetCursorPos(buttonPos);

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoveredColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, activeColor);

    if (ImGui::Button(buttonLabel, buttonSize))
        onClick();

    if (ImGui::IsItemHovered())
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

    ImGui::PopStyleColor(3);
}

//----------Main loop handlers----------
void CTMApp::HandleMessages()
{
    MSG msg;
    
    // auto targetFrameTime = std::chrono::milliseconds(16);  // ~60 FPS (16ms per frame)
    // auto frameStartTime = std::chrono::high_resolution_clock::now();

    while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
        if (msg.message == WM_QUIT)
            done = true;
    }

    // //Calculate time spent in message handling
    // auto frameEndTime = std::chrono::high_resolution_clock::now();
    // auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(frameEndTime - frameStartTime);

    // //Sleep to limit the frame rate (if we have more time left)
    // if (elapsedTime < targetFrameTime) {
    //     auto sleepDuration = targetFrameTime - elapsedTime;
    //     Sleep(sleepDuration.count());
    // }
}

bool CTMApp::HandleOcclusion()
{
    if (g_SwapChainOccluded && g_pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED) {
        ::Sleep(10);
        return true;
    }
    g_SwapChainOccluded = false;
    return false;
}

void CTMApp::HandleResizing()
{
    if (g_ResizeWidth != 0 && g_ResizeHeight != 0) {
        CleanupRenderTarget();
        g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
        g_ResizeWidth = g_ResizeHeight = 0;
        CreateRenderTarget();
    }
}

//----------Helper functions----------
void CTMApp::InitializeMainWindow()
{
    // Create application window
    wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"CTMImGui", nullptr };
    ::RegisterClassExW(&wc);
    
    windowHandle = ::CreateWindowExW(WS_EX_LAYERED, wc.lpszClassName, L"DirectXCTMImGui", WS_POPUP,
                                    0, 0, windowWidth, windowHeight, nullptr, nullptr, wc.hInstance, nullptr);
    
    //Set the App instance pointer in the window's user data
    ::SetWindowLongPtr(windowHandle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    
    SetLayeredWindowAttributes(windowHandle, RGB(0, 0, 0), 0, ULW_COLORKEY);
}

void CTMApp::SetupImGui()
{
    //Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    //Setup Dear ImGui style
    ImGui::StyleColorsDark();

    //Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(windowHandle);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
}

bool CTMApp::CreateDeviceD3D()
{
    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = windowHandle;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    //createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED) // Try high-performance WARP software driver if hardware is not available.
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CTMApp::CleanupDeviceD3D()
{
    CleanupRenderTarget();

    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CTMApp::CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CTMApp::CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

void CTMApp::SetMaxResWindowSize()
{
    windowWidth  = GetSystemMetrics(SM_CXSCREEN);
    windowHeight = GetSystemMetrics(SM_CYSCREEN);

    //Set the max, min main imgui window size as well
    maxMainWindowSize = {windowWidth, windowHeight};
}

// OUTCAST lmao
LRESULT WINAPI CTMApp::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    CTMApp* pApp = reinterpret_cast<CTMApp*>(::GetWindowLongPtr(hwnd, GWLP_USERDATA));

    if (pApp)
        pApp->HandleWndProc(hwnd, msg, wParam, lParam);

    return ::DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT WINAPI CTMApp::HandleWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam); // Queue resize
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

#ifndef CTM_FILE_PATHS_HPP
#define CTM_FILE_PATHS_HPP

//Window constants
#define CTM_APP_CLASS_NAME  L"CTMImGuiApp"
#define CTM_APP_WINDOW_NAME L"DirectXCTMImGuiApp"

//EWT constants
#define TCPIP_PROVIDER_GUID { 0x2F07E2EE, 0x15DB, 0x40F1, { 0x90, 0xEF, 0x9D, 0x7B, 0xA2, 0x82, 0x18, 0x8A } }

//Mutex constant (Not the most secure way to do it but for now lets do it like this).
//Also added a uuid at the end to sort of make it unique????? i have no idea how these work. (Generated using python uuid)
#define CTM_APP_MUTEX_NAME L"Global\\CTMImGui_App_Single_Instance_Mutex_88bac14c-67fe-44e9-a29a-071b22a95104"

//File paths
#define FONT_PRESS_START_PATH "./Fonts/PressStart.ttf"

//NC Region height
#define NCREGION_HEIGHT (40)

//Client Region sidebar width
#define CREGION_SIDEBAR_WIDTH (70)

#endif
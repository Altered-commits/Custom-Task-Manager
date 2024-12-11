#ifndef CTM_FILE_PATHS_HPP
#define CTM_FILE_PATHS_HPP

//Window constants
#define CTM_APP_CLASS_NAME  L"CTMImGuiApp"
#define CTM_APP_WINDOW_NAME L"DirectXCTMImGuiApp"
#define CTM_APP_WINDOW_WIDTH  (1280)
#define CTM_APP_WINDOW_HEIGHT (720)
#define CTM_APP_WINDOW_MIN_WIDTH  (1200)
#define CTM_APP_WINDOW_MIN_HEIGHT (600)

//EWT constants
#define MICROSOFT_WINDOWS_KERNEL_NETWORK_GUID { 0x7DD42A49, 0x5329, 0x4832, { 0x8D, 0xFD, 0x43, 0xD9, 0x79, 0x15, 0x3A, 0x88 } }

//Mutex constant (Not the most secure way to do it but for now lets do it like this).
//Also added a uuid at the end to sort of make it unique????? i have no idea how these work. (Generated using python uuid)
#define CTM_APP_MUTEX_NAME L"Global\\CTMImGui_App_Single_Instance_Mutex_88bac14c-67fe-44e9-a29a-071b22a95104"

//File paths (relative to where exe file exists)
#define FONT_PRESS_START_PATH "./Fonts/PressStart.ttf"

//Non Client Region height
#define NCREGION_HEIGHT (40)

//Client Region sidebar width
#define CREGION_SIDEBAR_WIDTH (70)

#endif
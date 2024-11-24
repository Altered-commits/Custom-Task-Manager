#ifndef CTM_FILE_PATHS_HPP
#define CTM_FILE_PATHS_HPP

//Window constansts
#define CTM_APP_CLASS_NAME  L"CTMImGuiApp"
#define CTM_APP_WINDOW_NAME L"DirectXCTMImGuiApp"

//Mutex constant (Not the most secure way to do it but for now lets do it like this).
//Also added a uuid at the end to sort of make it unique????? i have no idea how these work. (Generated using python uuid)
#define CTM_APP_MUTEX_NAME L"Global\\CTMImGui_App_Single_Instance_Mutex_88bac14c-67fe-44e9-a29a-071b22a95104"

//File paths
#define FONT_PRESS_START_PATH "./Fonts/PressStart.ttf"

//NC Region height
#define NCREGION_HEIGHT (40)

//Client Region options and settings menu width
#define CREGION_SIDEBAR_WIDTH (70)

#endif
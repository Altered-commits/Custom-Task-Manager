#ifndef CTM_FILE_PATHS_HPP
#define CTM_FILE_PATHS_HPP

//All the stuff which user can tinker with
//Ofc nothings stopping you from changing the other files but-
//-if you don't know what you are doing, just change this file only, that too only window stuff (like nc region height, window width, etc)

//Window constants
#define CTM_APP_CLASS_NAME  L"CTMImGuiApp"
#define CTM_APP_WINDOW_NAME L"DirectXCTMImGuiApp"
#define CTM_APP_WINDOW_WIDTH  (1280)
#define CTM_APP_WINDOW_HEIGHT (720)
#define CTM_APP_WINDOW_MIN_WIDTH  (1200)
#define CTM_APP_WINDOW_MIN_HEIGHT (600)

//EWT constants (Change these GUIDs if it doesn't work for your system)
#define MICROSOFT_WINDOWS_KERNEL_NETWORK_GUID { 0x7DD42A49, 0x5329, 0x4832, { 0x8D, 0xFD, 0x43, 0xD9, 0x79, 0x15, 0x3A, 0x88 } }
#define MICROSOFT_WINDOWS_KERNEL_FILE_GUID    { 0xEDD08927, 0x9CC4, 0x4E65, { 0xB9, 0x70, 0xC2, 0x56, 0x0F, 0xB5, 0xC2, 0x89 } }

//File paths (relative to where exe file exists)
#define FONT_PRESS_START_PATH "./Fonts/PressStart.ttf"

//Non Client (aka title bar) Region height
#define NCREGION_HEIGHT (40)

//Client Region (aka the screen below title bar) sidebar width
#define CREGION_SIDEBAR_WIDTH (70)

#endif
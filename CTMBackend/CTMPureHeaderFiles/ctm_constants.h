#ifndef CTM_FILE_PATHS_HPP
#define CTM_FILE_PATHS_HPP

//All the stuff which user can tinker with
//Ofc nothings stopping you from changing the other files but-
//-if you don't know what you are doing, just change this file only, that too only window stuff (like nc region height, window width, etc)

//Window constants
#define CTM_APP_CLASS_NAME  L"CTMImGuiApp"
#define CTM_APP_WINDOW_NAME L"DirectXCTMImGuiApp"

#define CTM_APP_WINDOW_TITLE      "Task Manager"
#define CTM_APP_WINDOW_WIDTH      (1280)
#define CTM_APP_WINDOW_HEIGHT     (720)
#define CTM_APP_WINDOW_MIN_WIDTH  (1200)
#define CTM_APP_WINDOW_MIN_HEIGHT (600)

//App content class
#define CTM_APP_CONTENT_DEFAULT_WINDOW_PADDING {10, 10}

//Performance screen constants
#define CTM_PERFSCR_TITLE "Performance Screen"

//EWT constants (Change these GUIDs if it doesn't work for your system)
#define MICROSOFT_WINDOWS_KERNEL_NETWORK_GUID { 0x7DD42A49, 0x5329, 0x4832, { 0x8D, 0xFD, 0x43, 0xD9, 0x79, 0x15, 0x3A, 0x88 } }
#define MICROSOFT_WINDOWS_KERNEL_FILE_GUID    { 0xEDD08927, 0x9CC4, 0x4E65, { 0xB9, 0x70, 0xC2, 0x56, 0x0F, 0xB5, 0xC2, 0x89 } }

//File paths (relative to where exe file exists)
#define FONT_PRESS_START_PATH "./Fonts/PressStart.ttf"

//Non Client (aka title bar) Region height
#define NCREGION_HEIGHT (40)

//Client Region (aka the screen below title bar) sidebar width
#define CREGION_SIDEBAR_WIDTH         (70)
#define CREGION_PERFSCR_SIDEBAR_WIDTH CREGION_SIDEBAR_WIDTH

/*
 * I also decided to define some common macros which i could use around files
 * Because this happens to be quite commonly used across other files :)
 */

//1) Conversion ratios
#define CTM_DIV_BY_KB(val)     ((val) / (1024.0))
#define CTM_DIV_BY_KBSQR(val)  ((val) / (1024.0 * 1024.0))
#define CTM_DIV_BY_KBCUBE(val) ((val) / (1024.0 * 1024.0 * 1024.0))

#define CTM_MUL_BY_KB(val)     ((val) * (1024.0))
#define CTM_MUL_BY_KBSQR(val)  ((val) * (1024.0 * 1024.0))
#define CTM_MUL_BY_KBCUBE(val) ((val) * (1024.0 * 1024.0 * 1024.0))

//1.1) Increasing readability
#define CTM_BYTES_TO_KB(val) CTM_DIV_BY_KB(val)
#define CTM_BYTES_TO_MB(val) CTM_DIV_BY_KBSQR(val)
#define CTM_BYTES_TO_GB(val) CTM_DIV_BY_KBCUBE(val)

#define CTM_KB_TO_BYTES(val) CTM_MUL_BY_KB(val)
#define CTM_MB_TO_BYTES(val) CTM_MUL_BY_KBSQR(val)
#define CTM_GB_TO_BYTES(val) CTM_MUL_BY_KBCUBE(val)

#define CTM_KB_TO_GB(val)    CTM_DIV_BY_KBSQR(val)
#define CTM_GB_TO_KB(val)    CTM_MUL_BY_KBSQR(val)
#define CTM_MB_TO_GB(val)    CTM_DIV_BY_KB(val)
#define CTM_GB_TO_MB(val)    CTM_MUL_BY_KB(val)

#endif
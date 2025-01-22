#include "ctm_perf_common.h"
#include <iostream>
bool CTMPerformanceCommon::WSToSWithEllipsisTruncation(PSTR dest, PWSTR src, int destSize, std::size_t srcSizeExternal)
{
    //Either destination or source is nullptr, or destination size is < 4 (no space even for ellipsis truncation '...\0')
    if(!dest || !src || destSize < 4)
        return false;
    
    //Get the length of source string without null terminator
    std::size_t srcSize      = srcSizeExternal ? srcSizeExternal : std::wcslen(src);
    bool        needsTrunc   = srcSize >= destSize;
    int         trueDestSize = needsTrunc ? destSize - 4 : destSize;
    
    //Convert wide char (in our case is BSTR) to 8 bit char
    int res = WideCharToMultiByte(
                        CP_UTF8,      //UTF8 encoding is used
                        0,            //NOTE: For the code page 65001 (UTF-8), dwFlags must be set to either 0 or WC_ERR_INVALID_CHARS
                        src,          //Source string
                        -1,           //Source string is null terminated so just use whatever the size of the source is
                        dest,         //Destination string
                        trueDestSize, //Destination buffer size after conditional checks
                        //For CP_UTF8, these parameters need to be NULL
                        NULL,
                        NULL
                    );
    
    //Conversion failed
    if(res == 0)
        return false;

    //Check if ellipsis is needed
    //If it does, then add ellipsis (...) at the end along with null terminator ('.', '.', '.' and '\0')
    if(needsTrunc)
        std::memcpy(dest + trueDestSize, "...", 4);

    return true;
}

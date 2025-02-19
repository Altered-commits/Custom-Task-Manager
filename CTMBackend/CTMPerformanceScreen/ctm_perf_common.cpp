//My stuff
#include "ctm_perf_common.h"
//Stdlib stuff
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

float CTMPerformanceCommon::EncodeDoubleWithUnits(double inData)
{
    /*
     * The converted value will range somewhere from 0 to 1023 until it gets converted again to higher type.
     * So it won't take that many bits in practice
     */
    //Base case
    if(inData <= 0)
        return 0;

    //End deduced data type
    std::uint8_t calcUnitIndex = 0;

    //Scale down the size using simple division. This outperforms log + pow implementation, atleast on my system
    while(inData >= 1024.0 && calcUnitIndex < dataUnitsSize - 1)
    {
        inData /= 1024.0;
        ++calcUnitIndex;
    }

    //Now that the value has been converted, the value can be casted to float without much data loss
    CTMFloatView fv;
    fv.floatView = static_cast<float>(inData);

    //Set the 3 bits in mantissa (furthest from exponent) to contain its data type (KB, MB... and so on)
    fv.bitView = (fv.bitView & ~0x7) | calcUnitIndex;

    return fv.floatView;
}

void CTMPerformanceCommon::DecodeDoubleWithUnits(float inData, std::uint8_t& outType, float& outData)
{
    //Create a float view of data
    CTMFloatView fv;
    fv.floatView = inData;
    
    //Extract the 3 least significant bits of mantissa
    outType = (fv.bitView & 0x7);

    //Clear the last three bits and retrive the data
    //NOTE: Clearing out the last three bits doesn't hurt the accuracy that much. So yeah, no issue
    fv.bitView &= ~0x7;
    outData = fv.floatView;
}

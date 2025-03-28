//My stuff
#include "ctm_perf_common.h"
//Stdlib stuff
#include <iostream>

//This is like a wrapper function around CTMMisc::WSToSWithEllipsisTruncation
bool CTMPerformanceCommon::WSToSWithEllipsisTruncation(PSTR outString, PWSTR inString, int destSize, std::size_t srcSize)
{
    return CTMMisc::WSToSWithEllipsisTruncation(outString, inString, destSize, srcSize);
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

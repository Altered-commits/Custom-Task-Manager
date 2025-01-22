#ifndef CTM_PERFORMANCE_COMMON_HPP
#define CTM_PERFORMANCE_COMMON_HPP

//Windows stuff
#include <windows.h>
//Stdlib stuff
#include <memory>
#include <cstring>
#include <variant>
#include <vector>

/*
 * Houses the most common macros, typedefs and classes for performance pages like CPU, Memory, etc.
 */

//'using' makes my life alot easier
using MetricsValue  = std::variant<DWORD, float, double, const char*>;
using MetricsVector = std::vector<std::pair<const char*, MetricsValue>>;

//Few defines for WMI querying because the function querying wmi looks cluttered, an absolute mess. Other files will just use proper functions
//This macro assumes that you will be using the above defined 'MetricsVector'. MV stands for Metrics Vector
#define CTM_WMI_ERROR_MV(mv, idx, ...) mv[static_cast<std::size_t>(idx)].second = "Failed";\
                                       CTM_LOG_ERROR(__VA_ARGS__);

#define CTM_WMI_ERROR_RET(...) CTM_LOG_ERROR(__VA_ARGS__);\
                               return false;

#define CTM_WMI_WSTOS_CONDITION(outBuffer, outBufferSize, inBuffer) \
                            if(!CTMPerformanceCommon::WSToSWithEllipsisTruncation(outBuffer, inBuffer, outBufferSize))

#define CTM_WMI_WSTOS_WITH_MV(outBuffer, inBuffer, mv, idx, errorMsg) \
                    CTM_WMI_WSTOS_CONDITION(outBuffer, sizeof(outBuffer), inBuffer)\
                    { CTM_WMI_ERROR_MV(mv, idx, errorMsg, " Error code: ", GetLastError()) }

#define CTM_WMI_WSTOS_WITH_ERROR(outBuffer, inBuffer, errorMsg) \
                    CTM_WMI_WSTOS_CONDITION(outBuffer, sizeof(outBuffer), inBuffer)\
                    { CTM_LOG_ERROR(errorMsg, " Error code: ", GetLastError()); }

#define CTM_WMI_WSTOS_WITH_ERROR_CONT(outBuffer, outBufferSize, inBuffer, errorMsg) \
                    CTM_WMI_WSTOS_CONDITION(outBuffer, outBufferSize, inBuffer)\
                    { CTM_LOG_ERROR(errorMsg, " Error code: ", GetLastError()); continue; }

#define CTM_WMI_START_CONDITION(cond)  if(cond) {
#define CTM_WMI_ELIF_CONDITION(cond)   } else if(cond) {
#define CTM_WMI_ELSE_CONDITION()       } else {
#define CTM_WMI_END_CONDITION()        }

//Some common functions enclosed in class (almost like a namespace except i'm not using namespace because i'm an idiot)
class CTMPerformanceCommon
{
public:
    static bool WSToSWithEllipsisTruncation(PSTR, PWSTR, int, std::size_t = 0);
};

#endif
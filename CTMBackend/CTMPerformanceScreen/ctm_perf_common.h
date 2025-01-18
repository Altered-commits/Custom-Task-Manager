#ifndef CTM_PERFORMANCE_COMMON_HPP
#define CTM_PERFORMANCE_COMMON_HPP

//Windows stuff
#include <minwindef.h>
//Stdlib stuff
#include <variant>
#include <vector>

/*
 * Houses the most common macros and typedefs for performance pages like CPU, Memory, etc.
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

#define CTM_WMI_WSTOS_CONDITION(wmi, outBuffer, outBufferSize, inBuffer) \
                            if(!wmi.WSToSWithEllipsisTruncation(outBuffer, inBuffer, outBufferSize))

#define CTM_WMI_WSTOS_WITH_MV(wmi, outBuffer, inBuffer, mv, idx, errorMsg) \
                    CTM_WMI_WSTOS_CONDITION(wmi, outBuffer, sizeof(outBuffer), inBuffer)\
                    { CTM_WMI_ERROR_MV(mv, idx, errorMsg, " Error code: ", GetLastError()) }

#define CTM_WMI_WSTOS_WITH_ERROR(wmi, outBuffer, inBuffer, errorMsg) \
                    CTM_WMI_WSTOS_CONDITION(wmi, outBuffer, sizeof(outBuffer), inBuffer)\
                    { CTM_LOG_ERROR(errorMsg, " Error code: ", GetLastError()); }

#define CTM_WMI_WSTOS_WITH_ERROR_CONT(wmi, outBuffer, outBufferSize, inBuffer, errorMsg) \
                    CTM_WMI_WSTOS_CONDITION(wmi, outBuffer, outBufferSize, inBuffer)\
                    { CTM_LOG_ERROR(errorMsg, " Error code: ", GetLastError()); continue; }

#define CTM_WMI_START_CONDITION(cond)  if(cond) {
#define CTM_WMI_ELIF_CONDITION(cond)   } else if(cond) {
#define CTM_WMI_ELSE_CONDITION()       } else {
#define CTM_WMI_END_CONDITION()        }

#endif
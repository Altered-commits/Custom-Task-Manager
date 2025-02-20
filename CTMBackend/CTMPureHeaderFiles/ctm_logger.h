#ifndef CTM_LOGGER_HPP
#define CTM_LOGGER_HPP

//Stdlib stuff
#include <iostream>
#include <chrono>
#include <iomanip>

//MSVC warnings forcing me to use _s function, NOPE, i handle stuff in my own way so no thanks
#pragma warning(disable:4996)

enum class CTMLogLevel
{
    Info,
    Success,
    Warning,
    Error
};

class CTMLogger {
public:
    template <typename... Args>
    static void Log(CTMLogLevel level, char endingChar, Args&&... args)
    {
        auto now     = std::chrono::system_clock::now();
        auto nowTime = std::chrono::system_clock::to_time_t(now);

        auto[logLevelString, logLevelColorCode] = LogLevelToStringAndColor(level);

        //Print timestamp
        std::cout << '[' << std::put_time(std::localtime(&nowTime), "%Y-%m-%d %H:%M:%S") << ']'
                  << logLevelColorCode << '[' << logLevelString << "]\033[0m: ";

        //Directly print variadic arguments
        (std::cout << ... << args) << endingChar;
    }

    template<typename... Args>
    static void LogPureText(Args&&... args)
    {
        //Directly print variadic arguments
        (std::cout << ... << args) << '\n';
    }

private:
    static std::pair<const char*, const char*> LogLevelToStringAndColor(CTMLogLevel level)
    {
        switch(level)
        {
            case CTMLogLevel::Info:    return {"INFO", "\033[36m"};    //Cyan
            case CTMLogLevel::Success: return {"SUCCESS", "\033[32m"}; //Green
            case CTMLogLevel::Warning: return {"WARNING", "\033[33m"}; //Yellow
            case CTMLogLevel::Error:   return {"ERROR", "\033[31m"};   //Red
            default:                   return {"UNKNOWN", "\033[0m"};  //Reset color to default
        }
    }
};

//Macros to make my life easier
#define CTM_LOG_ERROR(...)   CTMLogger::Log(CTMLogLevel::Error,   '\n', __VA_ARGS__)
#define CTM_LOG_WARNING(...) CTMLogger::Log(CTMLogLevel::Warning, '\n', __VA_ARGS__)
#define CTM_LOG_SUCCESS(...) CTMLogger::Log(CTMLogLevel::Success, '\n', __VA_ARGS__)
#define CTM_LOG_INFO(...)    CTMLogger::Log(CTMLogLevel::Info,    '\n', __VA_ARGS__)

//Macros without newline
#define CTM_LOG_ERROR_NONL(...)   CTMLogger::Log(CTMLogLevel::Error,   '\0', __VA_ARGS__)
#define CTM_LOG_WARNING_NONL(...) CTMLogger::Log(CTMLogLevel::Warning, '\0', __VA_ARGS__)

//Macros outputting pure text
#define CTM_LOG_TEXT(...) CTMLogger::LogPureText(__VA_ARGS__)

#endif
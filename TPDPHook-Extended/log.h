#pragma once
#include "typedefs.h"
#include "textconvert.h"
#include <string>
#include <sstream>

enum class LogLevel
{
    fatal,
    error,
    warn,
    info,
    debug,
    trace
};

HOOKAPI void log_set_level(LogLevel level);
HOOKAPI LogLevel log_get_level();

[[noreturn]] HOOKAPI void log_fatal(const wchar_t *msg); // calls abort()
HOOKAPI void log_error(const wchar_t *msg);
HOOKAPI void log_warn(const wchar_t *msg, bool show_msgbox = false);
HOOKAPI void log_info(const wchar_t *msg, bool show_msgbox = false);
HOOKAPI void log_debug(const wchar_t *msg);
HOOKAPI void log_trace(const wchar_t *msg);

#pragma warning(push)
#pragma warning(disable:4722)
template<LogLevel level>
class LogStream
{
private:
    std::wstringstream strm_;

public:
    LogStream() = default;
    ~LogStream()
    {
        switch(level)
        {
        case LogLevel::fatal: // FIXME: warning C4722 (intentional, but annoying)
            log_fatal(strm_.str().c_str()); // calls abort()
            break;
        case LogLevel::error:
            log_error(strm_.str().c_str());
            break;
        case LogLevel::warn:
            log_warn(strm_.str().c_str());
            break;
        case LogLevel::info:
            log_info(strm_.str().c_str());
            break;
        case LogLevel::debug:
            log_debug(strm_.str().c_str());
            break;
        case LogLevel::trace:
            log_trace(strm_.str().c_str());
            break;
        default:
            break;
        }
    }

    template<typename T>
    LogStream& operator<<(const T& obj)
    {
        strm_ << obj;
        return *this;
    }
    LogStream& operator<<(const std::string& str)
    {
        strm_ << ansi_to_utf(str);
        return *this;
    }
    LogStream& operator<<(const char *str)
    {
        strm_ << ansi_to_utf(str);
        return *this;
    }
};
#pragma warning(pop)

typedef LogStream<LogLevel::fatal> LOG_FATAL;
typedef LogStream<LogLevel::error> LOG_ERROR;
typedef LogStream<LogLevel::warn> LOG_WARN;
typedef LogStream<LogLevel::info> LOG_INFO;
typedef LogStream<LogLevel::debug> LOG_DEBUG;
typedef LogStream<LogLevel::trace> LOG_TRACE;

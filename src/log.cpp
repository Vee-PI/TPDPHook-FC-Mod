#include <fstream>
#include "log.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

LogLevel g_level = LogLevel::debug;
std::wofstream g_logfile;
std::wstring g_lastlog;
std::size_t g_repcount = 0;

void log_set_level(LogLevel level)
{
    g_level = level;
}

LogLevel log_get_level()
{
    return g_level;
}

static void log_file(const wchar_t *msg, LogLevel level)
{
    std::wstring prefix;
    switch(level)
    {
    case LogLevel::fatal:
        prefix = L"FATAL: ";
        break;
    case LogLevel::error:
        prefix = L"ERROR: ";
        break;
    case LogLevel::warn:
        prefix = L"WARNING: ";
        break;
    case LogLevel::info:
        prefix = L"INFO: ";
        break;
    case LogLevel::debug:
        prefix = L"DEBUG: ";
        break;
    case LogLevel::trace:
        prefix = L"TRACE: ";
        break;
    default:
        prefix = L"ERROR: ";
        break;
    }

    if(!g_logfile.is_open())
        g_logfile.open("TPDPHook.log", std::ios::trunc);

    if(!g_lastlog.empty() && (msg == g_lastlog))
    {
        ++g_repcount;
        return;
    }
    else if((g_repcount > 0) && (msg != g_lastlog))
    {
        g_logfile << L"Previous message repeated " << g_repcount << L" times.\n\n";
        g_repcount = 0;
    }

    g_lastlog = msg;
    g_logfile << prefix << msg << L"\n\n";
    g_logfile.flush();
}

void log_fatal(const wchar_t *msg)
{
    log_file(msg, LogLevel::fatal);
    MessageBoxW(GetActiveWindow(), msg, L"TPDPHook Fatal Error", MB_ICONERROR | MB_OK);
    abort();
}

void log_error(const wchar_t *msg)
{
    log_file(msg, LogLevel::error);
    MessageBoxW(GetActiveWindow(), msg, L"TPDPHook Error", MB_ICONERROR | MB_OK);
}

void log_warn(const wchar_t *msg, bool show_msgbox)
{
    if(g_level >= LogLevel::warn)
    {
        log_file(msg, LogLevel::warn);
        if(show_msgbox)
            MessageBoxW(GetActiveWindow(), msg, L"TPDPHook", MB_ICONEXCLAMATION | MB_OK);
    }
}

void log_info(const wchar_t *msg, bool show_msgbox)
{
    if(g_level >= LogLevel::info)
    {
        log_file(msg, LogLevel::info);
        if(show_msgbox)
            MessageBoxW(GetActiveWindow(), msg, L"TPDPHook", MB_ICONINFORMATION | MB_OK);
    }
}

void log_debug(const wchar_t *msg)
{
    if(g_level >= LogLevel::debug)
    {
        log_file(msg, LogLevel::debug);
    }
}

void log_trace(const wchar_t *msg)
{
    if(g_level >= LogLevel::trace)
    {
        log_file(msg, LogLevel::trace);
    }
}

#include <filesystem>
#include <sstream>
#include "ini_parser.h"
#include "hook.h"
#include "log.h"
#include "tpdp/hook_tpdp.h"
#include "dxgi/hook_dxgi.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

HMODULE g_hmodule = nullptr;

static void read_config()
{
    IniFile::global.read("TPDPHook.ini");
    const auto& level = IniFile::global["general"]["logging"];
    if(level == "trace")
        log_set_level(LogLevel::trace);
    else if(level == "debug")
        log_set_level(LogLevel::debug);
    else if(level == "info")
        log_set_level(LogLevel::info);
    else if(level == "warning")
        log_set_level(LogLevel::warn);
    else if(level == "warn")
        log_set_level(LogLevel::warn);
    else if(level == "error")
        log_set_level(LogLevel::error);
    else if(IniFile::global["general"]["debug"] == "true") // backwards compatibility
        log_set_level(LogLevel::debug);
    else
        log_set_level(LogLevel::error);
}

BOOL WINAPI DllMain(HMODULE hModule, DWORD fdwReason, [[maybe_unused]] LPVOID lpvReserved)
{
    switch(fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        g_hmodule = hModule;
        RVA::base((uintptr_t)GetModuleHandleW(nullptr));

        // global config
        read_config();

        LOG_DEBUG() << std::hex << std::showbase << L"Image Base: " << RVA::base();

        // now done in tpdphook_early_init()
        // see hook_tpdp.cpp
        //tpdp_install_hooks();
        break;

    case DLL_PROCESS_DETACH:
        tpdp_uninstall_hooks();
        dxgi_uninstall_hooks();
        break;

    default:
        break;
    }

    return TRUE;
}

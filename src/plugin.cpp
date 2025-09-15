#include "plugin.h"
#include "log.h"
#include "typedefs.h"
#include <filesystem>
#include <cwctype>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

void load_plugins()
{
    if(!std::filesystem::exists(L"./plugins") || !std::filesystem::is_directory(L"./plugins"))
    {
        log_debug(L"Could not locate plugin folder.");
        return;
    }

    for(auto& entry : std::filesystem::directory_iterator(L"./plugins"))
    {
        auto& path = entry.path();
        auto ext = path.extension().wstring();
        for(auto& c : ext)
            c = std::towlower(c);

        if(ext == L".dll")
        {
            auto hm = LoadLibraryW(path.c_str());
            if(hm == nullptr)
            {
                LOG_WARN() << L"Failed to load plugin: " << path.wstring();
                return;
            }

            auto proc = (VoidCall)GetProcAddress(hm, "hook_plugin_init");
            if(proc == nullptr)
            {
                LOG_WARN() << L"Could not locate hook_plugin_init in module: " << path.wstring();
                FreeLibrary(hm);
                return;
            }

            proc();
        }
    }
}

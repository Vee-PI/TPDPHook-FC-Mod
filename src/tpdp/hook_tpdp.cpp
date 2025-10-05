#include <memory>
#include <unordered_map>
#include <filesystem>
#include <fstream>
#include <vector>
#include "hook_tpdp.h"
#include "../ini_parser.h"
#include "../hook.h"
#include "../log.h"
#include "../version.h"
#include "../plugin.h"
#include "../textconvert.h"
#include "../network.h"
#include "custom_skills.h"
#include "misc_hacks.h"
#include "custom_abilities.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <ShlObj.h>

// This file contains most of the basic infrastructure that supports TPDPHook
// Early setup happens here, as well as the high level skill API

VoidCall g_anim_init_func = nullptr;
VoidCall g_effect_init_func = nullptr;

bool g_hooks_installed = false;
bool g_translated = false;

const char *g_wnd_title = "%s ver%.3f (TPDPHook v" TPDPHOOK_VERSION_STRING ") [%04.1ffps]";
unsigned int g_dxhandle_limit = ID_NONE;

// skill animation function pointer tables
uint32_t *g_anim_tables[5] = {};

// skill effect function pointer tables
uint32_t *g_effect_tables[6] = {};

std::unique_ptr<uint32_t[]> g_orig_anims[] = {
    std::make_unique<uint32_t[]>(1024),
    std::make_unique<uint32_t[]>(1024),
    std::make_unique<uint32_t[]>(1024),
    std::make_unique<uint32_t[]>(1024),
    std::make_unique<uint32_t[]>(1024)
};

std::unique_ptr<uint32_t[]> g_orig_effects[] = {
    std::make_unique<uint32_t[]>(1024),
    std::make_unique<uint32_t[]>(1024),
    std::make_unique<uint32_t[]>(1024),
    std::make_unique<uint32_t[]>(1024),
    std::make_unique<uint32_t[]>(1024),
    std::make_unique<uint32_t[]>(1024)
};

bool tpdp_hooks_installed()
{
    return g_hooks_installed;
}
bool tpdp_eng_translation()
{
    return g_translated;
}

// return path to gn_enbu.ini
static std::filesystem::path get_ini_path()
{
    PWSTR roaming = nullptr;
    if(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &roaming) != S_OK)
    {
        CoTaskMemFree(roaming);
        log_warn(L"SHGetKnownFolderPath failed.");
        return {};
    }

    auto path = std::filesystem::path(roaming) / L"FocasLens" / L"幻想人形演舞" / L"gn_enbu.ini";
    CoTaskMemFree(roaming);

    return path;
}

// read gn_enbu.ini to get base game install path
static std::filesystem::path get_basegame_path()
{
    auto ini = get_ini_path();
    if(ini.empty() || !std::filesystem::exists(ini))
        return {};

    std::ifstream f(ini, std::ios::binary);
    auto sz = (std::size_t)std::filesystem::file_size(ini);

    auto buf = std::make_unique<char[]>(sz);
    if(!f.read(buf.get(), sz))
        return {};

    try
    {
        auto str = sjis_to_utf(buf.get(), sz);
        std::wstring path;
        auto pos = str.find(L"InstallPath=");
        if(pos != std::string::npos)
        {
            pos += 12;
            path = str.substr(pos, str.find(L"\r\n", pos) - pos);
            return path;
        }
    }
    catch(...)
    {
        return {};
    }

    return {};
}

// assign skill function to effect ID
void register_custom_skill(unsigned int effect_id, SkillCall skill_func)
{
    g_effect_tables[5][effect_id] = (uint32_t)skill_func;
}

// assign skill activation function to effect ID
void register_custom_skill_activator(unsigned int effect_id, ActivatorCall activator_func)
{
    g_effect_tables[2][effect_id] = (uint32_t)activator_func;
}

static int no_op()
{
    return 0;
}

void register_custom_skill_anim(unsigned int id, VoidCall resource_loader, VoidCall resource_deleter, DrawCall1 draw1, DrawCall2 draw2, TickCall tick)
{
    auto noop = (uint32_t)&no_op;
    g_anim_tables[1][id] = (draw1 != nullptr) ? (uint32_t)draw1 : noop;
    g_anim_tables[0][id] = (draw2 != nullptr) ? (uint32_t)draw2 : noop;
    g_anim_tables[2][id] = (tick != nullptr) ? (uint32_t)tick : noop;
    g_anim_tables[3][id] = (resource_loader != nullptr) ? (uint32_t)resource_loader : noop;
    g_anim_tables[4][id] = (resource_deleter != nullptr) ? (uint32_t)resource_deleter : noop;
}

void copy_skill_anim(unsigned int src, unsigned int dest)
{
    if(src >= 1024)
    {
        LOG_WARN() << L"Skill anim source ID out of range: " << src;
        return;
    }
    if(dest >= 1024)
    {
        LOG_WARN() << L"Skill anim dest ID out of range: " << dest;
        return;
    }

    for(auto i = 0; i < 5; ++i)
        g_anim_tables[i][dest] = g_orig_anims[i][src];
}

void copy_skill_effect(unsigned int src, unsigned int dest)
{
    if(src >= 1024)
    {
        LOG_WARN() << L"Skill anim source ID out of range: " << src;
        return;
    }
    if(dest >= 1024)
    {
        LOG_WARN() << L"Skill anim dest ID out of range: " << dest;
        return;
    }

    for(auto i = 0; i < 6; ++i)
        g_effect_tables[i][dest] = g_orig_effects[i][src];
}

void init_new_skill(unsigned int id)
{
    if(id >= 1024)
    {
        LOG_WARN() << L"Skill ID out of range: " << id;
        return;
    }

    for(auto i = 0; i < 6; ++i)
        g_effect_tables[i][id] = g_orig_effects[i][0];
}

// initialize skill animations
// maps function pointers to skill IDs
void hook_init_anims(void)
{
    g_anim_init_func();

    for(auto i = 0; i < 5; ++i)
        memcpy(g_orig_anims[i].get(), g_anim_tables[i], 4096);

    std::unordered_map<std::size_t, std::size_t> map;
    for(auto& it : IniFile::global["skill_anims"])
    {
        std::size_t k, v;

        try
        {
            k = std::stoul(it.first);
            v = std::stoul(it.second);
        }
        catch(...)
        {
            continue;
        }

        map[k] = v;
    }

    for(auto it = map.begin(); it != map.end();)
    {
        auto k = it->first;
        auto v = it->second;
        if(k >= 1024)
        {
            LOG_DEBUG() <<  L"Skill ID out of range: " << k;
            it = map.erase(it);
            continue;
        }
        if(v >= 1024)
        {
            LOG_DEBUG() << L"Skill ID out of range: " << v;
            it = map.erase(it);
            continue;
        }

        for(auto i = 0; i < 5; ++i)
            g_anim_tables[i][k] = g_orig_anims[i][v];
        ++it;
    }

    LOG_DEBUG() << L"Skill hook successful.\nPatched " << map.size() << L" skills.";
}

// initialize skill effects
// maps function pointers to effect IDs
void hook_init_effects(void)
{
    g_effect_init_func();

    for(auto i = 0; i < 6; ++i)
        memcpy(g_orig_effects[i].get(), g_effect_tables[i], 4096);

    std::unordered_map<std::size_t, std::size_t> map;
    for(auto& it : IniFile::global["effects"])
    {
        std::size_t k, v;

        try
        {
            k = std::stoul(it.first);
            v = std::stoul(it.second);
        }
        catch(...)
        {
            continue;
        }

        map[k] = v;
    }

    for(auto it = map.begin(); it != map.end();)
    {
        auto k = it->first;
        auto v = it->second;
        if(k >= 1024)
        {
            LOG_DEBUG() << L"Effect value out of range: " << k;
            it = map.erase(it);
            continue;
        }
        if(v >= 1024)
        {
            LOG_DEBUG() << L"Effect value out of range: " << v;
            it = map.erase(it);
            continue;
        }

        for(auto i = 0; i < 6; ++i)
            g_effect_tables[i][k] = g_orig_effects[i][v];
        ++it;
    }

    init_custom_skills();

    LOG_DEBUG() << L"Effect hook successful.\nPatched " << map.size() << L" effects.";

    load_plugins();
}

// fix disappearing sprites bug
// override maximum dxlib handles
int __cdecl dxhandlemgr_override(int type, int objsz, int /*maxnum*/, void *init_func, void *term_func, void *name)
{
    return RVA(0x1f80d0).ptr<int(__cdecl*)(int,int,int,void*,void*,void*)>()(type, objsz, g_dxhandle_limit, init_func, term_func, name);
}

// patch all the things
void tpdp_install_hooks()
{
    g_anim_init_func = RVA(0x1402f0).ptr<VoidCall>();
    g_effect_init_func = RVA(0x469c0).ptr<VoidCall>();
    patch_call(RVA(0x17e3ef), &hook_init_anims);
    patch_call(RVA(0x17e3f4), &hook_init_effects);
    
    g_anim_tables[0] = RVA(0x946500).ptr<uint32_t*>();
    g_anim_tables[1] = RVA(0x947528).ptr<uint32_t*>();
    g_anim_tables[2] = RVA(0x94b938).ptr<uint32_t*>();
    g_anim_tables[3] = RVA(0x94c9f8).ptr<uint32_t*>();
    g_anim_tables[4] = RVA(0x94dad0).ptr<uint32_t*>();

    g_effect_tables[0] = RVA(0x93df98).ptr<uint32_t*>();
    g_effect_tables[1] = RVA(0x93efa0).ptr<uint32_t*>();
    g_effect_tables[2] = RVA(0x93ffa0).ptr<uint32_t*>();
    g_effect_tables[3] = RVA(0x940fa0).ptr<uint32_t*>();
    g_effect_tables[4] = RVA(0x941fb0).ptr<uint32_t*>();
    g_effect_tables[5] = RVA(0x942fb0).ptr<uint32_t*>();

    if(IniFile::global.get_bool("general", "set_window_title"))
    {
        auto mem = RVA(0x1af5f1).ptr<uint8_t*>();
        unsigned int opcode = *mem;
        if(opcode == 0x68u)
        {
            auto ptr = g_wnd_title;
            patch_memory(RVA(0x1af5f2), &ptr, 4);
        }
        else
        {
            LOG_FATAL() << std::hex << std::showbase << L"Failed to patch memory, unexpected opcode: " << opcode << L"\n"
                        << L"address: " << (uintptr_t)mem << L"\n";
        }
    }

    g_hooks_installed = true;
    init_networking();
    init_misc_hacks();
    init_custom_abilities();
}

void tpdp_uninstall_hooks()
{
    // uninstall? pffft!
    shutdown_networking();
}

// GetPrivateProfileStringA
static DWORD __stdcall gpps_override(
    LPCSTR /*lpAppName*/,
    LPCSTR /*lpKeyName*/,
    LPCSTR lpDefault,
    LPSTR  lpReturnedString,
    DWORD  nSize,
    LPCSTR /*lpFileName*/)
{
    auto path = utf_to_sjis(get_basegame_path().wstring());
    if(path.empty() || (path.size() >= nSize))
        path = lpDefault;

    auto sz = path.size();
    path.copy(lpReturnedString, sz);
    lpReturnedString[sz] = 0;
    return sz;
}

static void check_installpath()
{
    auto path = get_basegame_path();
    if(!path.empty() && std::filesystem::exists(path / L"dat" / L"gn_dat1.arc"))
        return;

    auto msg = L"Could not locate base game \"Touhou Puppet Dance Performance\"\r\n"
                "Shard of Dreams expansion cannot run without the base game installed.\r\n"
                "Please locate base game folder (NOT shard of dreams folder)";
    MessageBoxW(nullptr, msg, L"Base game missing", MB_OK | MB_ICONEXCLAMATION);

    wchar_t buf[MAX_PATH] = {};
    BROWSEINFOW bi = {};
    bi.pszDisplayName = buf;
    bi.lpszTitle = L"Select Base Game Folder";
    bi.ulFlags = BIF_USENEWUI;

    PIDLIST_ABSOLUTE pl = SHBrowseForFolderW(&bi);
    if(!pl)
        return;

    SHGetPathFromIDListW(pl, buf);
    CoTaskMemFree(pl);

    auto ini = utf_to_sjis(std::wstring(L"[InstallSettings]\r\nInstallPath=") + buf + L"\r\n");
    auto outpath = get_ini_path();
    if(outpath.empty())
        return;

    std::filesystem::create_directories(outpath.parent_path());
    std::ofstream f(outpath, std::ios::binary | std::ios::trunc);
    f.write(ini.data(), ini.size());
}

static void patch_install_check()
{
    // replacement instruction smaller than original
    // fill with nops
    uint8_t buf[] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };
    patch_memory(RVA(0x10a0), buf, sizeof(buf));
    write_call(RVA(0x10a0), &gpps_override);
}

// called after CRT initialization
// but before construction of globals
// this hook is patched in by tpdphook_early_init rather than baked into the exe
static int __cdecl global_init(int param_1)
{
    log_trace(L"global_init");

    if(IniFile::global.get_bool("general", "force_jp_locale"))
    {
        auto func = RVA(0x2f2fc7).ptr<char*(__cdecl*)(int, const char*)>(); // C library setlocale
        func(LC_ALL, "Japanese_Japan.932");
    }

    return RVA(0x2f14e1).ptr<int(__cdecl*)(int)>()(param_1);
}

// NOTE: early_init and main_init are called by a modified
// game executable

// called immediately at entry point
// before CRT initialization
extern "C" __declspec(dllexport) void tpdphook_early_init()
{
    log_trace(L"early_init");

    patch_call(RVA(0x2f3cb5), &global_init);

    if (IniFile::global.get_bool("general", "auto_repair_installpath"))
    {
        check_installpath();
        patch_install_check();
    }

    if (IniFile::global.get_bool("general", "force_jp_locale"))
    {
        SetThreadLocale(MAKELCID(MAKELANGID(LANG_JAPANESE, SUBLANG_JAPANESE_JAPAN), SORT_DEFAULT));
        SetThreadUILanguage(MAKELANGID(LANG_JAPANESE, SUBLANG_JAPANESE_JAPAN));

        // push imm32, push imm8
        uint8_t buf[] = { 0x68, 0, 0, 0, 0, 0x6a, LC_ALL };
        *(const char**)&buf[1] = "Japanese_Japan.932";

        // fix existing call to setlocale
        patch_memory(RVA(0x17e318), buf, sizeof(buf));
    }

    g_translated = (std::string(*RVA(0x1af5f2).ptr<const char**>()).find("Translation") != std::string::npos);
    if (g_translated)
    {
        // translate base game missing message (only if using eng patch)
        auto msg = "Could not locate base game \"Touhou Puppet Dance Performance\"\r\n"
            "Shard of Dreams expansion cannot run without the base game installed.\r\n"
            "If the base game is already installed, try re-running the installer with japanese locale.";
        patch_memory(RVA(0x11a8), &msg, 4);
    }

    g_dxhandle_limit = IniFile::global.get_uint("general", "dxhandle_limit_override");
    if (g_dxhandle_limit != ID_NONE)
        scan_and_replace_call(RVA(0x1f80d0), &dxhandlemgr_override);
}

// called immediately in main()
extern "C" __declspec(dllexport) void tpdphook_main_init()
{
    log_trace(L"main_init");

    tpdp_install_hooks();
}

#include <filesystem>
#include <mutex>
#include "../log.h"
#include "hook_dxgi.h"
#include "../tpdp/hook_tpdp.h"
#include "../hook.h"
#include "../ini_parser.h"
#include "dxgi_impl.h"

#include <ShlObj.h>

// DXGI API proxy implementation

typedef HRESULT(WINAPI *DXGICall)(REFIID riid, void **ppFactory);
typedef HRESULT(WINAPI *DXGICall2)(UINT Flags, REFIID riid, void **ppFactory);

DXGICall g_dxgi_func = nullptr;
DXGICall g_dxgi_func1 = nullptr;
DXGICall2 g_dxgi_func2 = nullptr;

void *g_CompatString = nullptr;
void *g_CompatValue = nullptr;
void *g_DXGID3D10CreateDevice = nullptr;
void *g_DXGID3D10CreateLayeredDevice = nullptr;
void *g_DXGID3D10GetLayeredDeviceSize = nullptr;
void *g_DXGID3D10RegisterLayers = nullptr;
void *g_DXGIDeclareAdapterRemovalSupport = nullptr;
void *g_DXGIDumpJournal = nullptr;
void *g_DXGIGetDebugInterface1 = nullptr;
void *g_DXGIReportAdapterConfiguration = nullptr;

std::once_flag dxgi_initialized;

static void __stdcall missing_export(const wchar_t *str)
{
    LOG_DEBUG() << L"Call to missing DXGI export: " << str;
}

#define INIT_EXPORT(NAME) g_ ## NAME = (void*)GetProcAddress(hmodule, #NAME )
static void init_dxgi()
{
    PWSTR syspath = nullptr;
    if(SHGetKnownFolderPath(FOLDERID_SystemX86, 0, NULL, &syspath) != S_OK)
    {
        CoTaskMemFree(syspath);
        log_fatal(L"SHGetKnownFolderPath failed.");
    }

    auto path = std::filesystem::path(syspath) / L"dxgi.dll";
    CoTaskMemFree(syspath);

    LOG_TRACE() << L"Loading: " << path.c_str();
    auto hmodule = LoadLibraryW(path.c_str());

    if(hmodule == nullptr)
        log_fatal((L"Could not load " + path.wstring()).c_str());

    g_dxgi_func = (DXGICall)GetProcAddress(hmodule, "CreateDXGIFactory");
    if(g_dxgi_func == nullptr)
        log_fatal(L"CreateDXGIFactory missing");

    g_dxgi_func1 = (DXGICall)GetProcAddress(hmodule, "CreateDXGIFactory1");
    if(g_dxgi_func1 == nullptr)
        log_fatal(L"CreateDXGIFactory1 missing");

    g_dxgi_func2 = (DXGICall2)GetProcAddress(hmodule, "CreateDXGIFactory2");
    if(g_dxgi_func2 == nullptr)
        log_warn(L"CreateDXGIFactory2 missing");

    INIT_EXPORT(CompatString);
    INIT_EXPORT(CompatValue);
    INIT_EXPORT(DXGID3D10CreateDevice);
    INIT_EXPORT(DXGID3D10CreateLayeredDevice);
    INIT_EXPORT(DXGID3D10GetLayeredDeviceSize);
    INIT_EXPORT(DXGID3D10RegisterLayers);
    INIT_EXPORT(DXGIDeclareAdapterRemovalSupport);
    INIT_EXPORT(DXGIDumpJournal);
    INIT_EXPORT(DXGIGetDebugInterface1);
    INIT_EXPORT(DXGIReportAdapterConfiguration);

    log_debug(L"DXGI initialized.");

    if(!tpdp_hooks_installed())
    {
        // If we get here, it means someone started the game with the normal game exe
        // instead of our custom one.
        // Warn them about it in their respective languages.
        if(std::string(*RVA(0x1af5f2).ptr<const char**>()).find("Translation") != std::string::npos)
            log_error(L"Please launch the game using \"sod_extended.exe\"\r\nExtended mod will not work properly without it!");
        else
            log_error(L"ゲームを「sod_extended.exe」で開始してください。\r\nそうしなければ、ゲームが起動しません。");
    }
}

HRESULT WINAPI CreateDXGIFactory(REFIID riid, void **ppFactory)
{
    std::call_once(dxgi_initialized, &init_dxgi);

    auto fullscreen = IniFile::global.get_bool("general", "allow_fullscreen");
#if 0
    if(fullscreen)
    {
        if(g_dxgi_func2 != nullptr)
        {
            log_debug(L"Redirecting CreateDXGIFactory to CreateDXGIFactory2.");
            auto result = g_dxgi_func2(0, riid, ppFactory);
            hook_dxgifactory(*ppFactory);
            return result;
        }
        else
            log_warn(L"allow_fullscreen specified but CreateDXGIFactory2 not available.");
    }
#endif

    log_debug(L"Redirecting CreateDXGIFactory.");

    auto result = g_dxgi_func(riid, ppFactory);
    if(fullscreen)
        hook_dxgifactory(*ppFactory);
    return result;
}

HRESULT WINAPI CreateDXGIFactory1(REFIID riid, void **ppFactory)
{
    std::call_once(dxgi_initialized, &init_dxgi);

    log_debug(L"Redirecting CreateDXGIFactory1.");

    return g_dxgi_func1(riid, ppFactory);
}

HRESULT WINAPI CreateDXGIFactory2(UINT Flags, REFIID riid, void **ppFactory)
{
    std::call_once(dxgi_initialized, &init_dxgi);

    auto msg = (g_dxgi_func2 != nullptr) ? L"Redirecting CreateDXGIFactory2." : L"Redirecting CreateDXGIFactory2 to CreateDXGIFactory1.";
    log_debug(msg);

    if(g_dxgi_func2 != nullptr)
        return g_dxgi_func2(Flags, riid, ppFactory);
    else
        return g_dxgi_func1(riid, ppFactory); // This probably works?
}

// Undocumented internal APIs
// Shouldn't get called, but pass them through if present

// funny macro for defining jump indirection stubs
#define DEF_PASSTHRU(NAME, RETVAL, RETSZ)			\
__declspec(naked)									\
HRESULT WINAPI NAME ()								\
{													\
    static auto funcname = L ## #NAME;				\
                                                    \
    __asm											\
    {												\
        __asm cmp g_ ## NAME, 0						\
        __asm jz nojmp								\
        __asm jmp g_ ## NAME						\
        __asm nojmp:								\
        __asm push funcname							\
        __asm call missing_export					\
        __asm mov eax, RETVAL						\
        __asm ret RETSZ								\
    }												\
}

// 0x80004001 = E_NOTIMPL
// 0x80004002 = E_NOINTERFACE
DEF_PASSTHRU(CompatString, 0, 0x10);
DEF_PASSTHRU(CompatValue, 0, 0x8);
DEF_PASSTHRU(DXGID3D10CreateDevice, 0x80004001, 0x18);
DEF_PASSTHRU(DXGID3D10CreateLayeredDevice, 0x80004001, 0x14);
DEF_PASSTHRU(DXGID3D10GetLayeredDeviceSize, 0, 0x8);
DEF_PASSTHRU(DXGID3D10RegisterLayers, 0x80004001, 0x8);
DEF_PASSTHRU(DXGIDeclareAdapterRemovalSupport, 0, 0);
DEF_PASSTHRU(DXGIDumpJournal, 0x80004001, 0x4);
DEF_PASSTHRU(DXGIGetDebugInterface1, 0x80004002, 0xc);
DEF_PASSTHRU(DXGIReportAdapterConfiguration, 0x80004001, 0x4);


void dxgi_uninstall_hooks()
{
    dxgi_impl_unhook();
}

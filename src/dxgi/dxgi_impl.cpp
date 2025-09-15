#include "dxgi_impl.h"
#include "../hook.h"
#include "../log.h"
#include <map>

#define CINTERFACE
#include <dxgi1_3.h>

std::map<void*, void*> g_hooks; // mem_addr, orig_func

// scale mode with preserved aspect ratio impossible with HWND backend
#if 0
typedef decltype(IDXGIFactory2Vtbl::MakeWindowAssociation) MWAFunc;
typedef decltype(IDXGIFactory2Vtbl::CreateSwapChain) CSCFunc;

HRESULT STDMETHODCALLTYPE mwa_override(IDXGIFactory2 *This, HWND WindowHandle, [[maybe_unused]] UINT Flags)
{
    auto vtable = This->lpVtbl;
    auto addr = &vtable->MakeWindowAssociation;
    if(!g_hooks.count(addr))
        log_fatal(L"MakeWindowAssociation called without hook!");
    return MWAFunc(g_hooks[addr])(This, WindowHandle, 0);
}

HRESULT STDMETHODCALLTYPE csc_override(IDXGIFactory2 *This, IUnknown *pDevice, DXGI_SWAP_CHAIN_DESC *pDesc, IDXGISwapChain **ppSwapChain)
{
    auto vtable = This->lpVtbl;
    auto addr = &vtable->CreateSwapChain;
    if(!g_hooks.count(addr))
        log_fatal(L"CreateSwapChain called without hook!");

    DXGI_SWAP_CHAIN_DESC1 newdesc{};
    newdesc.Width = pDesc->BufferDesc.Width;
    newdesc.Height = pDesc->BufferDesc.Height;
    newdesc.Format = pDesc->BufferDesc.Format;
    newdesc.BufferUsage = pDesc->BufferUsage;
    newdesc.BufferCount = pDesc->BufferCount;
    newdesc.SampleDesc = pDesc->SampleDesc;
    //newdesc.Scaling = DXGI_SCALING_ASPECT_RATIO_STRETCH;
    newdesc.Scaling = DXGI_SCALING_STRETCH;
    newdesc.SwapEffect = pDesc->SwapEffect;
    newdesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    newdesc.Flags = pDesc->Flags;

    DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsdesc{};
    fsdesc.RefreshRate = pDesc->BufferDesc.RefreshRate;
    fsdesc.ScanlineOrdering = pDesc->BufferDesc.ScanlineOrdering;
    fsdesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    //fsdesc.Windowed = pDesc->Windowed;
    fsdesc.Windowed = true;

    auto result = vtable->CreateSwapChainForHwnd(This, pDevice, pDesc->OutputWindow, &newdesc, &fsdesc, nullptr, (IDXGISwapChain1**)ppSwapChain);

    vtable->MakeWindowAssociation(This, pDesc->OutputWindow, 0);

    return result;
}

void hook_dxgifactory(void *ptr)
{
    log_debug(L"Hooking DXGIFactory vtable.");
    auto vtable = ((IDXGIFactory2*)ptr)->lpVtbl;

    void *addr = &vtable->MakeWindowAssociation;
    if(!g_hooks.count(addr))
    {
        g_hooks[addr] = vtable->MakeWindowAssociation;
        auto buf = &mwa_override;
        patch_memory(addr, &buf, sizeof(buf));
    }

    addr = &vtable->CreateSwapChain;
    if(!g_hooks.count(addr))
    {
        g_hooks[addr] = vtable->CreateSwapChain;
        auto buf = &csc_override;
        patch_memory(addr, &buf, sizeof(buf));
    }
}
#endif

typedef decltype(IDXGIFactoryVtbl::MakeWindowAssociation) MWAFunc;
typedef decltype(IDXGIFactoryVtbl::CreateSwapChain) CSCFunc;

HRESULT STDMETHODCALLTYPE mwa_override(IDXGIFactory *This, HWND WindowHandle, [[maybe_unused]] UINT Flags)
{
    auto vtable = This->lpVtbl;
    auto addr = &vtable->MakeWindowAssociation;
    if(!g_hooks.count(addr))
        log_fatal(L"MakeWindowAssociation called without hook!");
    return MWAFunc(g_hooks[addr])(This, WindowHandle, 0);
}

void hook_dxgifactory(void *ptr)
{
    log_debug(L"Hooking DXGIFactory vtable.");
    auto vtable = ((IDXGIFactory*)ptr)->lpVtbl;

    void *addr = &vtable->MakeWindowAssociation;
    if(!g_hooks.count(addr))
    {
        g_hooks[addr] = vtable->MakeWindowAssociation;
        auto buf = &mwa_override;
        patch_memory(addr, &buf, sizeof(buf));
    }
}

void dxgi_impl_unhook()
{
    for(auto& it : g_hooks)
        patch_memory(it.first, &it.second, sizeof(it.second));
}

#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

void dxgi_uninstall_hooks();

extern "C"
{

HRESULT WINAPI CreateDXGIFactory(REFIID riid, void **ppFactory);
HRESULT WINAPI CreateDXGIFactory1(REFIID riid, void **ppFactory);
HRESULT WINAPI CreateDXGIFactory2(UINT Flags, REFIID riid, void **ppFactory);

}

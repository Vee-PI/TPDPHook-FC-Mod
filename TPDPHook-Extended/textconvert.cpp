#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define CP_SJIS 932
#include <Windows.h>
#include "textconvert.h"

std::wstring sjis_to_utf(const std::string& str)
{
    std::wstring ret;

    if(str.empty())
        return ret;

    auto len = MultiByteToWideChar(CP_SJIS, MB_PRECOMPOSED, str.c_str(), (int)str.size(), NULL, 0);
    if(!len)
        return ret;

    ret.resize(len);
    if(!MultiByteToWideChar(CP_SJIS, MB_PRECOMPOSED, str.c_str(), (int)str.size(), &ret[0], len))
        return {};

    return ret;
}

std::wstring sjis_to_utf(const char *str, std::size_t sz)
{
    std::wstring ret;

    auto len = MultiByteToWideChar(CP_SJIS, MB_PRECOMPOSED, str, (int)sz, NULL, 0);
    if(!len)
        return ret;

    ret.resize(len);
    if(!MultiByteToWideChar(CP_SJIS, MB_PRECOMPOSED, str, (int)sz, &ret[0], len))
        return {};

    return ret;
}

std::string utf_to_sjis(const std::wstring& str)
{
    std::string ret;

    if(str.empty())
        return ret;

    auto len = WideCharToMultiByte(CP_SJIS, 0, str.c_str(), (int)str.size(), NULL, 0, NULL, NULL);
    if(!len)
        return ret;

    ret.resize(len);
    if(!WideCharToMultiByte(CP_SJIS, 0, str.c_str(), (int)str.size(), &ret[0], len, NULL, NULL))
        return {};

    return ret;
}

std::wstring utf_widen(const std::string& str)
{
    std::wstring ret;

    if(str.empty())
        return ret;

    auto len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
    if(!len)
        return ret;

    ret.resize(len);
    if(!MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &ret[0], len))
        return {};

    return ret;
}

std::string utf_narrow(const std::wstring& str)
{
    std::string ret;

    if(str.empty())
        return ret;

    auto len = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0, NULL, NULL);
    if(!len)
        return ret;

    ret.resize(len);
    if(!WideCharToMultiByte(CP_UTF8, 0, str.c_str(), (int)str.size(), &ret[0], len, NULL, NULL))
        return {};

    return ret;
}

std::wstring ansi_to_utf(const std::string& str)
{
    std::wstring ret;

    if(str.empty())
        return ret;

    auto len = MultiByteToWideChar(CP_ACP, 0, str.c_str(), (int)str.size(), NULL, 0);
    if(!len)
        return ret;

    ret.resize(len);
    if(!MultiByteToWideChar(CP_ACP, 0, str.c_str(), (int)str.size(), &ret[0], len))
        return {};

    return ret;
}

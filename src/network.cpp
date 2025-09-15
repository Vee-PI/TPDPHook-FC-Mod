#include "network.h"
#include "log.h"

#define NOMINMAX
#include <WinSock2.h>
#include <WS2tcpip.h>

static bool g_initialized = false;

void init_networking()
{
    WSADATA wsa{};

    auto result = WSAStartup(MAKEWORD(2, 2), &wsa);
    if(result != 0)
    {
        LOG_WARN() << L"WSAStartup failed with code: " << (unsigned int)result;
        return;
    }

    if((LOBYTE(wsa.wVersion) != 2) || (HIBYTE(wsa.wVersion) != 2))
    {
        LOG_WARN() << L"Winsock version mismatch.";
        WSACleanup();
        return;
    }

    g_initialized = true;
}

void shutdown_networking()
{
    if(g_initialized)
        WSACleanup();
}

uint32_t hostname_to_ipv4(const char *hostname)
{
    if(!g_initialized)
        return 0;

    ADDRINFOA hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    ADDRINFOA *result = nullptr;
    auto errc = getaddrinfo(hostname, nullptr, &hints, &result);
    if(errc != 0)
    {
        //wchar_t *str = nullptr;
        //FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        //               nullptr, errc, 0, (LPWSTR)&str, 0, nullptr);
        //LOG_WARN() << L"Failed to resolve hostname \"" << hostname << L"\": " << str;
        //LocalFree(str);
        LOG_WARN() << L"Attempt to resolve hostname \"" << hostname << L"\" failed with error code: " << errc;
        return 0;
    }

    uint32_t ret = 0;
    for(auto i = result; i != nullptr; i = i->ai_next)
    {
        if(i->ai_family == AF_INET)
        {
            ret = ((sockaddr_in*)i->ai_addr)->sin_addr.s_addr;
            break;
        }
    }
    freeaddrinfo(result);

    if(ret == 0)
        LOG_WARN() << L"No IPv4 address found for hostname \"" << hostname;

    return ret;
}

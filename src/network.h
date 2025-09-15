#pragma once
#include <cstdint>
#include <string>
#include "typedefs.h"

#ifdef HOOK_BUILD
void init_networking();
void shutdown_networking();
#endif

HOOKAPI uint32_t hostname_to_ipv4(const char *hostname);
static inline uint32_t hostname_to_ipv4(const std::string& hostname) { return hostname_to_ipv4(hostname.c_str()); }

#pragma once

#ifdef USE_INTERNAL_CONFIG
extern const char *G_INTERNAL_CONFIG;
#else
constexpr const char *G_INTERNAL_CONFIG = "";
#endif

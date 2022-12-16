#pragma once
#include <limits>

#ifdef HOOK_BUILD
#define HOOKAPI __declspec(dllexport)
#else
#define HOOKAPI __declspec(dllimport)
#endif // HOOK_BUILD

typedef void(*VoidCall)(void);
typedef int(__cdecl*SkillCall)(int,int);
typedef int(__cdecl*ActivatorCall)(int);

constexpr auto ID_NONE = std::numeric_limits<unsigned int>::max();

#pragma once
#include "../typedefs.h"

#ifdef HOOK_BUILD
void tpdp_install_hooks();
void tpdp_uninstall_hooks();
bool tpdp_hooks_installed();
bool tpdp_eng_translation();
#endif

HOOKAPI void register_custom_skill(unsigned int effect_id, SkillCall skill_func);
HOOKAPI void register_custom_skill_activator(unsigned int effect_id, ActivatorCall activator_func);
HOOKAPI void copy_skill_anim(unsigned int src, unsigned int dest);
HOOKAPI void init_new_skill(unsigned int id);
HOOKAPI void copy_skill_effect(unsigned int src, unsigned int dest);
HOOKAPI void register_custom_skill_anim(unsigned int id, VoidCall resource_loader, VoidCall resource_deleter, DrawCall1 draw1, DrawCall2 draw2, TickCall tick);

#pragma once

void init_misc_hacks();
void do_terrain_hook();
int do_possess();

extern bool g_form_changes[2][6];
extern unsigned int g_id_form_change;
extern unsigned int g_id_form_target;
extern unsigned int g_id_form_target_style;
int do_form_change(int player, int pid, int style, bool switchin, bool possess = false);
extern unsigned int g_id_possess;
extern unsigned int g_id_possess_target;
extern unsigned int g_id_possess_target_style;

struct WishState
{
    unsigned int turns = 0;
    unsigned int heal = 0;
};
extern WishState g_wish_state[2];

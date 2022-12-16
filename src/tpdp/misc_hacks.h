#pragma once

void init_misc_hacks();
void do_terrain_hook();

extern bool g_form_changes[2][6];
extern unsigned int g_id_form_change;
extern unsigned int g_id_form_target;
extern unsigned int g_id_form_target_style;
int do_form_change(int player, int pid, int style, bool switchin);

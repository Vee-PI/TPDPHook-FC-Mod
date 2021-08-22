#include "hook_tpdp.h"
#include "custom_skills.h"
#include "tpdp_data.h"
#include "../hook.h"
#include"../log.h"
#include "../ini_parser.h"
#include "../typedefs.h"
#include <vector>
#include <utility>
#include <algorithm>

// Implementations for custom skills go here.
// Register skill functions to their effect IDs in init_custom_skills()
// at the bottom of this file.

static auto g_godstone_hack = false;
static auto g_stargazer_hack = false;
static auto g_timegazer_hack = false;

static auto g_season_base = ID_NONE;
static auto g_season_duration = 5;

static auto g_id_delirium = ID_NONE;
static auto g_id_custom1 = ID_NONE;
static auto g_id_custom2 = ID_NONE;

typedef std::pair<Status, Status> StatusPair;
static std::vector<StatusPair> g_calamity_statuses;

int __cdecl season_activator(int /*player*/)
{
    skill_succeeded() = 1;
    return 1;
}

// Realm skill modified to work with timegazer/stargazer.
// Invoked from wrapper function below.
int skill_realm(int player, int /*effect_chance*/, int terrain, int weather, bool season = false)
{
    static auto frame_count = 0;

    auto& skill_state = get_skill_state();
    auto battle_state = get_battle_state(player);

    bool has_stargazer = battle_state->active_ability == 318;
    bool has_timegazer = battle_state->active_ability == 319;

    auto held_item = decrypt_puppet(battle_state->active_puppet).held_item;

    int min_duration = 5;
    if(g_godstone_hack && ((held_item == 303) || ((weather + 303) == held_item)))
        min_duration = 8;

    int weather_duration = (g_stargazer_hack && has_stargazer) ? 0x7ffffff5 : min_duration; // value apparently used by weather skills with stargazer
    int terrain_duration = (g_timegazer_hack && has_timegazer) ? 8 : 5;

    if(season)
    {
        weather_duration = g_season_duration;
        terrain_duration = g_season_duration;
    }

    switch(skill_state)
    {
    case 0: // initial state, first call
        frame_count = 0;
        if(skill_succeeded())
        {
            // set weather
            auto state = get_terrain_state();
            state->weather_type = (uint8_t)weather;
            state->weather_duration = weather_duration;

            reset_weather_msg();
            skill_state = 1;
            return 1;
        }
        skill_state = 3;
        break;

    case 1: // show weather message
        if(do_weather_msg() == 0)
        {
            // set terrain
            auto state = get_terrain_state();
            state->terrain_type = (uint8_t)terrain;
            state->terrain_duration = terrain_duration;

            reset_terrain_msg();
            clear_battle_text();
            skill_state = 2;
            return 1;
        }
        break;

    case 2: // show terrain message
        if(do_terrain_msg() == 0)
        {
            skill_state = 10;
            return 0;
        }
        break;

    case 3: // skill failed
        if(set_battle_text("But the skill failed.", 0, 0) != 1)
        {
            if(++frame_count > (is_game_60fps() ? 60 : 30))
            {
                clear_battle_text();
                skill_state = 10;
                return 0;
            }
        }
        break;

    case 10: // done
        // return 0 to end the skill
        return 0;
    }

    // return 1 to keep the skill going
    return 1;
}

// Template for generating wrapper functions to invoke
// realm skills with different arguments.
template<Terrain T, Weather U, bool V = false>
int __cdecl realm_dispatch(int player, int effect_chance)
{
    return skill_realm(player, effect_chance, T, U, V);
}

int __cdecl skill_delirium(int player, int /*effect_chance*/)
{
    auto& skill_state = get_skill_state();
    switch(skill_state)
    {
    case 0:
        if(!skill_succeeded())
        {
            skill_state = 3;
            return 0;
        }
        reset_stat_mod();
        skill_state = 1;
        return 1;

    case 1:
        if(apply_stat_mod(STAT_SPATK, -1, player) == 0)
        {
            skill_state = 2;
            reset_stat_mod();
        }
        return 1;

    case 2:
        if(apply_stat_mod(STAT_SPDEF, -1, player) == 0)
        {
            skill_state = 3;
            return 0;
        }
        return 1;

    default:
        return 0;
        break;
    }
}

// calamity/supernova
int __cdecl skill_custom1(int player, int /*effect_chance*/)
{
    auto& skill_state = get_skill_state();
    static Status status1 = STATUS_NONE, status2 = STATUS_NONE;
    switch(skill_state)
    {
    case 0:
    {
        if(!skill_succeeded())
        {
            skill_state = 2;
            return 0;
        }
        reset_status_anim();
        skill_state = 1;
        auto statuses = g_calamity_statuses[get_rng(g_calamity_statuses.size() - 1)];
        status1 = statuses.first;
        status2 = statuses.second;
        return 1;
    }

    case 1:
        if(apply_status_effect(!player, status1, status2) == 0)
        {
            skill_state = 2;
            return 0;
        }
        return 1;

    default:
        return 0;
        break;
    }
}

// boundary rend
int __cdecl skill_custom2(int player, int /*effect_chance*/)
{
    auto& skillstate = get_skill_state();
    if((skillstate == 0) && skill_succeeded())
    {
        auto otherstate = get_battle_state(!player);
        if(otherstate->active_puppet->hp > 0)
            otherstate->active_puppet->hp = 1;
        skillstate = 1;
    }

    return 0;
}

// Bind skill functions to effect ids.
void init_custom_skills()
{
    // register modified realm skills for stargazer/timegazer
    g_godstone_hack = IniFile::global.get_bool("general", "realm_godstone_hack");
    g_stargazer_hack = IniFile::global.get_bool("general", "realm_stargazer_hack");
    g_timegazer_hack = IniFile::global.get_bool("general", "realm_timegazer_hack");
    if(g_godstone_hack || g_stargazer_hack || g_timegazer_hack)
    {
        register_custom_skill(247, &realm_dispatch<TERRAIN_SUZAKU, WEATHER_HEAVYFOG>);
        register_custom_skill(248, &realm_dispatch<TERRAIN_BYAKKO, WEATHER_AURORA>);
        register_custom_skill(249, &realm_dispatch<TERRAIN_SEIRYU, WEATHER_CALM>);
        register_custom_skill(250, &realm_dispatch<TERRAIN_KOHRYU, WEATHER_DUSTSTORM>);
        register_custom_skill(251, &realm_dispatch<TERRAIN_GENBU, WEATHER_SUNSHOWER>);
    }

    g_season_base = IniFile::global.get_uint("skills", "effect_id_season_base");
    g_season_duration = IniFile::global.get_uint("skills", "season_duration", g_season_duration);
    if(g_season_base != ID_NONE)
    {
        init_new_skill(g_season_base);
        register_custom_skill(g_season_base, &realm_dispatch<TERRAIN_GENBU, WEATHER_DUSTSTORM, true>);
        register_custom_skill_activator(g_season_base, &season_activator);
        init_new_skill(g_season_base + 1);
        register_custom_skill(g_season_base + 1, &realm_dispatch<TERRAIN_SEIRYU, WEATHER_SUNSHOWER, true>);
        register_custom_skill_activator(g_season_base + 1, &season_activator);
        init_new_skill(g_season_base + 2);
        register_custom_skill(g_season_base + 2, &realm_dispatch<TERRAIN_SUZAKU, WEATHER_AURORA, true>);
        register_custom_skill_activator(g_season_base + 2, &season_activator);
        init_new_skill(g_season_base + 3);
        register_custom_skill(g_season_base + 3, &realm_dispatch<TERRAIN_BYAKKO, WEATHER_HEAVYFOG, true>);
        register_custom_skill_activator(g_season_base + 3, &season_activator);
        init_new_skill(g_season_base + 4);
        register_custom_skill(g_season_base + 4, &realm_dispatch<TERRAIN_KOHRYU, WEATHER_CALM, true>);
        register_custom_skill_activator(g_season_base + 4, &season_activator);
    }

    g_id_delirium = IniFile::global.get_uint("skills", "effect_id_delirium");
    if(g_id_delirium != ID_NONE)
    {
        init_new_skill(g_id_delirium);
        register_custom_skill(g_id_delirium, &skill_delirium);
    }

    // pre-build table of status combos for supernova/calamity
    g_id_custom1 = IniFile::global.get_uint("skills", "effect_id_custom1");
    if(g_id_custom1 != ID_NONE)
    {
        init_new_skill(g_id_custom1);
        register_custom_skill(g_id_custom1, &skill_custom1);

        const Status statuses1[] =
        {
            STATUS_POIS,
            STATUS_BURN,
            STATUS_BLIND,
            STATUS_FEAR,
            STATUS_WEAK,
            STATUS_PARA,
            STATUS_HVYPOIS,
            STATUS_HVYBURN,
            STATUS_STOP,
            STATUS_SHOCK,
            STATUS_HVYWEAK,
            STATUS_CONFUSE
        };

        const Status statuses2[] =
        {
            STATUS_POIS,
            STATUS_BURN,
            STATUS_BLIND,
            STATUS_FEAR,
            STATUS_WEAK,
            STATUS_PARA,
            STATUS_CONFUSE
        };

        // pretty sure this can be done better
        // but i'm too tired to brain it out
        for(auto i : statuses1)
        {
            if((i > STATUS_PARA) && (i < STATUS_CONFUSE))
            {
                g_calamity_statuses.push_back({ i, STATUS_NONE });
                continue;
            }
            for(auto j : statuses2)
            {
                if(i == j)
                    continue;
                StatusPair reverse = { j, i };
                if(std::find(g_calamity_statuses.begin(), g_calamity_statuses.end(), reverse) != g_calamity_statuses.end())
                    continue;
                g_calamity_statuses.push_back({ i, j });
            }
        }
    }

    g_id_custom2 = IniFile::global.get_uint("skills", "effect_id_custom2");
    if(g_id_custom2 != ID_NONE)
    {
        init_new_skill(g_id_custom2);
        register_custom_skill(g_id_custom2, &skill_custom2);
    }
}

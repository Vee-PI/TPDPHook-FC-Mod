#include "custom_abilities.h"
#include "tpdp_data.h"
#include "misc_hacks.h"
#include "../hook.h"
#include "../ini_parser.h"

static bool g_ability_init = true;
static bool g_terrain_setters = false;
static auto g_id_idola = ID_NONE;
static auto g_id_fascination = ID_NONE;
static auto g_id_ceremony = ID_NONE;

// these are switch-in abilities (e.g. auto setters, aggressive)
// other abilities probably implemented in misc_hacks.cpp

// implements auto-setter abilities for
// arbitrary combinations of weather/terrain
static bool do_terrain_setter(int& ability_state, int player, Terrain terrain, Weather weather = WEATHER_NONE, int duration = 5)
{
    static auto do_weather = false;
    static auto do_terrain = false;

    auto state = get_terrain_state();

    switch(ability_state)
    {
    case 0:
        do_weather = (weather != WEATHER_NONE) && (weather != state->weather_type);
        do_terrain = (terrain != TERRAIN_NONE) && (terrain != state->terrain_type);
        if(do_weather || do_terrain)
        {
            reset_ability_activation(player);
            ability_state = 1;
            return true;
        }
        ability_state = 10;
        return false;

    case 1:
        if(show_ability_activation(player) == 0)
        {
            if(do_weather)
            {
                state->weather_type = (uint8_t)weather;
                state->weather_duration = duration;

                reset_weather_msg();
                ability_state = 2;
                return true;
            }
            ability_state = 3;
            return true;
        }
        return true;

    case 2:
        if(do_weather_msg() == 0)
            ability_state = 3;
        return true;

    case 3:
        if(do_terrain)
        {
            state->terrain_type = (uint8_t)terrain;
            state->terrain_duration = duration;

            reset_terrain_msg();
            clear_battle_text();
            ability_state = 4;
            return true;
        }
        ability_state = 10;
        return false;

    case 4:
        if(do_terrain_msg() == 0)
        {
            ability_state = 10;
            return false;
        }
        return true;

    default:
        return false;
    }
}

// Idola Diabolus
static bool do_idola(int& ability_state, int player)
{
    static StatIndex index = STAT_HP;

    switch(ability_state)
    {
    case 0:
    {
        auto otherstate = get_battle_state(!player);
        auto otherpuppet = decrypt_puppet(otherstate->active_puppet);
        auto foatk = calculate_stat(STAT_FOATK, &otherpuppet);
        auto spatk = calculate_stat(STAT_SPATK, &otherpuppet);
        index = (spatk > foatk) ? STAT_SPDEF : STAT_FODEF;
        reset_ability_activation(player);
        ability_state = 1;
        return true;
    }

    case 1:
        if(show_ability_activation(player) == 0)
        {
            reset_stat_mod();
            ability_state = 2;
        }
        return true;

    case 2:
        if(apply_stat_mod(index, 1, player) == 0)
        {
            ability_state = 10;
            return false;
        }
        return true;

    default:
        return false;
    }
}

static bool do_ability(int player)
{
    static auto _state = 0;
    static bool skip = false;
    bool result = false;
    auto ability = (unsigned int)get_battle_state(player)->active_ability;
    if(g_ability_init)
    {
        _state = get_ability_state();
        g_ability_init = false;
        skip = false;
    }

    if(!skip)
    {
        if(g_terrain_setters)
        {
            switch(ability)
            {
            case 450:
                result = do_terrain_setter(_state, player, TERRAIN_SEIRYU);
                break;
            case 451:
                result = do_terrain_setter(_state, player, TERRAIN_SUZAKU);
                break;
            case 452:
                result = do_terrain_setter(_state, player, TERRAIN_BYAKKO);
                break;
            case 453:
                result = do_terrain_setter(_state, player, TERRAIN_GENBU);
                break;
            case 454:
                result = do_terrain_setter(_state, player, TERRAIN_KOHRYU);
                break;
            default:
                break;
            }
        }

        if(ability == g_id_idola)
            result = do_idola(_state, player);
        if(ability == g_id_fascination)
            result = do_terrain_setter(_state, player, TERRAIN_SEIRYU, WEATHER_HEAVYFOG, 0x7ffffff5);
        if((ability == g_id_ceremony) && (_state == 0))
        {
            do_terrain_hook();
            result = false;
        }
    }

    if(result == false)
        skip = true;
    return result;
}

// low level hack for intercepting execution of abilities
static int __fastcall ability_dispatch(int player)
{
    int ret;
    if(!do_ability(player))
    {
        ret = RVA(0x7bc0).ptr<int(__fastcall*)(int)>()(player);
        if(ret == 0)
            g_ability_init = true;
    }
    else
        return 1;

    return ret;
}

// initialization
void init_custom_abilities()
{
    scan_and_replace_call(RVA(0x7bc0), &ability_dispatch);

    g_terrain_setters = IniFile::global.get_bool("general", "enable_terrain_setters");
    g_id_idola = IniFile::global.get_uint("abilities", "id_idola");
    g_id_fascination = IniFile::global.get_uint("abilities", "id_custom_abl1");
    g_id_ceremony = IniFile::global.get_uint("abilities", "id_field_abl");
}

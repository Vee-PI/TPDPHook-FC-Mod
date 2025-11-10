#include "misc_hacks.h"
#include "tpdp_data.h"
#include "../ini_parser.h"
#include "../hook.h"
#include "../log.h"
#include "../network.h"
#include "hook_tpdp.h"
#include "custom_skills.h"
#include "archive.h"
#include "savefile.h"
#include "gamedata.h"
#include "graphics.h"
#include "../textconvert.h"
#include <cassert>
#include <algorithm>
#include <set>
#include <memory>
#include <mutex>
#include <regex>
#include <fstream>

static const char* g_element_names_en[] = {
    "None",
    "Void",
    "Fire",
    "Water",
    "Nature",
    "Earth",
    "Steel",
    "Wind",
    "Electric",
    "Light",
    "Dark",
    "Nether",
    "Poison",
    "Fighting",
    "Illusion",
    "Sound",
    "Dream",
    "Warped",
};
static const char* g_element_names_jp[] = {
    "None",
    "無",
    "炎",
    "水",
    "自然",
    "大地",
    "鋼鉄",
    "風",
    "雷",
    "光",
    "闇",
    "冥",
    "毒",
    "闘",
    "幻",
    "音",
    "夢",
    "歪",
};


/*      HERE LIES SPICY JANK
 * This file contains unholy hackery
 * unfit for the eyes of the innocent.
 * Continue at your own risk.
 */

// pointers
static void *g_mine_return_addr = nullptr;
static void *g_stealth_return_addr = nullptr;
static void *g_bind_return_addr = nullptr;
static void *g_pois_return_addr = nullptr;
static void *g_sprintf_addr = nullptr;
static void *g_background_return_addr = nullptr;
static void *g_background_bgm_return_addr = nullptr;
static void *g_box_icon_return_addr = nullptr;
//static void *g_music_return_addr = nullptr;
//static void *g_music_hackreturn_addr = nullptr;

// item IDs
static auto g_id_boots = ID_NONE;
static auto g_id_giant_bit = ID_NONE;
static auto g_id_wyrmprint = ID_NONE;
static auto g_id_drum = ID_NONE;
static auto g_id_genbu_seed = ID_NONE;
static auto g_id_kohryu_seed = ID_NONE;
static auto g_id_seiryu_seed = ID_NONE;
static auto g_id_byakko_seed = ID_NONE;
static auto g_id_suzaku_seed = ID_NONE;
static auto g_id_resheal = ID_NONE;
static auto g_id_feast = ID_NONE;
static auto g_id_prismatic = ID_NONE; //ID 665
static auto g_id_mending = ID_NONE; //ID 666 mod=0.5, All charms including this and below do not function (Hold 1 Items)
//static auto g_id_suncharm = ID_NONE; //ID 667 | Aurora, SpDef
//static auto g_id_mooncharm = ID_NONE; //ID 668 | Fog, FoDef
//static auto g_id_landcharm = ID_NONE; //ID 669 | Dust Storm, FoAtk
//static auto g_id_skycharm = ID_NONE; //ID 670 | Calm, SpAtk
//static auto g_id_spectrumcharm = ID_NONE; //ID 671 | Sunshower, Speed


// item mods
static auto g_mod_wyrmprint_high = 1.4;
static auto g_mod_wyrmprint_low = 1.2;
static auto g_mod_drum = 1.3;
static auto g_mod_genbu_seed = 0.5;
static auto g_mod_kohryu_seed = 1.5;
static auto g_mod_seiryu_seed = 4.0 / 3.0;
static auto g_mod_suzaku_seed = 1.0 / 8.0;
static auto g_mod_resheal = 1.0 / 8.0;
static auto g_power_giant_bit = 120u;
static auto g_mod_feast = 1.0 / 3.0;
static auto g_mod_prismatic = 1.15;
static auto g_mod_mending = 0.5;

// ability IDs
static auto g_id_stacking = ID_NONE;
static auto g_id_light_wings = ID_NONE;
static auto g_id_water_wings = ID_NONE;
static auto g_id_astronomy = ID_NONE;
static auto g_id_empowered = ID_NONE;
static auto g_id_drain_abl = ID_NONE;
static auto g_id_calm_traveler = ID_NONE;
unsigned int g_id_form_change = ID_NONE;
unsigned int g_id_form_target = ID_NONE;
unsigned int g_id_form_target_style = ID_NONE;
static auto g_id_form_base = ID_NONE;
static auto g_id_form_base_style = ID_NONE;
static auto g_id_morale = ID_NONE;
static auto g_id_curse = ID_NONE;
static auto g_id_merciless = ID_NONE;
static auto g_id_choice_focus = ID_NONE;
static auto g_id_choice_spread = ID_NONE;
static auto g_id_imposter = ID_NONE;
static auto g_id_anti_status = ID_NONE;
static auto g_id_magic = ID_NONE;
static auto g_id_warmup = ID_NONE;
static auto g_id_challenge = ID_NONE;
static auto g_id_sand_castle = ID_NONE;
unsigned int g_id_possess = ID_NONE;
unsigned int g_id_possess_target = ID_NONE;
unsigned int g_id_possess_target_style = ID_NONE;
static auto g_id_possess_base = ID_NONE;
static auto g_id_possess_base_style = ID_NONE;
static auto g_id_gradate = ID_NONE;


// ability mods
static auto g_mod_class_abl = 0.1;
static auto g_mod_drain_heal = 1.0 / 8.0;
static auto g_mod_drain_def = 0.8;
static auto g_mod_calm_traveler = 2.0;
static auto g_mod_merciless = 1.5;
static auto g_mod_choice = 1.5;
static auto g_mod_magic = 1.1;
static auto g_mod_challenge = 1.2;
static auto g_mod_sand_castle = 2.0;

// skills
static auto g_id_blitzkrieg = ID_NONE;
static double g_mod_blitzkrieg = 2.0;
static auto g_id_future_skill = ID_NONE;
static bool g_patch_miracle = false;
static bool g_macroburst_acc_boost = false;
static auto g_id_arrow = ID_NONE;
static double g_mod_arrow = 2.0;
static auto g_id_eclipse = ID_NONE;
static double g_mod_eclipse = 2.0;

WishState g_wish_state[2];

bool g_form_changes[2][6]{};

std::string get_element_string(unsigned int type)
{
    bool english = tpdp_eng_translation();
    return english ? g_element_names_en[type] : g_element_names_jp[type];
}

static std::unique_ptr<uint32_t[]> g_music_handle_buf;
static std::unique_ptr<uint8_t[]> g_music_loop_buf;

static std::unique_ptr<uint8_t[]> g_player_level_cap = std::make_unique<uint8_t[]>(0x198 * 5);
static std::unique_ptr<uint8_t[]> g_ai_level_cap = std::make_unique<uint8_t[]>(0x198 * 5);

static constexpr size_t DOLLICON_WIDTH = 0x10;
static constexpr size_t DOLLICON_HEIGHT = 0x20;
static constexpr size_t DOLLICON_COUNT = 0x200;
static constexpr size_t HAZARD_ICON_WIDTH = 3;
static constexpr size_t HAZARD_ICON_HEIGHT = 9;
static constexpr size_t HAZARD_ICON_COUNT = HAZARD_ICON_WIDTH * HAZARD_ICON_HEIGHT;
static std::unique_ptr<uint32_t[]> g_dollicon_handle_buf = std::make_unique<uint32_t[]>(DOLLICON_COUNT);
static std::unique_ptr<uint32_t[]> g_mark_handle_buf = std::make_unique<uint32_t[]>(6);                     // puppet box mark icons
static std::unique_ptr<uint32_t[]> g_number_handle_buf = std::make_unique<uint32_t[]>(11);                  // weather timer graphics
static std::unique_ptr<uint32_t[]> g_small_number_handle_buf = std::make_unique<uint32_t[]>(10);
static std::unique_ptr<uint32_t[]> g_hazard_handle_buf = std::make_unique<uint32_t[]>(HAZARD_ICON_COUNT);   // hazard overlay icons
static std::unique_ptr<uint32_t[]> g_type_handle_buf = std::make_unique<uint32_t[]>(17);                    // type overlay icons

std::once_flag g_music_init;

std::string g_website_url = "";

// overlay jank
static uint32_t g_weather_counter = 0;
static uint32_t g_terrain_counter = 0;
static uint32_t g_last_weather_duration = 0;
static uint32_t g_last_terrain_duration = 0;
static uint32_t g_field_barrier_turns[] = { 0, 0 };
static uint32_t g_field_protect_turns[] = { 0, 0 };
static bool g_debug_overlay = false;

// NOTE: we technically should be doing some XSAVE and FXSAVEs in here
// but the game does not use SSE and our code does not use x87 (except floating point return values)
// so we can avoid clobbering eachother if we're careful

/*
struct alignas(16) FXSaveData
{
    uint8_t buf[512];
};
*/

/* // replaced by ../network.cpp
uint32_t ip_str_to_be32(const std::string& str)
{
    std::smatch m;
    std::regex ip_regex(R"(^(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3}))");
    if(std::regex_search(str, m, ip_regex))
    {
        auto p1 = (uint32_t)std::stoul(m[1]) & 0xFF;
        auto p2 = (uint32_t)std::stoul(m[2]) & 0xFF;
        auto p3 = (uint32_t)std::stoul(m[3]) & 0xFF;
        auto p4 = (uint32_t)std::stoul(m[4]) & 0xFF;

        return (p1 << 0) | (p2 << 8) | (p3 << 16) | (p4 << 24);
    }

    return 0;
}
*/

// Secret Ceremony
// called whenever weather/terrain is changed
void do_terrain_hook()
{
    static const ElementType weathertypes[] = { ELEMENT_VOID, ELEMENT_WIND, ELEMENT_LIGHT, ELEMENT_DARK, ELEMENT_EARTH, ELEMENT_WARPED };
    static const ElementType terraintypes[] = { ELEMENT_VOID, ELEMENT_NATURE, ELEMENT_FIRE, ELEMENT_STEEL, ELEMENT_WATER, ELEMENT_EARTH };
    auto id_ceremony = IniFile::global.get_uint("abilities", "id_field_abl");
    for(auto i = 0; i < 2; ++i)
    {
        auto state = get_battle_state(i);
        if((uint)state->active_ability == id_ceremony)
        {
            auto tstate = get_terrain_state();
            std::vector<ElementType> newtypes;
            if((tstate->terrain_type != TERRAIN_NONE) && (tstate->terrain_type < 6u))
                newtypes.push_back(terraintypes[tstate->terrain_type]);
            if((tstate->weather_type != WEATHER_NONE) && (tstate->weather_type < 6u))
                newtypes.push_back(weathertypes[tstate->weather_type]);
            if((newtypes.size() > 1) && (newtypes[0] == newtypes[1]))
                newtypes[1] = ELEMENT_NONE;

            if(newtypes.empty())
            {
                auto puppet = decrypt_puppet(state->active_puppet);
                auto data = get_puppet_data()[puppet.puppet_id];
                newtypes.push_back((ElementType)data.styles[puppet.style_index].element1);
                newtypes.push_back((ElementType)data.styles[puppet.style_index].element2);
            }
            else if(newtypes.size() < 2)
                newtypes.push_back(ELEMENT_NONE);

            state->active_type1 = (byte)newtypes[0];
            state->active_type2 = (byte)newtypes[1];
        }
    }
}

int do_possess()
{
    for (auto i = 0; i < 2; ++i)
    {
        auto state = get_battle_state(i);
        if ((uint)state->active_ability == g_id_possess) //ancient possession//
        {
            auto tstate = get_terrain_state();
            if (tstate->weather_type != WEATHER_DUSTSTORM)
            {
                return do_form_change(i, g_id_possess_target, g_id_possess_target_style, false, true);
            }
            else if (tstate->weather_type == WEATHER_DUSTSTORM)
            {
                return do_form_change(i, g_id_possess_base, g_id_possess_base_style, false, true);
            }
        }
    }
    return 0;
}

// called when the battle state is reset
// before a new battle begins
void do_battlestate_reset()
{
    // call the original function first
    auto func = RVA(0x1bf70).ptr<VoidCall>();
    func();

    // reset any custom state here
    for(auto i = 0; i < 2; ++i)
        for(auto& j : g_form_changes[i])
            j = false;

    // future sight
    for(auto& i : g_future_turns)
        i = 0;

    auto bdata = get_battle_data();
    bool loaded_yuuma = false, loaded_posses = false;
    for(auto i = 0; i < 2; ++i) // load form change sprite if user present
    {
        for(auto& j : bdata[i].puppets)
        {
            auto puppet = decrypt_puppet(&j);
            if((puppet.puppet_id == g_id_form_base) && (puppet.style_index == g_id_form_base_style) && (loaded_yuuma == false))
            {
                load_puppet_sprite(g_id_form_target);
                loaded_yuuma = true;
            }
            else if ((puppet.puppet_id == g_id_possess_base) && (puppet.style_index == g_id_possess_base_style) && (loaded_posses == false))
            {
                load_puppet_sprite(g_id_possess_target);
                loaded_posses = true;
            }
        }
    }

    for (auto& wish : g_wish_state)
        wish = {};

    g_weather_counter = 0;
    g_terrain_counter = 0;
    g_last_weather_duration = 0;
    g_last_terrain_duration = 0;
    for(auto& i : g_field_barrier_turns)
        i = 0;
    for(auto& i : g_field_protect_turns)
        i = 0;
    return;
}

// override skill damage calculation
// this overrides the *final result* of vanilla damage calculation
uint do_dmg_calc(BattleState *state, BattleState *otherstate, int player, [[maybe_unused]] uint acc,
                 [[maybe_unused]] bool crit, BattleData *bdata, BattleData *otherbdata, [[maybe_unused]] ushort skill_id, uint orig_dmg)
{
    if(state == nullptr)
        state = get_battle_state(player);
    if(otherstate == nullptr)
        otherstate = get_battle_state(!player);
    if(bdata == nullptr)
        bdata = &get_battle_data()[player];
    if(otherbdata == nullptr)
        otherbdata = &get_battle_data()[!player];
    auto dmg = (double)orig_dmg;

    //auto sid = (uint)state->active_skill_id;
    //auto sid = (uint)skill_id;
    //const auto& skilldata = get_skill_data()[(sid < 1024) ? sid : 0u];
    //auto eid = skilldata.effect_id;
    auto puppet = decrypt_puppet(state->active_puppet);
    auto otherpuppet = decrypt_puppet(otherstate->active_puppet);

    if((puppet.held_item == g_id_wyrmprint) && can_use_items(state)) // Radiant Hairpin (AKA wyrmprint)
    {
        auto max_hp = std::max(calculate_stat(STAT_HP, &puppet), 1u);
        if(puppet.hp == max_hp)
            dmg *= g_mod_wyrmprint_high;
        else if(puppet.hp > 0)
        {
            auto ratio = double(puppet.hp) / double(max_hp);
            dmg += ((dmg * (g_mod_wyrmprint_low - 1.0)) * ratio);
        }
    }
    else if((puppet.held_item == g_id_drum) && can_use_items(state)) // Tsuzumi Drum
    {
        auto type1 = state->active_type1;
        auto type2 = state->active_type2;
        bool boost = true;
        auto data = get_skill_data();
        for(auto i : puppet.skills)
        {
            if(data[i].type == SKILL_TYPE_STATUS)
                continue;
            auto type = data[i].element;
            if((type == type1) || (type == type2))
            {
                boost = false;
                break;
            }
        }
        if(boost)
            dmg *= g_mod_drum;
    }
    else if ((puppet.held_item == g_id_prismatic) && can_use_items(state)) // Prismatic Hairpin
    {
        auto type1 = state->active_type1;
        auto type2 = state->active_type2;
        auto data = get_skill_data();
        auto curskill = (skill_id != 0) ? skill_id : state->active_skill_id;
        {
            if ((data[curskill].element == type1) || (data[curskill].element == type2))
                dmg *= g_mod_prismatic;
        }
    }
    else if((puppet.held_item == g_id_seiryu_seed) && (get_terrain_state()->terrain_type == TERRAIN_SEIRYU) && can_use_items(state))
    {
        dmg *= g_mod_seiryu_seed; // Yggdrasil Seed
    }

    if((otherpuppet.held_item == g_id_seiryu_seed) && (get_terrain_state()->terrain_type == TERRAIN_SEIRYU) && can_use_items(otherstate))
    {
        dmg *= g_mod_seiryu_seed; // also Yggdrasil Seed
    }

    if (((uint)state->active_ability == g_id_merciless) && (otherstate->active_ability != 335) && (get_terrain_state()->terrain_type != TERRAIN_KOHRYU))
    {
        for(auto status : otherpuppet.status_effects)
        {
            if(status != STATUS_NONE)
            {
                dmg *= g_mod_merciless; //merciless atk boost
                break;
            }
        }
    }
    else if (((uint)state->active_ability == g_id_astronomy) && (otherstate->active_ability != 335) && (get_terrain_state()->terrain_type != TERRAIN_KOHRYU))
    {
        auto data = get_skill_data();
        if (data[skill_id].classification == SKILL_CLASS_BU)
            dmg *= g_mod_class_abl; //astronomy atk boost
    }

    else if (((uint)state->active_ability == g_id_empowered) && (otherstate->active_ability != 335) && (get_terrain_state()->terrain_type != TERRAIN_KOHRYU))
    {
        auto data = get_skill_data();
        if (data[skill_id].classification == SKILL_CLASS_EN)
            dmg *= g_mod_class_abl; //empowered atk boost
    }

    else if (((uint)state->active_ability == g_id_magic) && (otherstate->active_ability != 335) && (get_terrain_state()->terrain_type != TERRAIN_KOHRYU))
    {
        auto data = get_skill_data();
        if (data[skill_id].power >= 120)
            dmg *= g_mod_magic; //magic boost
    }

   /* auto data = get_skill_data();
    if (g_patch_miracle && (data->effect_id == 120))
    {
        auto otherstate.
        auto boost = 0;
        for (auto stat : otherstate->stat_modifiers)
            if (stat > 0)
        damage *= num_stat_boosts + 1;
    } 
    miracle reprisal, come back when you're stronger */

    if((int)dmg < 0)
    {
        dmg = 1;
        log_warn(L"Clamping damage underflow to 1");
    }
    if(dmg > 999)
    {
        dmg = 999;
        log_warn(L"Clamping damage overflow to 999");
    }

    return (uint)dmg;
}

// calling convention fixup stub
__declspec(naked)
uint __fastcall _do_dmg_calc(BattleState *state, BattleState *otherstate, int player, uint acc,
                             bool crit, BattleData *bdata, ushort skill_id)
{
    BattleData *otherbdata;
    __asm
    {
        push ebp
        mov ebp, esp
        sub esp, __LOCAL_SIZE
        mov state, ecx
        mov otherstate, edx
        mov otherbdata, eax
    }

    uint orig_result, result;
    orig_result = calculate_damage(state, otherstate, player, acc, crit, bdata, otherbdata, skill_id);
    result = do_dmg_calc(state, otherstate, player, acc, crit, bdata, otherbdata, skill_id, orig_result);

    __asm
    {
        mov eax, result
        mov esp, ebp
        pop ebp
        ret // not actually fastcall
    }
}

// override in-battle stat calculation
int __stdcall do_battle_stats(BattleState *state, byte stat_index, bool crit, bool param_4, bool param_5)
{
    auto func = RVA(0xa850).ptr<int(__stdcall*)(BattleState*,byte,bool,bool,bool)>(); // original stat calculation function
    auto result = (double)func(state, stat_index, crit, param_4, param_5); // use vanilla calculation as a base
    auto puppet = decrypt_puppet(state->active_puppet);
    auto terrain_state = get_terrain_state();

    switch(stat_index)
    {
    case STAT_FOATK:
        if((uint)state->active_ability == g_id_choice_focus)
            result *= g_mod_choice;
        break;

    case STAT_SPATK:
        if((uint)state->active_ability == g_id_choice_spread)
            result *= g_mod_choice;
        break;

    case STAT_FODEF:
        if (((uint)state->active_ability == g_id_sand_castle) && (terrain_state->weather_type == WEATHER_DUSTSTORM))
            result *= g_mod_sand_castle; // Silent Running

    case STAT_SPDEF:
        if((uint)state->active_ability == g_id_drain_abl) // Cursed Being def drop
            result *= g_mod_drain_def;
        break;

    case STAT_SPEED:
        if((puppet.held_item == g_id_kohryu_seed) && (terrain_state->terrain_type == TERRAIN_KOHRYU) && (state->active_ability != 245) && (state->active_ability != 390))
            result *= g_mod_kohryu_seed; // Izanagi Object

        if(((uint)state->active_ability == g_id_calm_traveler) && (terrain_state->weather_type == WEATHER_CALM))
            result *= g_mod_calm_traveler; // Silent Running
        break;

    default:
        break;
    }

    if(result <= 1.0)
        return 1;
    return (int)result;
}
    /*else if (((uint)state->active_ability == g_id_astronomy) && (otherstate->active_ability != 335) && (terrain_state->terrain_type != TERRAIN_KOHRYU) && (data->classification == SKILL_CLASS_BU))
    {
        auto power = (double)data->power;
        data->power = (byte)std::clamp(power + (power * g_mod_class_abl), 0.0, 255.0); // Astronomy
    }
    else if(((uint)state->active_ability == g_id_empowered) && (otherstate->active_ability != 335) && (terrain_state->terrain_type != TERRAIN_KOHRYU) && (data->classification == SKILL_CLASS_EN))
    {
        auto power = (double)data->power;
        data->power = (byte)std::clamp(power + (power * g_mod_class_abl), 0.0, 255.0); // Empowered
    }
    else if (((uint)state->active_ability == g_id_magic) && (otherstate->active_ability != 335) && (terrain_state->terrain_type != TERRAIN_KOHRYU) && (data->power >= 120))
    {
        auto power = (double)data->power;
        data->power = (byte)std::clamp(power + (power * g_mod_magic), 0.0, 255.0);
    }*/

// override type/power/prio/etc changes
SkillData *__fastcall do_adjusted_skill_data(int player, ushort skill_id)
{
    auto data = get_adjusted_skill_data(player, skill_id);

    auto state = get_battle_state(player);
    auto otherstate = get_battle_state(!player);
    auto terrain_state = get_terrain_state();
    auto curskill = (skill_id != 0) ? skill_id : state->active_skill_id;
    const auto& basedata = get_skill_data()[curskill];
    if ((data->effect_id == g_id_blitzkrieg) && ((state->turn_order == 0) || (otherstate->num_turns == 0)))
        data->power = (byte)std::clamp((double)data->power * g_mod_blitzkrieg, 0.0, 255.0);

    else if ((data->effect_id == g_id_arrow) && ((terrain_state->weather_type == WEATHER_HEAVYFOG) && (terrain_state->weather_duration > 0) && ((uint)otherstate->active_ability != 117)) && ((uint)state->active_ability != 117))
        data->power = (byte)std::clamp((double)data->power * g_mod_arrow, 0.0, 255.0);

    else if ((data->effect_id == g_id_eclipse) && ((terrain_state->weather_type == WEATHER_AURORA) && (terrain_state->weather_duration > 0) && ((uint)otherstate->active_ability != 117)) && ((uint)state->active_ability != 117))
        data->power = (byte)std::clamp((double)data->power * g_mod_eclipse, 0.0, 255.0);

    if(((uint)state->active_ability == g_id_light_wings) && (basedata.element == ELEMENT_LIGHT))
    {
        if((data->priority < 6) && ((uint)otherstate->active_ability != 341)) // Grand Opening
            data->priority += 1;
    }
    else if (((uint)state->active_ability == g_id_water_wings) && (basedata.element == ELEMENT_WATER))
    {
        if ((data->priority < 6) && ((uint)otherstate->active_ability != 341)) // Water Opening
            data->priority += 1;
    }


    // macroburst
    if(g_macroburst_acc_boost && (curskill == 359))
    {
        if((terrain_state->weather_type == WEATHER_CALM) && (terrain_state->weather_duration > 0))
            data->accuracy = 100;
    }

    // miracle reprisal
    if(g_patch_miracle && (data->effect_id == 120))
    {
        auto boost = 0;
        for(auto stat : otherstate->stat_modifiers)
            if(stat > 0)
                boost += ((int)stat * 50);
        data->power = (byte)std::min((int)data->power + boost, 255);
    }

    // Pure Sand
    if(has_held_item(state, g_id_genbu_seed) && (terrain_state->terrain_type == TERRAIN_GENBU) && can_use_items(state))
    {
        data->accuracy = (byte)std::clamp((double)data->accuracy * g_mod_genbu_seed, 0.0, 100.0); // values over 100 ok???
    }

    return data;
}

// Spirit Torch
static bool do_suzaku_seed(int player)
{
    auto otherstate = get_battle_state(!player);
    auto state = get_battle_state(player);
    static int _state = 0;
    static int _frames = 0;

    switch(_state)
    {
    case 0:
    {
        auto otherpuppet = decrypt_puppet(otherstate->active_puppet);
        auto puppet = decrypt_puppet(state->active_puppet);
        auto max_hp = calculate_stat(STAT_HP, &otherpuppet);
        auto dmg = (int)((double)max_hp * g_mod_suzaku_seed);
        if((puppet.hp > 0) && (otherpuppet.hp > 0) && (otherstate->active_ability != 297))
        {
            otherstate->active_puppet->hp -= std::min(otherpuppet.hp, (ushort)std::max(dmg, 1));
            _state = 1;
            clear_battle_text();
            return true;
        }
        return false;
    }

    case 1:
    {
        auto name = std::string(otherstate->active_nickname);
        bool english = tpdp_eng_translation();
        if(player == 0)
            name = (english ? "Enemy " : "相手の　") + name;
        if(set_battle_text(name + (english ? " took damage from the\\nSpirit Torch!" : "は　スピリットトーチで\\nダメージを　受けた！")) != 1)
        {
            if(++_frames > get_game_fps())
            {
                _frames = 0;
                _state = 0;
                return false;
            }
        }
        return true;
    }

    default:
        _state = 0;
        return false;
    }
}

// Cursed Being
static bool do_drain_abl(int player)
{
    auto state = get_battle_state(player);
    auto otherstate = get_battle_state(!player);
    auto puppet = decrypt_puppet(state->active_puppet);
    auto otherpuppet = decrypt_puppet(otherstate->active_puppet);
    static int _state = 0;
    static int _frames = 0;
    if(state->active_puppet == nullptr)
        return false;
    if(otherstate->active_puppet == nullptr)
        return false;

    switch(_state)
    {
    case 0:
        _frames = 0;
        if((state->num_turns != 0) && (otherstate->num_turns != 0) && (puppet.hp > 0) && (otherpuppet.hp > 0))
        {
            reset_ability_activation(player);
            _state = 1;
            return true;
        }
        return false;

    case 1:
        if(show_ability_activation(player) == 0)
        {
            _state = 2;
            auto func = RVA(0x1401e0).ptr<VoidCall>();
            func();
        }
        return true;

    case 2:
    {
        void *anim_dispatch = RVA(0x1402a0);
        int result;
        __asm
        {
            mov ecx, player
            mov eax, 0x3ff
            call anim_dispatch
            mov result, eax
        }
        if(result == 0)
            _state = 3;
        return true;
    }

    case 3:
    {
        auto tstate = get_terrain_state();
        auto max_hp = (double)calculate_stat(STAT_HP, &puppet);
        auto othermax_hp = (double)calculate_stat(STAT_HP, &otherpuppet);
        auto dmg = std::min(std::max(othermax_hp * g_mod_drain_heal, 1.0), (double)otherpuppet.hp);
        otherstate->active_puppet->hp -= (ushort)dmg;
        bool do_heal = true;
        for(auto i : puppet.status_effects)
            if((i == STATUS_WEAK) || (i == STATUS_HVYWEAK))
                do_heal = false;
        if(do_heal)
        {
            if(tstate->terrain_type == TERRAIN_SUZAKU)
                state->active_puppet->hp = (ushort)std::max((double)puppet.hp - std::max(dmg * 0.5, 1.0), 0.0);
            else
                state->active_puppet->hp = (ushort)std::min((double)puppet.hp + dmg, max_hp);
        }
        _state = 4;
        return true;
    }

    case 4:
        if(++_frames > get_game_fps())
        {
            _state = 0;
            _frames = 0;
            return false;
        }
        return true;

    default:
        _state = 0;
        _frames = 0;
        return false;
    }
}

//Ancient Possession heal//
static bool do_possess_heal(int player)
{
    auto state = get_battle_state(player);
    auto puppet = decrypt_puppet(state->active_puppet);
    static int _state = 0;
    if (state->active_puppet == nullptr)
        return false;

    switch (_state)
    {
    case 0:
        if (puppet.hp > 0)
        {
            reset_heal_anim(player);
            _state = 1;
            return true;
        }
        return false;

    case 1:
    {
        if (do_heal_anim() == 0)
            _state = 2;
        return true;
    }

    case 2:
    {
        auto tstate = get_terrain_state();
        auto max_hp = (double)calculate_stat(STAT_HP, &puppet);
        bool do_heal = true;
        auto heal = max_hp * (1.0 / 16.0);;
        for (auto i : puppet.status_effects)
            if ((i == STATUS_WEAK) || (i == STATUS_HVYWEAK))
                do_heal = false;
        if (do_heal)
        {
            if (tstate->terrain_type == TERRAIN_SUZAKU)
                state->active_puppet->hp = (ushort)std::max((double)puppet.hp - std::max(heal * 0.5, 1.0), 0.0);
            else
                state->active_puppet->hp = (ushort)std::min((double)puppet.hp + heal, max_hp);
        }
        _state = 0;
        return false;
    }

    default:
        _state = 0;
        return false;
    }
}

static bool verdant_heal(int player)
{
    auto state = get_battle_state(player);
    auto puppet = decrypt_puppet(state->active_puppet);
    static int _state = 0;
    static int _frames = 0;

    switch (_state)
    {
    case 0:
        _frames = 0;
        if (g_wish_state[player].turns == 0)
            return false;
        --g_wish_state[player].turns;

        if ((state->active_puppet != nullptr) && (puppet.hp > 0))
        {
            reset_heal_anim(player);
            auto tstate = get_terrain_state();
            auto max_hp = calculate_stat(STAT_HP, &puppet);

            bool do_heal = true;
            for (auto i : puppet.status_effects)
            {
                if ((i == STATUS_WEAK) || (i == STATUS_HVYWEAK))
                {
                    do_heal = false;
                    state->active_puppet->status_effects[0] = STATUS_NONE;
                    state->active_puppet->status_effects[1] = STATUS_NONE;
                    break;
                }
            }

            if (do_heal)
            {
                if (tstate->terrain_type == TERRAIN_SUZAKU)
                {
                    state->active_puppet->hp = (ushort)std::max((double)puppet.hp - std::max(g_wish_state[player].heal * 0.5, 1.0), 0.0);
                    state->active_puppet->status_effects[0] = STATUS_NONE;
                    state->active_puppet->status_effects[1] = STATUS_NONE;
                }
                else
                {
                    state->active_puppet->hp = (ushort)std::min(max_hp, puppet.hp + g_wish_state[player].heal);
                    state->active_puppet->status_effects[0] = STATUS_NONE;
                    state->active_puppet->status_effects[1] = STATUS_NONE;
                }
            }
            _state = 1;
            return true;
        }
        return false;

    case 1:
    {
        if (do_heal_anim() == 0)
        {
            _state = 2;
            return true;
        }
        return true;
    }

    case 2:
    {
        constexpr auto enheal = " feels healthier due\\nto the border!";
        constexpr auto jpheal = "は結界の効果で元気になった！";
        constexpr auto endrain = " has lost HP due to Suzaku!";
        constexpr auto jpdrain = "は 朱雀に\\n体力を 奪われた……";
        auto name = std::string(state->active_nickname);
        bool english = tpdp_eng_translation();
        bool drain = get_terrain_state()->terrain_type == TERRAIN_SUZAKU;
        auto enmsg = drain ? endrain : enheal;
        auto jpmsg = drain ? jpdrain : jpheal;
        if (player == 1)
            name = (english ? "Enemy " : "相手の　") + name;
        if (set_battle_text(name + (english ? enmsg : jpmsg)) != 1)
        {
            if (++_frames > get_game_fps())
            {
                _frames = 0;
                _state = 0;
                return 0;
            }
        }
        return 1;
    }

    default:
        _frames = 0;
        _state = 0;
        return false;
    }
}

//figy berry/mending charm testing
static bool mending_heal(int player)
{
    auto state = get_battle_state(player);
    auto otherstate = get_battle_state(!player);
    static int _state = 0;
    static int _frames = 0;

    if(state->active_puppet == nullptr)
    {
        _state = 0;
        _frames = 0;
        return false;
    }
    auto puppet = decrypt_puppet(state->active_puppet);

    switch(_state)
    {
    case 0:
        if((puppet.hp > 0) && (puppet.hp <= (calculate_stat(STAT_HP, &puppet) / 4u)))
        {
            for(auto i : puppet.status_effects)
            {
                if((i == STATUS_WEAK) || (i == STATUS_HVYWEAK))
                    return false;
            }

            reset_item_consume(player);
            _state = 1;
            return true;
        }
        return false;

    case 1:
        if(show_item_consume() == 0)
            _state = 2;
        return true;

    case 2:
    {
        auto tstate = get_terrain_state();
        auto max_hp = (double)calculate_stat(STAT_HP, &puppet);

        if(tstate->terrain_type == TERRAIN_SUZAKU)
        {
            state->active_puppet->hp = (ushort)std::max(puppet.hp - std::max(max_hp * g_mod_mending * 0.5, 1.0), 0.0);
        }
        else
        {
            state->active_puppet->hp = (ushort)std::min(puppet.hp + std::max(max_hp * g_mod_mending, 1.0), max_hp);
        }

        reset_heal_anim(player);
        _state = 3;
        return true;
    }

    case 3:
        if(do_heal_anim() == 0)
            _state = 4;
        return true;

    case 4:
    {
        constexpr auto enheal = " feels healthier due\\nto the border!";
        constexpr auto jpheal = "は結界の効果で元気になった！";
        constexpr auto endrain = " has lost HP due to Suzaku!";
        constexpr auto jpdrain = "は 朱雀に\\n体力を 奪われた……";
        auto name = std::string(state->active_nickname);
        bool english = tpdp_eng_translation();
        bool drain = get_terrain_state()->terrain_type == TERRAIN_SUZAKU;
        auto enmsg = drain ? endrain : enheal;
        auto jpmsg = drain ? jpdrain : jpheal;
        if(player == 1)
            name = (english ? "Enemy " : "相手の　") + name;
        if(set_battle_text(name + (english ? enmsg : jpmsg)) != 1)
        {
            if(++_frames > get_game_fps())
            {
                _frames = 0;
                _state = 0;
                *state->consumed_item_ptr = puppet.held_item;
                puppet.held_item = 0;
                *state->active_puppet = encrypt_puppet(&puppet);
                return false;
            }
        }
        return true;
    }

    default:
        _frames = 0;
        _state = 0;
        return false;
    }
}

static bool do_future(int player)
{
    static int _state = 0;
    static int _frames = 0;
    auto otherstate = get_battle_state(!player);

    switch(_state)
    {
    case 0:
    {
        if(g_future_turns[player] == 0)
            return false;
        if(--g_future_turns[player] > 0)
            return false;
        auto typemod = get_type_multiplier(player, nullptr, (uint16_t)g_id_future_skill, nullptr);
        if((otherstate->active_puppet != nullptr) && (typemod > 0) && (otherstate->active_ability != 297) && (otherstate->active_puppet->hp > 0))
        {
            auto otherpuppet = decrypt_puppet(otherstate->active_puppet);
            auto maxhp = calculate_stat(STAT_HP, &otherpuppet);
            auto dmg = (uint)(0.3 * typemod * maxhp);
            dmg = std::max(std::min(dmg, (uint)otherstate->active_puppet->hp), 1u);
            otherstate->active_puppet->hp -= (ushort)dmg;
            clear_battle_text();
            _state = 1;
            return true;
        }
        clear_battle_text();
        _state = 2;
        return true;
    }

    case 1:
    {
        auto name = std::string(otherstate->active_nickname);
        bool english = tpdp_eng_translation();
        if(player == 0)
            name = (english ? "Enemy " : "相手の　") + name;
        auto msg = name + (english ? " was hit by the prayer!" : " に 願いが 命中した！");
        if(set_battle_text(msg) != 1)
        {
            if(++_frames > get_game_fps())
            {
                _state = 0;
                _frames = 0;
                clear_battle_text();
                return false;
            }
        }
        return true;
    }

    case 2:
    {
        bool english = tpdp_eng_translation();
        if(set_battle_text(english ? "The prayer came true!\\nBut it had no effect..." : "願いが 叶いました！\\nしかし 効果が無いようだ……") != 1)
        {
            if(++_frames > get_game_fps())
            {
                _state = 0;
                _frames = 0;
                clear_battle_text();
                return false;
            }
        }
        return true;
    }

    default:
        _state = 0;
        _frames = 0;
        return false;
    }
}

// end-of-turn items
// also jam in end-of-turn abilities
static bool do_item(int player)
{
    auto state = get_battle_state(player);
    auto otherstate = get_battle_state(!player);
    auto puppet = decrypt_puppet(state->active_puppet);
    static int _state = 0;

    // needs to be carefully sequenced like this or fuckery(TM) will ensue
    switch(_state)
    {
    case 0:
        if((puppet.held_item == g_id_suzaku_seed) && (get_terrain_state()->terrain_type == TERRAIN_SUZAKU) && can_use_items(state) && do_suzaku_seed(player))
            return true;
        _state = 1;
        return true;

    case 1:
        if(((uint)state->active_ability == g_id_drain_abl) && ((uint)otherstate->active_ability != 297) && do_drain_abl(player))
            return true;
        _state = 2;
        return true;

    case 2:
        if (((uint)state->active_ability == g_id_possess) && (get_terrain_state()->weather_type == WEATHER_DUSTSTORM) && do_possess_heal(player))
            return true;
        _state = 3;
        return true;

    case 3:

        if (verdant_heal(player))
            return true;
        _state = 4;
        return true;

    case 4:
        if ((g_id_future_skill != ID_NONE) && do_future(player))
            return true;
        _state = 5;
        return true;

    case 5:
        if((puppet.held_item == g_id_mending) && can_use_items(state) && (state->active_ability != 252) && (otherstate->active_ability != 252) && mending_heal(player))
            return true;
        _state = 0;
        return false;

    default:
        _state = 0;
        return false;
    }
}

// low level hack to intercept item activation
int __cdecl item_dispatch(int player)
{
    auto item_state = *RVA(0x93cf78).ptr<uint32_t*>();
    static auto _state = 0;
    if((item_state == 0) && (_state == 0))
    {
        if(do_item(player))
            return 1;
        _state = 1;
    }

    if(RVA(0x33470).ptr<int(__cdecl*)(int)>()(player) == 0)
    {
        _state = 0;
        return 0;
    }

    return 1;
}

// add check for boots to mine trap
__declspec(naked)
void do_mine_trap()
{
    BattleState *state;
    __asm
    {
        jg nojmp // no mem operand encoding for Jcc instructions
        jmp g_mine_return_addr
        nojmp:
        pushad
        pushfd
        push ebp
        mov ebp, esp
        sub esp, __LOCAL_SIZE
        mov state, esi
    }

    static void *jmpaddr = nullptr;
    {
        jmpaddr = has_held_item(state, g_id_boots) ? g_mine_return_addr : RVA(0xbf8d);
    }

    __asm
    {
        mov esp, ebp
        pop ebp
        popfd
        popad
        jmp jmpaddr
    }
}

// This patches into the jump table for the "state machine"
// that controls most of the battle mechanics.
// Largely untested, enable it by uncommenting the corresponding
// line in init_misc_hacks() below.
/*
__declspec(naked)
void do_battle_state()
{
    uint32_t index;
    BattleState *state;
    __asm
    {
        pushad
        pushfd
        push ebp
        mov ebp, esp
        sub esp, __LOCAL_SIZE
        mov index, eax
        mov state, edi
    }

    static void *jmpaddr = nullptr;
    {
        if(index += 2)
            ++index;
        jmpaddr = RVA(0x2ccd4).ptr<void**>()[index];
    }

    __asm
    {
        mov esp, ebp
        pop ebp
        popfd
        popad
        jmp jmpaddr
    }
}*/

#if 0
void on_event(uint32_t index)
{
    auto pnext = RVA(0x975e2c).ptr<uint32_t*>();

    switch(index)
    {
    case 0x50:
        log_fatal(L"Test event");
        break;

    default:
        break;
    }
}

__declspec(naked)
void do_event()
{
    uint32_t index;
    __asm
    {
        pushad
        pushfd
        push ebp
        mov ebp, esp
        sub esp, __LOCAL_SIZE
        mov index, eax
    }

    static void *jmpaddr = nullptr;
    {
        if(index < 0x50)
            jmpaddr = RVA(0x177348).ptr<void**>()[index];
        else
            jmpaddr = RVA(0x174210).ptr<void*>();
        on_event(index);
    }

    __asm
    {
        mov esp, ebp
        pop ebp
        popfd
        popad
        jmp jmpaddr
    }
}

void patch_event_table()
{
    patch_jump(RVA(0x1742b9), &do_event);
    uint8_t max_evt = 0x52;
    patch_memory(RVA(0x1742b0 + 2), &max_evt, 1);
}
#endif

// add check for boots to stealth trap
__declspec(naked)
void do_stealth_trap()
{
    BattleState *state;
    __asm
    {
        jnz nojmp
        jmp g_stealth_return_addr
        nojmp:
        pushad
        pushfd
        push ebp
        mov ebp, esp
        sub esp, __LOCAL_SIZE
        mov state, edi
    }

    static void *jmpaddr = nullptr;
    {
        jmpaddr = has_held_item(state, g_id_boots) ? g_stealth_return_addr : RVA(0xc15c);
    }

    __asm
    {
        mov esp, ebp
        pop ebp
        popfd
        popad
        jmp jmpaddr
    }
}

// add check for boots to bind trap
__declspec(naked)
void do_bind_trap()
{
    BattleState *state;
    __asm
    {
        jnz nojmp
        jmp g_bind_return_addr
        nojmp:
        pushad
        pushfd
        push ebp
        mov ebp, esp
        sub esp, __LOCAL_SIZE
        mov state, edi
    }

    static void *jmpaddr = nullptr;
    {
        jmpaddr = has_held_item(state, g_id_boots) ? g_bind_return_addr : RVA(0xc299);
    }

    __asm
    {
        mov esp, ebp
        pop ebp
        popfd
        popad
        jmp jmpaddr
    }
}

// add check for boots to poison trap
__declspec(naked)
void do_pois_trap()
{
    BattleState *state;
    __asm
    {
        jnz nojmp
        jmp g_pois_return_addr
        nojmp:
        pushad
        pushfd
        push ebp
        mov ebp, esp
        sub esp, __LOCAL_SIZE
        mov state, edi
    }

    static void *jmpaddr = nullptr;
    {
        jmpaddr = has_held_item(state, g_id_boots) ? g_pois_return_addr : RVA(0xc411);
    }

    __asm
    {
        mov esp, ebp
        pop ebp
        popfd
        popad
        jmp jmpaddr
    }
}

// Crystal Mirror
// piggybacks off cursed doll by adding an extra condition to the item check
__declspec(naked)
int __stdcall do_byakko_seed(uint16_t item_id)
{
    BattleState *state;
    __asm
    {
        push ebp
        mov ebp, esp
        sub esp, __LOCAL_SIZE
        mov state, ebx
    }

    int result;
    {
        auto otherstate = get_battle_state(0);
        if(state == otherstate)
            otherstate = get_battle_state(1);

        if((get_terrain_state()->terrain_type == TERRAIN_BYAKKO) && can_use_items(otherstate) && has_held_item(otherstate, g_id_byakko_seed))
            result = 1;
        else if(has_held_item(state, item_id))
            result = 1;
        else
            result = 0;
    }

    __asm
    {
        mov eax, result
        mov esp, ebp
        pop ebp
        ret 4
    }
}

// Choice abilities
// piggybacks off choice items by adding an extra condition to the item check
__declspec(naked)
int __stdcall do_choice_check(uint16_t item_id)
{
    BattleState *state;
    __asm
    {
        push ebp
        mov ebp, esp
        sub esp, __LOCAL_SIZE
        mov state, ebx
    }

    int result;
    {
        if(((uint)state->active_ability == g_id_choice_focus) || ((uint)state->active_ability == g_id_choice_spread))
            result = 1;
        else if(has_held_item(state, item_id))
            result = 1;
        else
            result = 0;
    }

    __asm
    {
        mov eax, result
        mov ebx, state // ebx preserved across call
        mov esp, ebp
        pop ebp
        ret 4
    }
}

void __fastcall do_choice_msg(const char **ptr)
{
    auto state = get_battle_state(0);
    if(((uint)state->active_ability == g_id_choice_focus) || ((uint)state->active_ability == g_id_choice_spread))
    {
        *ptr = "A choice ability";
    }
}

__declspec(naked)
void _do_choice_msg()
{
    __asm
    {
        pushad
        lea ecx, [esp + 0x30]
        call do_choice_msg
        popad
        jmp g_sprintf_addr
    }
}

// NOTE: giant bit functions as *recoil*
// as such it targets the current player rather than
// the opponent
static int do_giant_bit(int player)
{
    auto state = get_battle_state(player);

    if(state->active_puppet == nullptr)
        return 0;

    static int _state = 0;
    static int _frames = 0;

    switch(_state)
    {
    case 0:
        if((state->active_puppet->hp != 0) && (state->dmg_dealt > 0) && (state->active_ability != 297))
        {
            auto puppet = decrypt_puppet(state->active_puppet);
            auto max_hp = calculate_stat(STAT_HP, &puppet);
            auto dmg = std::max(1u, max_hp / 8u);
            state->active_puppet->hp -= (ushort)std::min((unsigned int)state->active_puppet->hp, dmg);
            clear_battle_text();
            _state = 1;
            return 1;
        }
        return 0;

    case 1:
    {
        auto name = std::string(state->active_nickname);
        bool english = tpdp_eng_translation();
        if(player != 0)
            name = (english ? "Enemy " : "相手の　") + name;
        if(set_battle_text(name + (english ? " took damage from the\\nGiant Bit!" : "は　大型ビットの\\n打ち返しで　ダメージを　受けた！")) != 1)
        {
            if(++_frames > get_game_fps())
            {
                _frames = 0;
                _state = 0;
                return 0;
            }
        }
        return 1;
    }

    default:
        _state = 0;
        return 0;
    }
}

// calling convention fixup for calling skill effects
static int do_skill(int player, uint16_t effect_id, int effect_chance)
{
    void *func = RVA(0x45f00);
    int result;
    __asm
    {
        push effect_chance
        mov ecx, player
        movzx eax, effect_id
        call func
        add esp, 4
        mov result, eax
    }
    return result;
}

// Stone Stacker
static int do_stacking(int player, uint16_t effect_id, int effect_chance)
{
    auto activation_funcs = RVA(0x93ffa0).ptr<int(__cdecl**)(int)>();
    static int _state = 0;

    // skills that misbehave or can't be double casted properly
    static const uint16_t blacklist[] = {
        179, 167, 59, 186, 187, 151, 87, 208, 61, 231, 229, 92, 168, 189,
        221, 175, 57, 223, 89, 181, 191, 206, 207, 241, 81, 177, 180, 185,
        188, 193, 195, 204, 205, 203, 199, 212, 210, 211, 173, 174, 196, 57,
        198, 86, 230, 245, 252, 60
    };

    switch(_state)
    {
    case 0:
        for(auto& i : blacklist) // ignore blacklisted IDs
        {
            if(i == effect_id)
                return 0;
        }

        if(activation_funcs[effect_id](player) != 0)
        {
            reset_ability_activation(player);
            _state = 1;
            return 1;
        }
        return 0;

    case 1:
        if(show_ability_activation(player) == 0)
        {
            get_skill_state() = 0;
            *RVA(0x93ef9c).ptr<uint32_t*>() = 0;
            reset_status_anim();
            clear_battle_text();
            _state = 2;
        }
        return 1;

    case 2:
        if(do_skill(player, effect_id, effect_chance) == 0)
        {
            _state = 0;
            return 0;
        }
        return 1;

    default:
        _state = 0;
        return 0;
    }
}

// Absorber
static int do_resheal(int player)
{
    auto state = get_battle_state(player);
    auto otherstate = get_battle_state(!player);
    static int _state = 0;
    static int _frames = 0;
    static bool weak = false;

    if(otherstate->active_puppet == nullptr)
    {
        _state = 0;
        _frames = 0;
        return 0;
    }

    switch(_state)
    {
    case 0:
    {
        _frames = 0;
        auto otherpuppet = decrypt_puppet(otherstate->active_puppet);
        auto mul = get_type_multiplier(player, nullptr, 0, nullptr); // auto detect
        auto max_hp = (double)calculate_stat(STAT_HP, &otherpuppet);
        if((otherpuppet.hp != 0) && (otherpuppet.hp < max_hp) && (state->dmg_dealt > 0) && (mul < 1.0) && (mul > 0.0))
        {
            auto tstate = get_terrain_state();
            weak = false;
            for(auto i : otherpuppet.status_effects)
                if((i == STATUS_WEAK) || (i == STATUS_HVYWEAK))
                    weak = true;
            if(!weak)
            {
                if(tstate->terrain_type == TERRAIN_SUZAKU)
                    otherstate->active_puppet->hp = (ushort)std::max((double)otherpuppet.hp - std::max(max_hp * g_mod_resheal * 0.5, 1.0), 0.0);
                else
                    otherstate->active_puppet->hp = (ushort)std::min((double)otherpuppet.hp + std::max(max_hp * g_mod_resheal, 1.0), max_hp);
            }
            reset_heal_anim(!player);
            _state = 1;
            return 1;
        }
    }
        return 0;

    case 1:
        if(weak || (do_heal_anim() == 0))
            _state = 2;
        return 1;

    case 2:
    {
        if(weak || (state->num_attacks > 0))
        {
            _frames = 0;
            _state = 0;
            return 0;
        }
        constexpr auto enheal = " recovered HP with\\nAbsorber!";
        constexpr auto jpheal = "は　アブソーバーで\\n体力を　回復した！";
        constexpr auto endrain = " has lost HP due to Suzaku!";
        constexpr auto jpdrain = "は 朱雀に\\n体力を 奪われた……";
        auto name = std::string(otherstate->active_nickname);
        bool english = tpdp_eng_translation();
        bool drain = get_terrain_state()->terrain_type == TERRAIN_SUZAKU;
        auto enmsg = drain ? endrain : enheal;
        auto jpmsg = drain ? jpdrain : jpheal;
        if(player == 0)
            name = (english ? "Enemy " : "相手の　") + name;
        if(set_battle_text(name + (english ? enmsg : jpmsg)) != 1)
        {
            if(++_frames > get_game_fps())
            {
                _frames = 0;
                _state = 0;
                return 0;
            }
        }
        return 1;
    }

    default:
        _state = 0;
        _frames = 0;
        return 0;
    }
}

// Eden's Apple
static int do_anti_status(int player)
{
    static int _state = 0;
    static int _frames = 0;

    switch(_state)
    {
    case 0:
        _frames = 0;
        _state = 1;
        reset_ability_activation(!player);
        return 1;

    case 1:
        if(show_ability_activation(!player) == 0)
        {
            _state = 2;
            clear_battle_text();
        }
        return 1;

    case 2:
        if(set_battle_text("The skill seems to have no effect...") != 1)
        {
            if(++_frames > get_game_fps())
            {
                _frames = 0;
                _state = 0;
                return 0;
            }
        }
        return 1;

    default:
        _state = 0;
        _frames = 0;
        return 0;
    }
}

int do_form_change(int player, int pid, int style, bool switchin, bool possess)
{
    static auto _state = 0;
    static auto _frames = 0;
    auto state = get_battle_state(player);
    auto puppet = decrypt_puppet(state->active_puppet);

    int active_index = 0;
    auto bdata = &get_battle_data()[player];
    for(auto i = 0; i < 6; ++i)
    {
        if(&bdata->puppets[i] == state->active_puppet)
        {
            active_index = i;
            break;
        }
    }

    switch(_state)
    {
    case 0:
        //state->field_0x4e = 0x00;
        if (state->active_ability != (short)g_id_form_change) // wrong ability
        {
            if (state->active_ability != (short)g_id_possess)
            {
                return 0;
            }
            else if (possess && (state->active_puppet_id == pid) && (state->active_style_index == style))
                return 0;

        }
        if(switchin && !g_form_changes[player][active_index]) // not transformed yet
            return 0;
        else if(!switchin && g_form_changes[player][active_index]) // already transformed
            return 0;
        if ((puppet.puppet_id != g_id_form_base) || (puppet.style_index != g_id_form_base_style)) // wrong puppet
        {
            // guaranteed not yuuma
            if ((puppet.puppet_id != g_id_possess_base) || (puppet.style_index != g_id_possess_base_style))
            {
                // guaranteed not yuuma or possess
                return 0; // block transform
            }
        }
        if(puppet.hp < 1) // ded
            return 0;

        // don't bother transforming if the opponent has no puppets left
        if(!has_live_puppets(!player))
            return 0;

        reset_ability_activation(player);
        _state = 1;
        return 1;

    case 1:
        if(show_ability_activation(player) == 0)
        {
            reset_ability_activation(player);
            _state = 2;
        }
        return 1;

    case 2:
        state->field_0x4e += 0x10;
        if(state->field_0x4e >= 0xff)
        {
            auto& data = get_puppet_data()[pid];
            state->field_0x4e = 0xff;
            _state = 3;
            state->active_puppet_id = (ushort)pid;
            //state->field_0x79 = (undefined)style;
            state->active_style_index = (byte)style;
            state->active_type1 = data.styles[style].element1;
            state->active_type2 = data.styles[style].element2;
            auto newpuppet = puppet;
            newpuppet.puppet_id = (ushort)pid;
            newpuppet.style_index = (byte)style;
            for(auto i = 0; i < 6; ++i)
                state->puppet_stats[i] = (ushort)calculate_stat(i, &newpuppet);
            if (!possess)
                g_form_changes[player][active_index] = true;
        }
        return 1;

    case 3:
        state->field_0x4e -= 0x10;
        if(state->field_0x4e <= 0x00)
        {
            state->field_0x4e = 0x00;
            _state = 4;
            if(!switchin && !possess)
            {
                state->active_puppet->hp = (ushort)calculate_stat(STAT_HP, &puppet);
                reset_heal_anim(player);
            }
        }
        return 1;

    case 4:
        if(possess || switchin || (do_heal_anim() == 0))
        {
            _state = 5;
            clear_battle_text();
        }
        return 1;

    case 5:
    {
        auto name = std::string(state->active_nickname);
        bool english = tpdp_eng_translation();
        if(player != 0)
            name = (english ? "Enemy " : "相手の　") + name;
        if(set_battle_text(name + (english ? " has changed appearance!" : "は 姿が変わった！")) != 1)
        {
            if(++_frames > get_game_fps())
            {
                _frames = 0;
                _state = 0;
                return 0;
            }
        }
        return 1;
    }

    default:
        _state = 0;
        _frames = 0;
        return 0;
        break;
    }
}

static int gradation(int player, const SkillData& skilldata)
{
    auto otherstate = get_battle_state(!player);
    static int _state = 0;
    static auto _frames = 0;


    switch (_state)
    {
    case 0:
        if (((otherstate->active_type1 != skilldata.element) || (otherstate->active_type2 != ELEMENT_NONE)) && (otherstate->active_puppet->hp > 0))
        {
            otherstate->active_type1 = skilldata.element;
            otherstate->active_type2 = ELEMENT_NONE;
            reset_ability_activation(player);
            _state = 1;
            return 1;
        }
        return 0;

    case 1:
        if (show_ability_activation(player) == 0)
        {
            _state = 2;
        }
        return 1;

    case 2:
    {
        auto name = std::string(otherstate->active_nickname);
        bool english = tpdp_eng_translation();

        if (player == 0)
            name = (english ? "Enemy " : "相手の　") + name;
        auto msg = std::string(name) + (english ? " has changed type to " : " の属性は") + (get_element_string(skilldata.element)) + (english ? "!" : " に変わった！");
        if (set_battle_text(msg.c_str()) != 1)
        {
            if (++_frames > get_game_fps())
            {
                _state = 0;
                _frames = 0;
                return 0;
            }
        }
        return 1;
    }

    default:
        _state = 0;
        return 0;
    }
}

int do_imposter(int player)
{
    static auto _state = 0;
    static auto _frames = 0;
    auto state = get_battle_state(player);
    auto otherstate = get_battle_state(!player);
    auto puppet = decrypt_puppet(state->active_puppet);

    switch(_state)
    {
    case 0:
        if(state->active_ability != (short)g_id_imposter) // wrong ability
            return 0;
        if(puppet.hp < 1) // ded
            return 0;

        // don't bother transforming if the opponent has no puppets left
        if(!has_live_puppets(!player))
            return 0;

        reset_ability_activation(player);
        _state = 1;
        return 1;

    case 1:
        if(show_ability_activation(player) == 0)
        {
            reset_ability_activation(player);
            _state = 2;
        }
        return 1;

    case 2:
        state->field_0x4e += 0x10;
        if(state->field_0x4e >= 0xff)
        {
            state->field_0x4e = 0xff;
            _state = 3;
            state->active_puppet_id = otherstate->active_puppet_id;
            //state->field_0x79 = (undefined)style;
            state->active_style_index = otherstate->active_style_index;
            state->active_type1 = otherstate->active_type1;
            state->active_type2 = otherstate->active_type2;
            state->active_ability = otherstate->active_ability;
            for(auto i = 1; i < 6; ++i) // NOTE: change 'i = 1' to 'i = 0' to copy HP stat as well 
                state->puppet_stats[i] = otherstate->puppet_stats[i];
            for(auto i = 0; i < 4; ++i)
            {
                state->puppet_skills[i] = otherstate->puppet_skills[i];
                state->puppet_sp[i] = otherstate->puppet_sp[i];
            }
        }
        return 1;

    case 3:
        state->field_0x4e -= 0x10;
        if(state->field_0x4e <= 0x00)
        {
            state->field_0x4e = 0x00;
            _state = 4;
            clear_battle_text();
        }
        return 1;

    case 4:
    {
        auto name = std::string(state->active_nickname);
        auto othername = std::string(otherstate->active_nickname);
        bool english = tpdp_eng_translation();
        if(player != 0)
            name = (english ? "Enemy " : "相手の　") + name;
        if(set_battle_text(name + (english ? " has copied " + othername + "!" : " は\\n" + othername + " に変わった！")) != 1)
        {
            if(++_frames > get_game_fps())
            {
                _frames = 0;
                _state = 0;
                return 0;
            }
        }
        return 1;
    }

    default:
        _state = 0;
        _frames = 0;
        return 0;
        break;
    }
}

static int do_morale(int player)
{
    static auto _state = 0;

    switch(_state)
    {
    case 0:
        reset_stat_mod();
        _state = 1;
        return 1;

    case 1:
        if (show_ability_activation(player) == 0)
        {
            reset_ability_activation(player);
            _state = 2;
        }
        return 1;

    case 2:
        if(apply_stat_mod(STAT_SPATK, 1, player) == 0)
        {
            _state = 0;
            return 0;
        }
        return 1;

    default:
        _state = 0;
        return 0;
    }
}

static int do_warmup(int player)
{
    static auto _state = 0;

    switch (_state)
    {
    case 0:
        reset_stat_mod();
        _state = 1;
        return 1;

    case 1:
        if (show_ability_activation(player) == 0)
        {
            reset_ability_activation(player);
            _state = 2;
        }
        return 1;

    case 2:
        if (apply_stat_mod(STAT_SPEED, 2, player) == 0)
        {
            _state = 0;
            return 0;
        }
        return 1;

    default:
        _state = 0;
        return 0;
    }
}
/*
static int do_feast(int player)
{
    auto state = get_battle_state(player);
    auto otherstate = get_battle_state(!player);
    static int _state = 0;
    static int _frames = 0;
    static bool weak = false;

        switch (_state)
        {
        case 0:
        {
            _frames = 0;
            auto puppet = decrypt_puppet(state->active_puppet);
            auto max_hp = (double)calculate_stat(STAT_HP, &puppet);
            if ((puppet.hp != 0) && (puppet.hp < max_hp))
            {
                auto tstate = get_terrain_state();
                weak = false;
                for (auto state->active_puppet : state->active_puppet.status_effects)
                    if ((state->active_puppet == STATUS_WEAK) || (state->active_puppet == STATUS_HVYWEAK))
                        weak = true;
                if (!weak)
                {
                    if (tstate->terrain_type == TERRAIN_SUZAKU)
                        state->active_puppet->hp = (ushort)std::max((double)state->active_puppet->hp - std::max(max_hp * g_mod_feast * 0.5, 1.0), 0.0);
                    else
                        state->active_puppet->hp = (ushort)std::min((double)state->active_puppet->hp + std::max(max_hp * g_mod_feast, 1.0), max_hp);
                }
                reset_heal_anim(player);
                _state = 1;
                return 1;
            }
        }
        return 0;

        case 1:
            if (weak || (do_heal_anim() == 0))
                _state = 2;
            return 1;

        case 2:
        {
            if (weak || (state->num_attacks > 0))
            {
                _frames = 0;
                _state = 0;
                return 0;
            }
            constexpr auto enheal = " recovered HP with\\nAbsorber!";
            constexpr auto jpheal = "は　アブソーバーで\\n体力を　回復した！";
            constexpr auto endrain = " has lost HP due to Suzaku!";
            constexpr auto jpdrain = "は 朱雀に\\n体力を 奪われた……";
            auto name = std::string(otherstate->active_nickname);
            bool english = tpdp_eng_translation();
            bool drain = get_terrain_state()->terrain_type == TERRAIN_SUZAKU;
            auto enmsg = drain ? endrain : enheal;
            auto jpmsg = drain ? jpdrain : jpheal;
            if (player == 0)
                name = (english ? "Enemy " : "相手の　") + name;
            if (set_battle_text(name + (english ? enmsg : jpmsg)) != 1)
            {
                if (++_frames > get_game_fps())
                {
                    _frames = 0;
                    _state = 0;
                    return 0;
                }
            }
            return 1;
        }

        default:
            _state = 0;
            _frames = 0;
            return 0;
        }
}*/

static int do_curse(int player)
{
    static auto _state = 0;
    static auto _frames = 0;
    auto state = get_battle_state(player);

    switch(_state)
    {
    case 0:
        if((state->ruinous_turns > 0) || (get_rng(99) >= 30))
            return 0;

        state->ruinous_turns = 4;
        reset_ability_activation(!player);
        _state = 1;
        return 1;

    case 1:
        if(show_ability_activation(!player) == 0)
        {
            clear_battle_text();
            _state = 2;
        }
        return 1;

    case 2:
    {
        auto name = std::string(state->active_nickname);
        bool english = tpdp_eng_translation();
        if(player != 0)
            name = (english ? "Enemy " : "相手の　") + name;
        auto msg = name + (english ? " has been cursed!" : " は呪われてしまった！");
        if(set_battle_text(msg.c_str()) != 1)
        {
            if(++_frames > get_game_fps())
            {
                _state = 0;
                _frames = 0;
                return 0;
            }
        }
        return 1;
    }

    default:
        _state = 0;
        return 0;
    }
}

// MASSIVE JANK
// override execution of skill effects
static int skill_dispatch(int player, uint16_t effect_id, int effect_chance)
{
    auto state = get_battle_state(player);
    auto otherstate = get_battle_state(!player);
    //auto activation_funcs = RVA(0x93ffa0).ptr<int(__cdecl**)(int)>();
    //auto effect_funcs = RVA(0x942fb0).ptr<uint32_t*>();
    //auto skill_id = (unsigned int)state->active_skill_id;
    auto& skilldata = *do_adjusted_skill_data(player, 0);

    static int _state = 0;

    // needs to be carefully sequenced like this or fuckery(TM) will ensue
    switch(_state)
    {
    case 0:
        if (((unsigned int)otherstate->active_ability == g_id_anti_status) && (skilldata.type == SKILL_TYPE_STATUS))
            return do_anti_status(player); // Eden's Apple
        if(do_skill(player, effect_id, effect_chance) == 0)
            _state = 1;
        return 1;

    case 1:
        if(((unsigned int)state->active_ability == g_id_stacking) && (skilldata.type == SKILL_TYPE_STATUS) && (do_stacking(player, effect_id, effect_chance) != 0))
            return 1;
        _state = 2;
        return 1;

    case 2:
        if(has_held_item(otherstate, g_id_giant_bit) && can_use_items(otherstate) && (skilldata.power >= g_power_giant_bit) && (do_giant_bit(player) != 0))
            return 1;
        _state = 3;
        return 1;

    case 3:
        if(has_held_item(otherstate, g_id_resheal) && can_use_items(otherstate) && (skilldata.type != SKILL_TYPE_STATUS) && (do_resheal(player) != 0))
            return 1;
        _state = 4;
        return 1;

    case 4:
        if((state->active_ability == (short)g_id_form_change) && (state->dmg_dealt > 0) && (otherstate->active_puppet->hp <= 0) && (do_form_change(player, g_id_form_target, g_id_form_target_style, false) != 0))
            return 1;
        _state = 5;
        return 1;

    case 5:
        if((state->active_ability == (short)g_id_morale) && (state->dmg_dealt > 0) && (otherstate->active_puppet->hp <= 0) && (do_morale(player) != 0))
            return 1;
        _state = 6;
        return 1;

    case 6:
        if((state->dmg_dealt > 0) && (otherstate->active_puppet->hp <= 0) && (do_imposter(player) != 0))
            return 1;
        _state = 8;
        return 1;

   /*case 7:
        if ((state->active_ability == (short)g_id_feast) && (state->dmg_dealt > 0) && (otherstate->active_puppet->hp <= 0) && (do_feast(player) != 0))
            return 1;
        _state = 8;
        return 1;*/ 

    case 8:
        if ((state->active_ability == (short)g_id_warmup) && (state->dmg_dealt > 0) && (otherstate->active_puppet->hp <= 0) && (do_warmup(player) != 0))
            return 1;
        _state = 9;
        return 1;

    case 9:
        if((otherstate->active_ability == (short)g_id_curse) && (state->dmg_dealt > 0) && (state->active_puppet->hp > 0) && (do_curse(player) != 0))
            return 1;
        _state = 10;
        return 1;

    case 10:
        if ((state->active_ability == (short)g_id_gradate) && (skilldata.type != SKILL_TYPE_STATUS) && (gradation(player, skilldata) != 0))
            return 1;

        _state = 0;
        return 0;

    default:
        _state = 0;
        return 0;
    }
}

// calling convention fixup stub
__declspec(naked)
int __cdecl _skill_dispatch(int effect_chance)
{
    int player;
    uint16_t effect_id;
    __asm
    {
        push ebp
        mov ebp, esp
        sub esp, __LOCAL_SIZE
        mov player, ecx
        mov effect_id, ax
    }

    int result;
    result = skill_dispatch(player, effect_id, effect_chance);

    __asm
    {
        mov eax, result
        mov esp, ebp
        pop ebp
        ret
    }
}

// apply hack for Heavy Duty Boots AKA Tengu Geta
static void patch_boots()
{
    g_mine_return_addr = RVA(0xc015);
    g_stealth_return_addr = RVA(0xc1e4);
    g_bind_return_addr = RVA(0xc35c);
    g_pois_return_addr = RVA(0xc5b7);

    patch_jump(RVA(0xbf87), &do_mine_trap);
    patch_jump(RVA(0xc156), &do_stealth_trap);
    patch_jump(RVA(0xc293), &do_bind_trap);
    patch_jump(RVA(0xc40b), &do_pois_trap);
}

// apply hack for Crystal Mirror
static void patch_byakko_seed()
{
    patch_call(RVA(0x2d9e2), &do_byakko_seed);
    patch_call(RVA(0x2da9a), &do_byakko_seed);
}

// fix bag auto sort cause it doesn't recognize new hold2 items
static void do_bag_sort(unsigned int pocket)
{
    //only the hold2 and key items pockets, delegate rest to original sorter function
    auto func = RVA(0x17c540).ptr<void(__cdecl*)(unsigned int)>();
    if((pocket != 3) && (pocket != 6))
        return func(pocket);

    // collect and sort unique IDs from pocket
    const auto sz = (pocket == 3) ? 0x80u : 0x20u;
    const auto pocketbuf = (pocket == 3) ? RVA(0xD7C147).ptr<uint16_t*>() : RVA(0xD7C3C7).ptr<uint16_t*>();
    std::set<uint16_t> ids;
    for(auto i = 0u; i < sz; ++i)
        if(pocketbuf[i] != 0)
            ids.insert(pocketbuf[i]);

    // clear pocket
    memset(pocketbuf, 0, sz * sizeof(uint16_t));

    // fill pocket with sorted list
    auto i = 0;
    for(auto j : ids)
        pocketbuf[i++] = j;
}

// override exp gain from battles
/*
static void do_exp_yield()
{
    auto func = RVA(0x1e8a0).ptr<VoidCall>();
    auto expbuf = RVA(0x93cc40).ptr<uint*>();
    auto gamestate = *RVA(0x93c130).ptr<uint*>();
    func();
    if(gamestate == 0)
    {
        for(auto i = 0; i < 6; ++i)
            expbuf[i] *= 69;
        memcpy(RVA(0x6fd4bd8), expbuf, sizeof(uint) * 6);
    }
}
*/

// override the games update checker to avoid
// "couldn't check for newest version" errors
static double __cdecl get_newest_version()
{
    return 1.103; // yes it's a fucking float
}

#if 0
__declspec(naked)
void do_music_hack()
{
    static uint32_t loop_pos = 0;
    static uint32_t index = 0;

    __asm
    {
        jnc nojmp
        jmp g_music_return_addr
        nojmp:
        pushad
        pushfd
        push ebp
        mov ebp, esp
        sub esp, __LOCAL_SIZE
        mov loop_pos, ecx
        mov index, eax
    }

    {
        auto newindex = index - 0x6b;
        void *loop_buf = RVA(0x6fd27b8);
        void *handle_buf = RVA(0x9956D0);
        auto loop_diff = (uintptr_t)g_music_loop_buf.get() - (uintptr_t)loop_buf;
        auto handle_diff = (uintptr_t)g_music_handle_buf.get() - (uintptr_t)handle_buf;
        if((loop_diff % 4) != 0)
            log_error(L"Music loop buffer misaligned!");
        if((handle_diff % 4) != 0)
            log_error(L"Music handle buffer misaligned!");
        if(newindex <= 5)
        {
            index = (handle_diff / 4u) + newindex;
            loop_pos = g_music_loop_buf[newindex];
        }
        else
        {
            index = 0;
            loop_pos = 0;
        }
    }

    __asm
    {
        mov esp, ebp
        pop ebp
        popfd
        popad
        mov ecx, loop_pos
        mov eax, index
        jmp g_music_hackreturn_addr
    }
}
#endif

static uint32_t load_music_file(uint32_t *outptr, int unkint, const char *filepath)
{
    void *func = RVA(0x1b24e0);
    uint32_t retval = 0;

    __asm
    {
        push unkint
        push outptr
        mov ebx, filepath
        call func
        add esp, 8
        mov retval, eax
    }

    return retval;
}

static uint32_t load_music_file(uint32_t *outptr, int unkint, const std::string& filepath)
{
    return load_music_file(outptr, unkint, filepath.c_str());
}

static int set_music_loop(uint32_t loop_pos, uint32_t handle)
{
    auto func = RVA(0x20c860).ptr<int(__cdecl*)(uint32_t, uint32_t)>();
    return func(loop_pos, handle);
}

static void load_extra_music()
{
    constexpr auto max_songs = 256u;
    constexpr auto total_loop_sz = (max_songs * 0x1cu) + 0xd60u;
    g_music_handle_buf = std::make_unique<uint32_t[]>(max_songs);
    g_music_loop_buf = std::make_unique<uint8_t[]>(total_loop_sz);
    memset(g_music_handle_buf.get(), 0, sizeof(uint32_t) * max_songs);
    memset(g_music_loop_buf.get(), 0, total_loop_sz);

    void *orig_loop_buf = RVA(0x6fd27b8);
    void *orig_handle_buf = RVA(0x9956D0);

    void *newbuf = g_music_handle_buf.get();
    scan_and_replace(&orig_handle_buf, &newbuf, 4);
    newbuf = &g_music_handle_buf[1];
    orig_handle_buf = RVA(0x9956D4);
    scan_and_replace(&orig_handle_buf, &newbuf, 4);
    newbuf = g_music_loop_buf.get();
    scan_and_replace(&orig_loop_buf, &newbuf, 4);

    // instructions that cap music id
    // address of imm8 byte of cmp reg, 0x6b
    uintptr_t rvas[] = {
        0x17cf82,
        0x17d49a,
        0x17d853,
        0x17d8af,
        0x17dc5e, // playback
        0x17e0cd,
        0x17e158
    };

    auto newcap = 0xffu;
    for(auto i : rvas)
        patch_memory(RVA(i), &newcap, 1);

    libtpdp::Archive arc;
    try
    {
        arc.open(L"dat\\gn_dat4.arc");
    }
    catch(libtpdp::ArcError ex)
    {
        LOG_ERROR() << L"Could not open gn_dat4.arc: " << ex.what();
        return;
    }

    auto csvfile = arc.get_file("bgm\\BgmData.csv");
    if(!csvfile)
    {
        LOG_ERROR() << L"Could not read BgmData.csv!";
        return;
    }

    libtpdp::CSVFile csv;
    if(!csv.parse(csvfile.data(), csvfile.size()))
    {
        LOG_ERROR() << L"Could not parse BgmData.csv!";
        return;
    }

    auto count = 0;
    for(auto& line : csv)
    {
        if(line.size() < 4)
            continue;

        auto sid = 0u;
        auto sample = 0u;
        try
        {
            sid = std::stoul(line[0]);
            sample = std::stoul(line[3]);
        }
        catch(...)
        {
            continue;
        }

        if((sid >= 0x6bu) && (sid < 256u))
        {
            auto filename = line[1] + L".ogg";
            auto path = utf_narrow(L"dat\\gn_dat4\\bgm\\" + filename);
            load_music_file(&g_music_handle_buf[sid], 3, path);
            set_music_loop(sample, g_music_handle_buf[sid]);
            *(uint32_t*)&g_music_loop_buf[sid * sizeof(uint32_t)] = sample;
            ++count;
        }
    }

    LOG_DEBUG() << L"Loaded " << count << L" extra songs";
}

static void do_music_init()
{
    auto func = RVA(0x17d3e0).ptr<VoidCall>();
    func();

    std::call_once(g_music_init, &load_extra_music);

    void *orig_loop_buf = RVA(0x6fd27b8);
    void *orig_handle_buf = RVA(0x9956D0);
    memcpy(g_music_handle_buf.get(), orig_handle_buf, sizeof(uint32_t) * 0x6b);
    memcpy(g_music_loop_buf.get(), orig_loop_buf, sizeof(uint32_t) * 0x6b);
    memcpy(g_music_loop_buf.get() + 0xd60u, RVA(0x6fd27b8 + 0xd60u), 0x6b * 0x1cu);
}

static void patch_bgm_limit()
{
    patch_call(RVA(0x1afb14), &do_music_init);
    //patch_jump(RVA(0x17dc5f), &do_music_hack);
    //g_music_return_addr = RVA(0x17dc65);
    //g_music_hackreturn_addr = RVA(0x17dc6c);
}

static void patch_network_addrs(uint32_t ip, uint32_t port)
{
    patch_memory(RVA(0x189969 + 6), &ip, sizeof(uint32_t));
    patch_memory(RVA(0x189979 + 1), &port, sizeof(uint32_t));

    auto url = g_website_url.c_str();
    void *ptr = RVA(0x42d300);
    scan_and_replace(&ptr, &url, sizeof(void*));
    patch_memory(RVA(0x1886b2 + 1), &url, sizeof(void*));
}

static void patch_miracle()
{
    uint8_t b = 0;
    patch_memory(RVA(0x2e409 + 2), &b, sizeof(b));
}

static void patch_catch_rate(uint8_t lvl_factor)
{
    uint8_t code[] = {
        0x6b, 0xd0, lvl_factor, // imul edx, eax, imm8
        0xeb, 0x04              // jmp rel8off 4
    };
    patch_memory(RVA(0xcea4), code, sizeof(code));
}

static void patch_level_cap(unsigned int player_lvl, unsigned int ai_lvl)
{
    for(size_t i = 0; i < 5; ++i)
    {
        *(uint32_t*)(&g_player_level_cap[i * 0x198]) = libtpdp::exp_for_level(i, player_lvl, true);
        *(uint32_t*)(&g_ai_level_cap[i * 0x198]) = libtpdp::exp_for_level(i, ai_lvl, true);
    }

    void *ai_addr = RVA(0x1566d1 + 2);
    void *player_addrs[] = { RVA(0x1659aa + 2), RVA(0x189864 + 2), RVA(0x1898c4 + 2) };

    void *addr = g_ai_level_cap.get();
    patch_memory(ai_addr, &addr, sizeof(addr));
    addr = g_player_level_cap.get();
    for(auto dest : player_addrs)
        patch_memory(dest, &addr, sizeof(addr));
}

static void unpatch_level_cap()
{
    void *addrs[] = { RVA(0x1566d1 + 2), RVA(0x1659aa + 2), RVA(0x189864 + 2), RVA(0x1898c4 + 2) };

    void *addr = RVA(0x93ac60);
    for(auto dest : addrs)
        patch_memory(dest, &addr, sizeof(addr));
}

void __cdecl load_dod_file(uint16_t id, [[maybe_unused]] bool set_exp)
{
    auto fun = RVA(0x156590).ptr<void(__cdecl*)(uint16_t, bool)>();
    fun(id, true);
}

void on_event(uint32_t index)
{
    auto pnext = RVA(0x975e2c).ptr<uint32_t*>();
    auto save_buf = RVA(0xd7c9f1).ptr<uint16_t*>();
    auto evs_buf = RVA(0xC69480).ptr<uint16_t*>();
    auto evs_index = *RVA(0x975E2C).ptr<uint32_t*>();

    switch (index)
    {
    case 0x50:
    {
        auto ptr = RVA(0xc47234).ptr<uint8_t*>();
        *ptr = 0;
        *pnext += 1;
        break;
    }

    case 0x51:
    {
        std::ofstream f("save\\extended.ini", std::ios::trunc);
        f << "[general]\r\nunlock_puppets=true";
        *pnext += 1;
        break;
    }

    case 0x52:
    {
        std::ofstream f("save\\extended.ini", std::ios::trunc);
        f << "[general]\r\nunlock_puppets=false";
        *pnext += 1;
        break;
    }

    case 0x53:
    {
        auto player_level = evs_buf[(evs_index * 10) + 1];
        auto ai_level = evs_buf[(evs_index * 10) + 2];
        if(player_level > 0)
            save_buf[0x80 - 1] = player_level;
        if(ai_level > 0)
            save_buf[0x80 - 2] = ai_level;
        ++(*pnext);
    }
    break;

    case 0x54:
    {
        auto player_level = evs_buf[(evs_index * 10) + 1];
        auto ai_level = evs_buf[(evs_index * 10) + 2];
        patch_level_cap(player_level ? player_level : save_buf[0x80 - 1], ai_level ? ai_level : save_buf[0x80 - 2]);
        if(evs_buf[(evs_index * 10) + 3] > 0)
            scan_and_replace_call(RVA(0x156590), &load_dod_file);
        ++(*pnext);
    }
    break;

    case 0x55:
        unpatch_level_cap();
        scan_and_replace_call(&load_dod_file, RVA(0x156590));
        ++(*pnext);
        break;

    default:
        break;
    }
}

__declspec(naked)
void do_event()
{
    uint32_t index;
    __asm
    {
        pushad
        pushfd
        push ebp
        mov ebp, esp
        sub esp, __LOCAL_SIZE
        mov index, eax
    }

    static void* jmpaddr = nullptr;
    {
        if (index < 0x50)
            jmpaddr = RVA(0x177348).ptr<void**>()[index];
        else
            jmpaddr = RVA(0x174210).ptr<void*>();
        on_event(index);
    }

    __asm
    {
        mov esp, ebp
        pop ebp
        popfd
        popad
        jmp jmpaddr
    }
}

void patch_event_table()
{
    patch_jump(RVA(0x1742b9), &do_event);
    uint8_t max_evt = 0x55;
    patch_memory(RVA(0x1742b0 + 2), &max_evt, 1);
}

// swap strings to force loading of alternate dolldata file
void swap_dolldata()
{
    libtpdp::Archive arc;
    libtpdp::CSVFile names;

    try
    {
        arc.open(L"dat\\gn_dat5.arc");
    }
    catch(libtpdp::ArcError ex)
    {
        LOG_ERROR() << L"Could not open gn_dat6.arc: " << ex.what();
        return;
    }

    {
        auto f = arc.get_file("name\\DollName.csv");
        if(!f)
        {
            LOG_ERROR() << L"Could not read DollName.csv!";
            return;
        }
        names.parse(f.data(), f.size());
    }
    arc.close();

    try
    {
        arc.open(L"dat\\gn_dat6.arc");
    }
    catch(libtpdp::ArcError ex)
    {
        LOG_ERROR() << L"Could not open gn_dat6.arc: " << ex.what();
        return;
    }

    // swap strings referencing DollData.dbs to DollData2.dbs
    auto dd1 = "%s\\DollData2.dbs";
    auto dd2 = "%s\\%s\\DollData2.dbs";
    void *addr1 = RVA(0x43aec4);
    void *addr2 = RVA(0x4414a4);
    IniFile ini("save\\extended.ini");
    if(ini.get_bool("general", "unlock_puppets"))
    {
        // directly overwrite the memory buffer since it won't get reloaded without a hard reboot
        auto dd = arc.get_file("doll\\DollData2.dbs");
        if(!dd)
        {
            LOG_ERROR() << L"Could not read DollData2.dbs!";
            return;
        }
        else
        {
            auto ptr = get_puppet_data();
            memcpy(ptr, dd.data(), dd.size());
        }

        // swap original strings for new ones
        scan_and_replace(&addr1, &dd1, sizeof(void*));
        scan_and_replace(&addr2, &dd2, sizeof(void*));
    }
    else
    {
        // directly overwrite the memory buffer since it won't get reloaded without a hard reboot
        auto dd = arc.get_file("doll\\DollData.dbs");
        if(!dd)
        {
            LOG_ERROR() << L"Could not read DollData.dbs!";
            return;
        }
        else
        {
            auto ptr = get_puppet_data();
            memcpy(ptr, dd.data(), dd.size());
        }

        // swap the original strings back in if previously swapped
        scan_and_replace(&dd1, &addr1, sizeof(void*));
        scan_and_replace(&dd2, &addr2, sizeof(void*));
    }

    // parse puppet names and write them to the name field in the DollData binary
    auto data = get_puppet_data();
    for(size_t i = 0; i < names.num_lines(); ++i)
    {
        if(i >= 512)
            break;
        auto& entry = names[i];
        if(entry.size() > 0)
        {
            auto str = utf_to_sjis(entry[0]);
            memcpy(&data[i].field_0x0, str.c_str(), std::min(str.size(), 31u));
            data[i].field_0x0[std::min(str.size(), 31u)] = 0; // null terminate
        }
    }
}

void load_background()
{
    libtpdp::Archive arc;
    libtpdp::CSVFile names;
    auto buf = RVA(0x93c188).ptr<uint32_t*>();
    auto id = *RVA(0x93bcd7).ptr<uint8_t*>();

    try
    {
        arc.open(L"dat\\gn_dat5.arc");
    }
    catch(libtpdp::ArcError ex)
    {
        LOG_ERROR() << L"Could not open gn_dat6.arc: " << ex.what();
        return;
    }

    {
        auto f = arc.get_file("name\\MapLocationName.csv");
        if(!f)
        {
            LOG_ERROR() << L"Could not read MapLocationName.csv!";
            return;
        }
        names.parse(f.data(), f.size());
    }
    arc.close();

    try
    {
        arc.open(L"dat\\gn_dat1.arc");
    }
    catch(libtpdp::ArcError ex)
    {
        LOG_ERROR() << L"Could not open gn_dat1.arc: " << ex.what();
        return;
    }

    if(id >= names.num_lines())
        return;
    auto& entry = names[id];
    if(entry.size() < 2)
        return;
    auto basename = utf_to_sjis(entry[1]);
    for(size_t i = 0; i < 16; ++i)
    {
        if(buf[i] == 0)
        {
            auto filepath = "battle\\locationBG\\" + basename + std::to_string(i) + ".png";
            if(arc.find(filepath) != arc.end()) // check for file existence
            {
                filepath = "dat\\gn_dat1\\" + filepath;
                buf[i] = LoadGraph(filepath.c_str());
            }
        }
    }
}

__declspec(naked)
static void _load_background()
{
    __asm
    {
        pushad
        pushfd
        push ebp
        mov ebp, esp
        sub esp, __LOCAL_SIZE
    }

    {
        load_background();
    }

    __asm
    {
        mov esp, ebp
        pop ebp
        popfd
        popad
        jmp g_background_return_addr
    }
}

void on_main_loop(uint32_t index)
{
    switch(index)
    {
    case 0:
        swap_dolldata();
        break;

    default:
        return;
    }
}

__declspec(naked)
void do_main_loop()
{
    uint32_t index;
    __asm
    {
        pushad
        pushfd
        push ebp
        mov ebp, esp
        sub esp, __LOCAL_SIZE
        mov index, eax
    }

    static void* jmpaddr = nullptr;
    {
        jmpaddr = RVA(0x1afa28).ptr<void**>()[index];
        on_main_loop(index);
    }

    __asm
    {
        mov esp, ebp
        pop ebp
        popfd
        popad
        jmp jmpaddr
    }
}

void patch_main_loop()
{
    patch_jump(RVA(0x1af855), &do_main_loop);
}

void do_backgrounds()
{
    auto func = RVA(0x24aa0).ptr<VoidCall>();
    auto id = *RVA(0x93bcd7).ptr<uint8_t*>();
    auto draw_handles = RVA(0x93c0e0).ptr<uint32_t*>(); // uint32_t[16] (?) handles to be drawn this frame
    auto sprite_handles = RVA(0x93c188).ptr<uint32_t*>(); // uint32_t[16] handles for all sprites allocated for this background

    switch(id)
    {
    default:
        func();
        break;
    }
}

static void do_background_bgm()
{
    auto background_id = *RVA(0xc5996a).ptr<uint8_t*>();
    auto bgm_id_ptr = RVA(0xc57871).ptr<uint8_t*>();

    switch(background_id)
    {
        // vanilla backgrounds
    case 3:
    case 0xf:
    case 0x19:
    case 0x29:
    case 0x33:
    case 0x34:
    case 0x35:
        *bgm_id_ptr = 0x5c;
        break;
    case 4:
    case 9:
    case 0xd:
    case 0x13:
    case 0x1a:
    case 0x1b:
    case 0x1c:
    case 0x1d:
        *bgm_id_ptr = 0x5d;
        break;
    case 5:
    case 0x14:
    case 0x15:
    case 0x16:
    case 0x17:
    case 0x18:
    case 0x24:
        *bgm_id_ptr = 0x5e;
        break;
    default:
        *bgm_id_ptr = 0x50;
        break;
    case 0x11:
    case 0x2e:
    case 0x3a:
        *bgm_id_ptr = 0x62;
        break;
    case 0x2c:
    case 0x2d:
    case 0x39:
    case 0x3b:
    case 0x3c:
        *bgm_id_ptr = 0x47;
        break;

        // custom entries begin here
    case 0x3d:
        *bgm_id_ptr = 0x40;
        break;
    }
}

__declspec(naked)
static void _do_background_bgm()
{
    __asm
    {
        pushad
        pushfd
        push ebp
        mov ebp, esp
        sub esp, __LOCAL_SIZE
    }

    {
        do_background_bgm();
    }

    __asm
    {
        mov esp, ebp
        pop ebp
        popfd
        popad
        jmp g_background_bgm_return_addr
    }
}

void patch_backgrounds()
{
    g_background_return_addr = RVA(0x19bbc).ptr<void*>();
    g_background_bgm_return_addr = RVA(0x175fbe).ptr<void*>();
    patch_call(RVA(0x2528b), &do_backgrounds);
    patch_jump(RVA(0x19ab0), &_load_background);
    patch_jump(RVA(0x175f77), &_do_background_bgm);
}

static void patch_dollicon()
{
    void *ptr = RVA(0x974328);
    void *newptr = g_dollicon_handle_buf.get();
    scan_and_replace(&ptr, &newptr,sizeof(ptr)); // swap references to original handle buffer with our larger one
    uint8_t width = DOLLICON_WIDTH;
    uint8_t height = DOLLICON_HEIGHT;
    uint32_t count = DOLLICON_COUNT;
    patch_memory(RVA(0x1afb3d + 1) /* push imm8 */, &height, sizeof(height));
    patch_memory(RVA(0x1afb3f + 1) /* push imm8 */, &width, sizeof(width));
    patch_memory(RVA(0x1afb41 + 1) /* push imm32 */, &count, sizeof(count));
}

// this is wedged into the middle of the function that draws the puppet box
// so that we can draw icons on top of the doll icons but below other UI elements
// like the cursor
static void draw_box_icons()
{
    auto data = get_puppet_data();
    auto styleicons = RVA(0xc4714c).ptr<uint32_t*>();
    auto xpos = RVA(0x9538d0).ptr<float*>();
    auto party_puppets = RVA(0x4ea988).ptr<PartyPuppet**>();

    // party puppets
    for(size_t i = 0; i < 6; ++i)
    {
        if(party_puppets[i] == nullptr)
            continue;
        PartyPuppet puppet{};
        memcpy(&puppet, party_puppets[i], libtpdp::PUPPET_SIZE); // unknown struct length, copy minimum amount of data
        decrypt_puppet_nocopy(&puppet);
        auto offset = (int)xpos[i]; // unclear what this is for but original code uses it
        int x = offset + 32 + (i * 36);
        int y = 49;
        DrawGraph(x, y, styleicons[data[puppet.puppet_id].styles[puppet.style_index & 3].style_type], 1);
        DrawGraph(x, y + 10, g_mark_handle_buf[puppet.mark], 1);
    }

    // box puppets
    for(size_t i = 0; i < 30; ++i)
    {
        auto buf = RVA(0xc60b88).ptr<uint8_t*>();
        PartyPuppet puppet{};
        memcpy(&puppet, &buf[i * libtpdp::PUPPET_SIZE_PARTY], libtpdp::PUPPET_SIZE_PARTY); // shorter than PartyPuppet
        decrypt_puppet_nocopy(&puppet);
        auto offset = (int)*RVA(0x953950).ptr<float*>(); // for sliding animation when switching boxes
        int x = offset + 32 + ((i % 6) * 36);
        int y = ((i / 6) * 40) + 118;
        DrawGraph(x, y, styleicons[data[puppet.puppet_id].styles[puppet.style_index & 3].style_type], 1);
        DrawGraph(x, y + 10, g_mark_handle_buf[puppet.mark], 1);
    }
}

__declspec(naked)
static void _draw_box_icons()
{
    __asm
    {
        pushad
        pushfd
        push ebp
        mov ebp, esp
        sub esp, __LOCAL_SIZE
    }

    {
        // replacement code for the hook site
        if(*RVA(0x9537e9).ptr<uint8_t*>() == 1)
            SetDrawBlendMode(0x11, 0xff);

        // called after puppet sprites are drawn
        draw_box_icons();
    }

    __asm
    {
        mov esp, ebp
        pop ebp
        popfd
        popad
        jmp g_box_icon_return_addr
    }
}

// load extra resources from dat1/common/graphic
static void load_common_graphics()
{
    auto func = RVA(0x1afb00).ptr<VoidCall>();
    func();

    if(IniFile::global.get_bool("general", "puppet_box_info_icons"))
    {
        if(g_mark_handle_buf[0] == (uint32_t)-1)
            LoadDivGraph("dat\\gn_dat1\\common\\graphic\\mark.png", 6, 1, 6, 8, 8, (int*)g_mark_handle_buf.get());
    }

    if(IniFile::global.get_bool("general", "enable_battle_overlay"))
    {
        if(g_number_handle_buf[0] == (uint32_t)-1)
            LoadDivGraph("dat\\gn_dat1\\common\\graphic\\numbers.png", 11, 11, 1, 16, 16, (int*)g_number_handle_buf.get());
        if(g_small_number_handle_buf[0] == (uint32_t)-1)
            LoadDivGraph("dat\\gn_dat1\\common\\graphic\\small_numbers.png", 10, 10, 1, 16, 16, (int*)g_small_number_handle_buf.get());
        if(g_hazard_handle_buf[0] == (uint32_t)-1)
            LoadDivGraph("dat\\gn_dat1\\common\\graphic\\hazards.png", HAZARD_ICON_COUNT, HAZARD_ICON_WIDTH, HAZARD_ICON_HEIGHT, 16, 16, (int*)g_hazard_handle_buf.get());
        if(g_type_handle_buf[0] == (uint32_t)-1)
            LoadDivGraph("dat\\gn_dat1\\common\\graphic\\type_tabs.png", 17, 17, 1, 9, 5, (int*)g_type_handle_buf.get());
    }
}

static void patch_box_icons()
{
    for(auto i = 0; i < 6; ++i)
        g_mark_handle_buf[i] = (uint32_t)-1;
    g_box_icon_return_addr = RVA(0x15504b);

    // we hook this location because it's convenient to draw on top of
    // puppet sprites but under other UI elements
    patch_jump(RVA(0x155033), _draw_box_icons);
}

static void draw_weather_timer()
{
    // we don't want to reveal the actual turn limit so we'll count up
    // by detecting the change in duration to advance the turn counter
    auto tstate = get_terrain_state();
    if(g_last_weather_duration != tstate->weather_duration)
    {
        if(g_last_weather_duration > tstate->weather_duration)
            ++g_weather_counter;
        else
            g_weather_counter = 0;
        g_last_weather_duration = tstate->weather_duration;
    }
    if(g_last_terrain_duration != tstate->terrain_duration)
    {
        if(g_last_terrain_duration > tstate->terrain_duration)
            ++g_terrain_counter;
        else
            g_terrain_counter = 0;
        g_last_terrain_duration = tstate->terrain_duration;
    }

    auto weather_alpha = *RVA(0x93c830).ptr<uint32_t*>();
    auto terrain_alpha = *RVA(0x93c834).ptr<uint32_t*>();
    if((tstate->weather_type == WEATHER_NONE) && (tstate->weather_icon == 0) && (weather_alpha == 0))
        g_weather_counter = 0;
    if((tstate->terrain_type == TERRAIN_NONE) && (tstate->terrain_icon == 0) && (terrain_alpha == 0))
        g_terrain_counter = 0;

    auto weather_turns = g_weather_counter;
    auto terrain_turns = g_terrain_counter;
    if((weather_turns > 9) || (tstate->weather_duration > 9))
        weather_turns = 10;
    if((terrain_turns > 9) || (tstate->terrain_duration > 9))
        terrain_turns = 10;

    constexpr auto weather_x = ((960 / 2) - 74) + 16;
    constexpr auto terrain_x = ((960 / 2) + 74) - 16;
    constexpr auto weather_y = 84 + 4;
    constexpr auto terrain_y = 84 - 4;

    // steady
    if(tstate->weather_icon > 0)
        DrawRotaGraph(weather_x, weather_y, 2.0, 0.0, g_number_handle_buf[weather_turns], 1);

    // fade in/out
    if(weather_alpha > 0)
    {
        SetDrawBlendMode(0x11, weather_alpha);
        DrawRotaGraph(weather_x, weather_y, 2.0, 0.0, g_number_handle_buf[weather_turns], 1);
        SetDrawBlendMode(0x11, 0xff);
    }

    // steady
    if(tstate->terrain_icon > 0)
        DrawRotaGraph(terrain_x, terrain_y, 2.0, 0.0, g_number_handle_buf[terrain_turns], 1);

    // fade in/out
    if(terrain_alpha > 0)
    {
        SetDrawBlendMode(0x11, terrain_alpha);
        DrawRotaGraph(terrain_x, terrain_y, 2.0, 0.0, g_number_handle_buf[terrain_turns], 1);
        SetDrawBlendMode(0x11, 0xff);
    }
}

// find the puppet that hobgoblin would mimic
static PartyPuppet *get_hobgoblin_puppet(int player)
{
    auto data = &get_battle_data()[player];

    for(auto i = 0; i < 6; ++i)
    {
        auto ptr = &data->puppets[5 - i];
        auto puppet = decrypt_puppet(ptr);
        if((puppet.puppet_id != 0) && (puppet.hp > 0))
            return ptr;
    }

    return &data->puppets[0];
}

static void draw_type_tabs()
{
    auto state = get_battle_state(0);
    auto otherstate = get_battle_state(1);

    constexpr auto width = 18;
    constexpr auto height = 10;

    // local player
    if((state->field_0x9d != 0) && (state->field_0xb1 == 0) && (state->active_puppet != nullptr))
    {
        PartyPuppet puppet;
        if(state->hobgoblin_is_active)
            puppet = decrypt_puppet(get_hobgoblin_puppet(0));
        else
            puppet = decrypt_puppet(state->active_puppet);
        const auto& data = get_puppet_data()[puppet.puppet_id].styles[puppet.style_index];
        if((data.element1 > 0) && (data.element1 < ELEMENT_MAX))
            DrawExtendGraph(192, 60 - height, 192 + width, 60, g_type_handle_buf[data.element1 - 1], 1);
        if((data.element2 > 0) && (data.element2 < ELEMENT_MAX))
            DrawExtendGraph(192 + width + 2, 60 - height, (192 + width + 2) + width, 60, g_type_handle_buf[data.element2 - 1], 1);
    }

    // opponent
    if((otherstate->field_0x9d != 0) && (otherstate->field_0xb1 == 0) && (otherstate->active_puppet != nullptr))
    {
        PartyPuppet puppet;
        if(otherstate->hobgoblin_is_active)
            puppet = decrypt_puppet(get_hobgoblin_puppet(1));
        else
            puppet = decrypt_puppet(otherstate->active_puppet);
        const auto& data = get_puppet_data()[puppet.puppet_id].styles[puppet.style_index];
        if((data.element1 > 0) && (data.element1 < ELEMENT_MAX))
            DrawExtendGraph(746, 60 - height, 746 + width, 60, g_type_handle_buf[data.element1 - 1], 1);
        if((data.element2 > 0) && (data.element2 < ELEMENT_MAX))
            DrawExtendGraph(746 + width + 2, 60 - height, (746 + width + 2) + width, 60, g_type_handle_buf[data.element2 - 1], 1);
    }
}

static void draw_hazard_icons()
{
    constexpr auto width = 32;
    constexpr auto height = 32;

    constexpr auto base_y = (720 / 2) - (4 * height);

    auto state = get_battle_state(0);
    auto mines = std::clamp(state->num_mine_trap, 0, 3);
    auto pois = std::clamp(state->num_poison_trap, 0, 2);
    auto fb_turns = (g_field_barrier_turns[0] > 5u && state->field_barrier_turns > 3) ? state->field_barrier_turns - 3u : state->field_barrier_turns;
    auto fp_turns = (g_field_protect_turns[0] > 5u && state->field_protect_turns > 3) ? state->field_protect_turns - 3u : state->field_protect_turns;

    // local player
    if(mines > 0)
        DrawExtendGraph(0, base_y + (height * 0), 0 + width, base_y + (height * 0) + height, g_hazard_handle_buf[mines - 1], 1);
    if(state->stealth_trap)
        DrawExtendGraph(0, base_y + (height * 1), 0 + width, base_y + (height * 1) + height, g_hazard_handle_buf[3], 1);
    if(pois > 0)
        DrawExtendGraph(0, base_y + (height * 2), 0 + width, base_y + (height * 2) + height, g_hazard_handle_buf[6 + (pois - 1)], 1);
    if(state->bind_trap)
        DrawExtendGraph(0, base_y + (height * 3), 0 + width, base_y + (height * 3) + height, g_hazard_handle_buf[9], 1);
    if(state->field_barrier_turns > 0)
    {
        DrawExtendGraph(0, base_y + (height * 4), 0 + width, base_y + (height * 4) + height, g_hazard_handle_buf[12], 1);
        DrawExtendGraph(0, base_y + (height * 4), 0 + width, base_y + (height * 4) + height, g_small_number_handle_buf[std::clamp(fb_turns, 0u, 9u)], 1);
    }
    if(state->field_protect_turns > 0)
    {
        DrawExtendGraph(0, base_y + (height * 5), 0 + width, base_y + (height * 5) + height, g_hazard_handle_buf[15], 1);
        DrawExtendGraph(0, base_y + (height * 5), 0 + width, base_y + (height * 5) + height, g_small_number_handle_buf[std::clamp(fp_turns, 0u, 9u)], 1);
    }
    if(state->wind_gods_grace_turns > 0)
    {
        DrawExtendGraph(0, base_y + (height * 6), 0 + width, base_y + (height * 6) + height, g_hazard_handle_buf[18], 1);
        DrawExtendGraph(0, base_y + (height * 6), 0 + width, base_y + (height * 6) + height, g_small_number_handle_buf[std::clamp((unsigned int)state->wind_gods_grace_turns, 0u, 9u)], 1);
    }
    if(state->lucky_rainbow_turns > 0)
    {
        DrawExtendGraph(0, base_y + (height * 7), 0 + width, base_y + (height * 7) + height, g_hazard_handle_buf[21], 1);
        DrawExtendGraph(0, base_y + (height * 7), 0 + width, base_y + (height * 7) + height, g_small_number_handle_buf[std::clamp((unsigned int)state->lucky_rainbow_turns, 0u, 9u)], 1);
    }
    if(state->veil_of_water_turns > 0)
    {
        DrawExtendGraph(0, base_y + (height * 8), 0 + width, base_y + (height * 8) + height, g_hazard_handle_buf[24], 1);
        DrawExtendGraph(0, base_y + (height * 8), 0 + width, base_y + (height * 8) + height, g_small_number_handle_buf[std::clamp((unsigned int)state->veil_of_water_turns, 0u, 9u)], 1);
    }

    state = get_battle_state(1);
    mines = std::clamp(state->num_mine_trap, 0, 3);
    pois = std::clamp(state->num_poison_trap, 0, 2);
    fb_turns = (g_field_barrier_turns[1] > 5u && state->field_barrier_turns > 3) ? state->field_barrier_turns - 3u : state->field_barrier_turns;
    fp_turns = (g_field_protect_turns[1] > 5u && state->field_protect_turns > 3) ? state->field_protect_turns - 3u : state->field_protect_turns;

    // opponent
    constexpr auto base_x = 960 - width;
    if(mines > 0)
        DrawExtendGraph(base_x, base_y + (height * 0), base_x + width, base_y + (height * 0) + height, g_hazard_handle_buf[mines - 1], 1);
    if(state->stealth_trap)
        DrawExtendGraph(base_x, base_y + (height * 1), base_x + width, base_y + (height * 1) + height, g_hazard_handle_buf[3], 1);
    if(pois > 0)
        DrawExtendGraph(base_x, base_y + (height * 2), base_x + width, base_y + (height * 2) + height, g_hazard_handle_buf[6 + (pois - 1)], 1);
    if(state->bind_trap)
        DrawExtendGraph(base_x, base_y + (height * 3), base_x + width, base_y + (height * 3) + height, g_hazard_handle_buf[9], 1);
    if(state->field_barrier_turns > 0)
    {
        DrawExtendGraph(0, base_y + (height * 4), 0 + width, base_y + (height * 4) + height, g_hazard_handle_buf[12], 1);
        DrawExtendGraph(0, base_y + (height * 4), 0 + width, base_y + (height * 4) + height, g_small_number_handle_buf[std::clamp(fb_turns, 0u, 9u)], 1);
    }
    if(state->field_protect_turns > 0)
    {
        DrawExtendGraph(0, base_y + (height * 5), 0 + width, base_y + (height * 5) + height, g_hazard_handle_buf[15], 1);
        DrawExtendGraph(0, base_y + (height * 5), 0 + width, base_y + (height * 5) + height, g_small_number_handle_buf[std::clamp(fp_turns, 0u, 9u)], 1);
    }
    if(state->wind_gods_grace_turns > 0)
    {
        DrawExtendGraph(0, base_y + (height * 6), 0 + width, base_y + (height * 6) + height, g_hazard_handle_buf[18], 1);
        DrawExtendGraph(0, base_y + (height * 6), 0 + width, base_y + (height * 6) + height, g_small_number_handle_buf[std::clamp((unsigned int)state->wind_gods_grace_turns, 0u, 9u)], 1);
    }
    if(state->lucky_rainbow_turns > 0)
    {
        DrawExtendGraph(0, base_y + (height * 7), 0 + width, base_y + (height * 7) + height, g_hazard_handle_buf[21], 1);
        DrawExtendGraph(0, base_y + (height * 7), 0 + width, base_y + (height * 7) + height, g_small_number_handle_buf[std::clamp((unsigned int)state->lucky_rainbow_turns, 0u, 9u)], 1);
    }
    if(state->veil_of_water_turns > 0)
    {
        DrawExtendGraph(0, base_y + (height * 8), 0 + width, base_y + (height * 8) + height, g_hazard_handle_buf[24], 1);
        DrawExtendGraph(0, base_y + (height * 8), 0 + width, base_y + (height * 8) + height, g_small_number_handle_buf[std::clamp((unsigned int)state->veil_of_water_turns, 0u, 9u)], 1);
    }
}

// the most fucked up printf you'll ever see
static int draw_uint(unsigned int val, int x, int y, int width, int height)
{
    static_assert(sizeof(val) == 4);
    uint8_t digits[10]; // 32-bit int cannot exceed 10 decimal digits
    int count = 0;
    do
    {
        auto rem = val % 10u;
        val /= 10u;
        digits[count] = (uint8_t)rem;
        ++count;
    } while(val > 0);

    for(auto i = count; i > 0; --i)
    {
        DrawExtendGraph(x, y, x + width, y + height, g_number_handle_buf[digits[i - 1]], 1);
        x += width;
    }

    return count;
}

static void draw_debug_overlay()
{
    constexpr auto width = 12;
    constexpr auto height = 12;

    auto state = get_battle_state(0);
    if(state->active_puppet != nullptr)
    {
        auto puppet = decrypt_puppet(state->active_puppet);
        auto data = get_puppet_data()[puppet.puppet_id].styles[puppet.style_index];
        auto x = 4;
        for(auto i = 0; i < 6; ++i)
        {
            auto num_digits = draw_uint(data.base_stats[i], x, 86, width, height);
            x += (num_digits * width) + 4;
        }
    }

    state = get_battle_state(1);
    if(state->active_puppet != nullptr)
    {
        auto puppet = decrypt_puppet(state->active_puppet);
        auto data = get_puppet_data()[puppet.puppet_id].styles[puppet.style_index];
        auto x = 570;
        for(auto i = 0; i < 6; ++i)
        {
            auto num_digits = draw_uint(data.base_stats[i], x, 86, width, height);
            x += (num_digits * width) + 4;
        }
    }
}

static void draw_battle_overlay()
{
    // call original first
    auto func = RVA(0x20720).ptr<VoidCall>();
    func();

    draw_type_tabs();
    draw_hazard_icons();
    draw_weather_timer();
    if(g_debug_overlay)
        draw_debug_overlay();
}

int __cdecl field_barrier_hook(int player, int effect_chance)
{
    auto func = RVA(0x1a8b60).ptr<SkillCall>();
    auto retval = func(player, effect_chance);

    g_field_barrier_turns[player] = get_battle_state(player)->field_barrier_turns;

    return retval;
}

int __cdecl field_protect_hook(int player, int effect_chance)
{
    auto func = RVA(0x1a8910).ptr<SkillCall>();
    auto retval = func(player, effect_chance);

    g_field_protect_turns[player] = get_battle_state(player)->field_protect_turns;

    return retval;
}

static void patch_battle_overlay()
{
    for(auto i = 0; i < 11; ++i)
        g_number_handle_buf[i] = (uint32_t)-1;
    for(auto i = 0; i < 10; ++i)
        g_small_number_handle_buf[i] = (uint32_t)-1;
    for(auto i = 0; i < HAZARD_ICON_COUNT; ++i)
        g_hazard_handle_buf[i] = (uint32_t)-1;
    for(auto i = 0; i < 17; ++i)
        g_type_handle_buf[i] = (uint32_t)-1;

    // patch in hooks for field protect/barrier so we know
    // how many turns were originally set
    register_custom_skill(90, field_protect_hook);
    register_custom_skill(91, field_barrier_hook);

    patch_call(RVA(0x255d5), draw_battle_overlay);
}

// initialization
void init_misc_hacks()
{
    // patch in hooks for basic stuff
    scan_and_replace_call(RVA(0x2dc60), &_do_dmg_calc);
    scan_and_replace_call(RVA(0x28850), &do_adjusted_skill_data);
    scan_and_replace_call(RVA(0x33470), &item_dispatch);
    scan_and_replace_call(RVA(0x45f00), &_skill_dispatch);
    scan_and_replace_call(RVA(0xa850), &do_battle_stats);
    scan_and_replace_call(RVA(0x202f0), &do_weather_msg);
    scan_and_replace_call(RVA(0x20590), &do_terrain_msg);
    scan_and_replace_call(RVA(0x1bf70), &do_battlestate_reset);

    // hook the main battle mechanic "state machine"
    //patch_jump(RVA(0x293e7), &do_battle_state);

    // hook into event table
    patch_event_table();

    // hook into main loop
    patch_main_loop();

    // hook background rendering
    patch_backgrounds();

    // too early to load sprites directly, so we will hook
    // the function that loads dat1 sprites
    patch_call(RVA(0x1af872), load_common_graphics);

    // bgm limit
    if(IniFile::global.get_bool("general", "extra_music_hack"))
        patch_bgm_limit();

    // unused stuff you can enable if you like
    //patch_call(RVA(0x1c714), &do_exp_yield);

    if(IniFile::global.get_bool("general", "override_bag_sort", true))
        scan_and_replace_call(RVA(0x17c540), &do_bag_sort);

    if(IniFile::global.get_bool("general", "disable_update_check"))
        patch_call(RVA(0x17e409), &get_newest_version);

    if(IniFile::global.get_bool("general", "extend_dollicon_limit"))
        patch_dollicon();

    if(IniFile::global.get_bool("general", "puppet_box_info_icons"))
        patch_box_icons();

    if(IniFile::global.get_bool("general", "enable_battle_overlay"))
        patch_battle_overlay();

    g_debug_overlay = IniFile::global.get_bool("general", "enable_debug_overlay");

    // change alt-color text in the costume shop
    if(tpdp_eng_translation())
        patch_memory(RVA(0x4335e8), "Alternate", 10);
    else
        patch_memory(RVA(0x4335e8), "スペシャル", 11);

    // patch matchmaking and website addresses
    g_website_url = IniFile::global["network"]["website_addr"];
    if(g_website_url.empty())
        g_website_url = "www.gensou-suwako.com";
    std::string ipstr = IniFile::global["network"]["server_addr"];
    if(ipstr.empty())
        ipstr = "160.16.225.5";
    auto ip = hostname_to_ipv4(ipstr);
    auto port = IniFile::global.get_uint("network", "server_port", 16390);
    patch_network_addrs(ip, port);

    // miracle reprisal
    g_patch_miracle = IniFile::global.get_bool("skills", "patch_miracle");
    if(g_patch_miracle)
        patch_miracle();

    // macroburst
    g_macroburst_acc_boost = IniFile::global.get_bool("skills", "macroburst_acc_boost");

    // catch rate
    auto lvl_factor = IniFile::global.get_uint("general", "catch_rate_lvl_factor");
    if(lvl_factor != ID_NONE)
        patch_catch_rate((uint8_t)lvl_factor);

    // choice abilities
    patch_call(RVA(0x0b349), &do_choice_check);
    patch_call(RVA(0x183db), &do_choice_check);
    patch_call(RVA(0x18729), &do_choice_check);
    patch_call(RVA(0x2cb3e), &do_choice_check);
    patch_call(RVA(0x1d965), &_do_choice_msg);
    g_sprintf_addr = RVA(0x2f0c26);

    // config item IDs
    g_id_boots = IniFile::global.get_uint("items", "id_boots");
    g_id_wyrmprint = IniFile::global.get_uint("items", "id_wyrmprint");
    g_id_drum = IniFile::global.get_uint("items", "id_drum");
    g_id_genbu_seed = IniFile::global.get_uint("items", "id_genbu_seed");
    g_id_kohryu_seed = IniFile::global.get_uint("items", "id_kohryu_seed");
    g_id_seiryu_seed = IniFile::global.get_uint("items", "id_seiryu_seed");
    g_id_byakko_seed = IniFile::global.get_uint("items", "id_byakko_seed");
    g_id_suzaku_seed = IniFile::global.get_uint("items", "id_suzaku_seed");
    g_id_giant_bit = IniFile::global.get_uint("items", "id_giant_bit");
    g_id_resheal = IniFile::global.get_uint("items", "id_resheal");
    g_id_prismatic = IniFile::global.get_uint("items", "id_prismatic"); //NEW
    g_id_mending = IniFile::global.get_uint("items", "id_mending"); //NEW

    // config item mods
    g_mod_wyrmprint_high = IniFile::global.get_double("items", "mod_wyrmprint_high", g_mod_wyrmprint_high);
    g_mod_wyrmprint_low = IniFile::global.get_double("items", "mod_wyrmprint_low", g_mod_wyrmprint_low);
    g_mod_drum = IniFile::global.get_double("items", "mod_drum", g_mod_drum);
    g_mod_genbu_seed = IniFile::global.get_double("items", "mod_genbu_seed", g_mod_genbu_seed);
    g_mod_kohryu_seed = IniFile::global.get_double("items", "mod_kohryu_seed", g_mod_kohryu_seed);
    g_mod_seiryu_seed = IniFile::global.get_double("items", "mod_seiryu_seed", g_mod_seiryu_seed);
    g_mod_suzaku_seed = IniFile::global.get_double("items", "mod_suzaku_seed", g_mod_suzaku_seed);
    g_mod_resheal = IniFile::global.get_double("items", "mod_resheal", g_mod_resheal);
    g_power_giant_bit = IniFile::global.get_uint("items", "giant_bit_power", g_power_giant_bit);
    g_mod_prismatic = IniFile::global.get_double("items", "mod_prismatic", g_mod_prismatic); //NEW
    g_mod_mending = IniFile::global.get_double("items", "mod_mending", g_mod_mending); //NEW


    // config ability IDs
    g_id_stacking = IniFile::global.get_uint("abilities", "id_stacking");
    g_id_light_wings = IniFile::global.get_uint("abilities", "id_light_wings");
    g_id_water_wings = IniFile::global.get_uint("abilities", "id_water_wings");
    g_id_astronomy = IniFile::global.get_uint("abilities", "id_bu_abl");
    g_id_empowered = IniFile::global.get_uint("abilities", "id_en_abl");
    g_id_drain_abl = IniFile::global.get_uint("abilities", "id_drain_abl");
    g_id_calm_traveler = IniFile::global.get_uint("abilities", "id_calm_traveler");
    g_id_form_change = IniFile::global.get_uint("abilities", "id_form_change");
    g_id_form_target = IniFile::global.get_uint("abilities", "id_form_target");
    g_id_form_target_style = IniFile::global.get_uint("abilities", "id_form_target_style");
    g_id_form_base = IniFile::global.get_uint("abilities", "id_form_base");
    g_id_form_base_style = IniFile::global.get_uint("abilities", "id_form_base_style");
    g_id_morale = IniFile::global.get_uint("abilities", "id_morale");
    g_id_curse = IniFile::global.get_uint("abilities", "id_curse");
    g_id_merciless = IniFile::global.get_uint("abilities", "id_merciless");
    g_id_choice_focus = IniFile::global.get_uint("abilities", "id_choice_focus");
    g_id_choice_spread = IniFile::global.get_uint("abilities", "id_choice_spread");
    g_id_imposter = IniFile::global.get_uint("abilities", "id_imposter");
    g_id_anti_status = IniFile::global.get_uint("abilities", "id_anti_status");
    g_id_magic = IniFile::global.get_uint("abilities", "id_magic");
    g_id_warmup = IniFile::global.get_uint("abilities", "id_warmup");
    g_id_feast = IniFile::global.get_uint("abilities", "id_feast");
    g_id_sand_castle = IniFile::global.get_uint("abilities", "id_sand_castle");
    g_id_possess = IniFile::global.get_uint("abilities", "id_possess");
    g_id_possess_target = IniFile::global.get_uint("abilities", "id_possess_target");
    g_id_possess_target_style = IniFile::global.get_uint("abilities", "id_possess_target_style");
    g_id_possess_base = IniFile::global.get_uint("abilities", "id_possess_base");
    g_id_possess_base_style = IniFile::global.get_uint("abilities", "id_possess_base_style");
    g_id_gradate = IniFile::global.get_uint("abilities", "id_gradate");


    // config ability mods
    g_mod_class_abl = IniFile::global.get_double("abilities", "mod_class_abl", g_mod_class_abl);
    g_mod_drain_heal = IniFile::global.get_double("abilities", "mod_drain_heal", g_mod_drain_heal);
    g_mod_drain_def = IniFile::global.get_double("abilities", "mod_drain_def", g_mod_drain_def);
    g_mod_calm_traveler = IniFile::global.get_double("abilities", "mod_calm_traveler", g_mod_calm_traveler);
    g_mod_merciless = IniFile::global.get_double("abilities", "mod_merciless", g_mod_merciless);
    g_mod_choice = IniFile::global.get_double("abilities", "mod_choice", g_mod_choice);
    g_mod_magic = IniFile::global.get_double("abilities", "mod_magic", g_mod_magic);
    //g_mod_feast = IniFile::global.get_double("abilities", "mod_feast", g_mod_feast);
    g_mod_sand_castle = IniFile::global.get_double("abilities", "mod_sand_castle", g_mod_sand_castle);

    // config skills
    g_id_blitzkrieg = IniFile::global.get_uint("skills", "effect_id_blitzkrieg");
    g_mod_blitzkrieg = IniFile::global.get_double("skills", "mod_blitzkrieg", g_mod_blitzkrieg);
    g_id_future_skill = IniFile::global.get_uint("skills", "skill_id_future");
    g_id_arrow = IniFile::global.get_uint("skills", "effect_id_arrow"); 
    g_mod_arrow = IniFile::global.get_double("skills", "mod_arrow", g_mod_arrow); 
    g_id_eclipse = IniFile::global.get_uint("skills", "effect_id_eclipse"); 
    g_mod_eclipse = IniFile::global.get_double("skills", "mod_eclipse", g_mod_eclipse); 

    if(g_id_boots != ID_NONE)
        patch_boots();

    if(g_id_byakko_seed != ID_NONE)
        patch_byakko_seed();

    auto smile_chance = IniFile::global.get_uint("abilities", "strong_smile_chance");
    auto dark_chance = IniFile::global.get_uint("abilities", "dark_force_chance");

    // strong smile
    if(smile_chance != ID_NONE)
    {
        auto chance = (uint8_t)smile_chance;
        patch_memory(RVA(0x5d2e + 6u), &chance, 1);
    }

    // dark force
    if(dark_chance != ID_NONE)
    {
        auto chance = (uint8_t)dark_chance;
        patch_memory(RVA(0x5ce7 + 6u), &chance, 1);
    }
}

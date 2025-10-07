#include "hook_tpdp.h"
#include "misc_hacks.h"
#include "custom_skills.h"
#include "tpdp_data.h"
#include "../hook.h"
#include"../log.h"
#include "../ini_parser.h"
#include "../typedefs.h"
#include "graphics.h"
#include "audio.h"
#include <vector>
#include <utility>
#include <algorithm>
#include <gamedata.h>
#include <ShlObj.h>
#include <cmath>
#include <numbers>

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
static auto g_id_sting = ID_NONE;
static auto g_id_refresh = ID_NONE; //ID 878
//static auto g_id_solar = ID_NONE;
static auto g_id_future = ID_NONE; //ID 876, but going unused
static auto g_id_fading = ID_NONE; //ID 853
static auto g_id_tabula = ID_NONE; //ID 882
static auto g_id_steps = ID_NONE; //ID 880
static auto g_id_steps2 = ID_NONE; //ID 881
static auto g_id_zephyr = ID_NONE; //ID 879
static auto g_id_spiritdance = ID_NONE; //ID 887
static auto g_id_hearth = ID_NONE; //ID 888
static auto g_id_verdant_border = ID_NONE;

// animations
static int g_test_anim_handle = 0;
static unsigned int g_test_anim_frames = 0;

int g_future_turns[2]{};

typedef std::pair<Status, Status> StatusPair;
static std::vector<StatusPair> g_calamity_statuses;

int __cdecl season_activator(int /*player*/)
{
    skill_succeeded() = 1;
    return 1;
}

int __cdecl verdant_activator(int player)
{
    auto state = get_battle_state(player);
    auto puppet = decrypt_puppet(state->active_puppet);
    for (auto i : puppet.status_effects)
        if ((i == STATUS_WEAK) || (i == STATUS_HVYWEAK))
        {
            skill_succeeded() = 0;
            return 0;
        }
    skill_succeeded() = 1;
    return 1;
}

int __cdecl solar_activator(int player)
{
    if(get_battle_state(player)->field_0xdc != 0)
        return RVA(0x1a2ee0).ptr<int(__cdecl*)(int)>()(player);
    skill_succeeded() = 1;
    return 1;
}

int __cdecl future_activator(int player)
{
    if(g_future_turns[player] == 0)
        return RVA(0x1a2ee0).ptr<int(__cdecl*)(int)>()(player);
    skill_succeeded() = 0;
    return 0;
}

int __cdecl fading_activator(int player)
{
    auto data = get_battle_data();
    *(RVA(0x941fa8).ptr<int*>()) = 0;

    if(data[player == 0].state.field_0x16e == 0)
    {
        skill_succeeded() = 1;
        return 1;
    }

    skill_succeeded() = 0;
    return 0;
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

int __cdecl skill_verdant_border(int player, int /*effect_chance*/)
{

    static int _frames = 0;
    auto state = get_battle_state(player);
    auto puppet = decrypt_puppet(state->active_puppet);
    auto& skill_state = get_skill_state();
    auto max_hp = calculate_stat(STAT_HP, &puppet);
    switch (skill_state)
    {
    case 0:
        _frames = 0;
        if (!skill_succeeded())
        {
            skill_state = 3;
            return 0;
        }
        g_wish_state[player].heal = max_hp * 0.25;
        g_wish_state[player].turns = 2;
        skill_state = 1;
        return 1;

    case 1:
    {
        constexpr auto enheal = " has deployed\\nits border!";
        constexpr auto jpheal = "は結界を展開した！";
        auto name = std::string(state->active_nickname);
        bool english = tpdp_eng_translation();
        auto enmsg = enheal;
        auto jpmsg = jpheal;
        if (player == 1)
            name = (english ? "Enemy " : "相手の　") + name;
        if (set_battle_text(name + (english ? enmsg : jpmsg)) != 1)
        {
            if (++_frames > get_game_fps())
            {
                _frames = 0;
                skill_state = 3;
                return 0;
            }
        }
        return 1;
    }

    default:
        _frames = 0;
        return 0;
        break;
    }
}

int __cdecl skill_steps(int player, int /*effect_chance*/)
{
    auto& skill_state = get_skill_state();
    switch (skill_state)
    {
    case 0:
        if (!skill_succeeded())
        {
            skill_state = 3;
            return 0;
        }
        reset_stat_mod();
        skill_state = 1;
        return 1;

    case 1:
        if (apply_stat_mod(STAT_SPATK, +1, player) == 0)
        {
            skill_state = 2;
            reset_stat_mod();
        }
        return 1;

    case 2:
        if (apply_stat_mod(STAT_FODEF, +1, player) == 0)
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

int __cdecl skill_steps2(int player, int /*effect_chance*/)
{
    auto& skill_state = get_skill_state();
    switch (skill_state)
    {
    case 0:
        if (!skill_succeeded())
        {
            skill_state = 3;
            return 0;
        }
        reset_stat_mod();
        skill_state = 1;
        return 1;

    case 1:
        if (apply_stat_mod(STAT_FOATK, +1, player) == 0)
        {
            skill_state = 2;
            reset_stat_mod();
        }
        return 1;

    case 2:
        if (apply_stat_mod(STAT_SPDEF, +1, player) == 0)
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



int __cdecl skill_sting(int player, int /*effect_chance*/)
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
        reset_status_anim();
        skill_state = 1;
        return 1;

    case 1:
        if(apply_status_effect(!player, STATUS_POIS, STATUS_PARA) == 0)
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

int __cdecl skill_tabula(int player, int /*effect_chance*/)
{
    auto& skill_state = get_skill_state();
    auto otherstate = get_battle_state(!player);
    auto otherpuppet = decrypt_puppet(otherstate->active_puppet);
    static int _frames = 0;
    switch(skill_state)
    {
    case 0:
        if (!skill_succeeded())
        {
            skill_state = 4;
            return 0;
            break;
        }
    case 1:
        if (skill_succeeded() && (otherstate->active_type1 == ELEMENT_VOID) && (otherstate->active_type2 == ELEMENT_NONE))
        {
                skill_state = 4;
                return 0;
                break;
        }

    case 2:
        if (otherpuppet.hp > 0)
        {
            {
                otherstate->active_type1 = ELEMENT_VOID;
                otherstate->active_type2 = ELEMENT_NONE;
            }
            skill_state = 3;
            return 1;
        }
        else
            skill_state = 4;
        return 0;
        break;

    case 3:
    {
        auto name = std::string(otherstate->active_nickname);
        bool english = tpdp_eng_translation();
        if (player == 0)
            name = (english ? "Enemy " : "相手の　") + name;
        if (set_battle_text(name + (english ? " has become a Void type!" : " は 自身の状態異常と！")) != 1)
        {
            if (++_frames > get_game_fps())
            {
                skill_state = 4;
                _frames = 0;
                return 0;
            }
        }
    }
        return 1;
    }
    return 0;
}

int __cdecl skill_refresh(int player, int /*effect_chance*/)
{
    auto& skill_state = get_skill_state();
    auto state = get_battle_state(player);
    static int _frames = 0;
    switch(skill_state)
    {
    case 0:
        if(!skill_succeeded())
        {
            skill_state = 3;
            return 0;
        }
        reset_heal_anim(player);
        state->active_puppet->status_effects[0] = 0;
        state->active_puppet->status_effects[1] = 0;
        skill_state = 1;
        return 1;

    case 1:
        if(do_heal_anim() == 0)
        {
            skill_state = 2;
            clear_battle_text();
        }
        return 1;

    case 2:
    {
        auto name = std::string(state->active_nickname);
        bool english = tpdp_eng_translation();
        if(player != 0)
            name = (english ? "Enemy " : "相手の　") + name;
        auto msg = name + (english ? " cleared its status effects!" : " は 自身の状態異常と！");
        if(set_battle_text(msg) != 1)
        {
            if(++_frames > get_game_fps())
            {
                skill_state = 3;
                _frames = 0;
                return 0;
            }
        }
        return 1;
    }

    default:
        return 0;
        break;
    }
}

int __cdecl skill_spiritdance(int player, int /*effect_chance*/) //Victory Dance from Pokemon (Physical Quiver Dance)
{
    auto& skill_state = get_skill_state();
    switch (skill_state)
    {
    case 0:
        if (!skill_succeeded())
        {
            skill_state = 4;
            return 0;
        }
        reset_stat_mod();
        skill_state = 1;
        return 1;

    case 1:
        if (apply_stat_mod(STAT_FOATK, +1, player) == 0)
        {
            skill_state = 2;
            reset_stat_mod();
        }
        return 1;

    case 2:
        if (apply_stat_mod(STAT_FODEF, +1, player) == 0)
        {
            skill_state = 3;
            reset_stat_mod();
        }
        return 1;

    case 3:
        if (apply_stat_mod(STAT_SPEED, +1, player) == 0)
        {
            skill_state = 4;
            return 0;
        }
        return 1;

    default:
        return 0;
        break;
    }
}

int __cdecl skill_hearth(int player, int /*effect_chance*/) //V-Create
{
    auto& skill_state = get_skill_state();
    switch (skill_state)
    {
    case 0:
        if (!skill_succeeded())
        {
            skill_state = 4;
            return 0;
        }
        reset_stat_mod();
        skill_state = 1;
        return 1;

    case 1:
        if (apply_stat_mod(STAT_FODEF, -1, player) == 0)
        {
            skill_state = 2;
            reset_stat_mod();
        }
        return 1;

    case 2:
        if (apply_stat_mod(STAT_SPDEF, -1, player) == 0)
        {
            skill_state = 3;
            reset_stat_mod();
        }
        return 1;

    case 3:
        if (apply_stat_mod(STAT_SPEED, -1, player) == 0)
        {
            skill_state = 4;
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

#if 0
// solar beam
int __cdecl skill_solar(int player, int /*effect_chance*/)
{
    auto& skill_state = get_skill_state();
    auto state = get_battle_state(player);
    static int _frames = 0;
    switch(skill_state)
    {
    case 0:
        if(!skill_succeeded())
        {
            skill_state = 4;
            return 0;
        }
        if(state->field_0xdc > 0)
        {
            skill_state = 4;
            state->field_0xdc = 0;
            return 0;
        }
        _frames = 0;
        state->field_0x46 = RVA(0x1ba140).ptr<uint(__cdecl*)(uint, uint, uint)>()(0xff, 0xff, 0xff);
        skill_state = 1;
        return 1;

    case 1:
        if((state->field_0x4a += 24.0f) >= 255.0f)
        {
            skill_state = 2;
            state->field_0x4a = 255.0f;
        }
        return 1;

    case 2:
        if((state->field_0x4a -= 24.0f) <= 0.0f)
        {
            skill_state = 3;
            state->field_0x4a = 0.0f;
            clear_battle_text();
        }
        return 1;

    case 3:
    {
        auto name = std::string(state->active_nickname);
        //bool english = tpdp_eng_translation();
        auto msg = "The power of ligma wells up\\nwithin " + name + "!";
        if(set_battle_text(msg.c_str()) != 1)
        {
            if(++_frames > get_game_fps())
            {
                auto terrainstate = get_terrain_state();
                state->field_0xdc = (terrainstate->weather_type == WEATHER_CALM) ? 3 : 2;
                skill_state = 4;
                _frames = 0;
                return 0;
            }
        }
        return 1;
    }

    default:
        return 0;
        break;
    }
}
#endif

int __cdecl skill_future(int player, int /*effect_chance*/)
{
    auto& skill_state = get_skill_state();
    static int _frames = 0;
    switch(skill_state)
    {
    case 0:
        if(!skill_succeeded() || (g_future_turns[player] > 0))
        {
            skill_state = 2;
            return 1;
        }
        g_future_turns[player] = 3;
        clear_battle_text();
        skill_state = 1;
        return 1;

    case 1:
    {
        auto state = get_battle_state(player);
        auto name = std::string(state->active_nickname);
        bool english = tpdp_eng_translation();
        if(player != 0)
            name = (english ? "Enemy " : "相手の　") + name;
        auto msg = name + (english ? " set a bomb!" : " は 爆弾を 仕掛けた！");
        if(set_battle_text(msg) != 1)
        {
            if(++_frames > get_game_fps())
            {
                skill_state = 3;
                _frames = 0;
                return 0;
            }
        }
        return 1;
    }

    case 2:
    {
        bool english = tpdp_eng_translation();
        if(set_battle_text(english ? "But the skill failed." : "しかし スキルは 失敗した。") != 1)
        {
            if(++_frames > get_game_fps())
            {
                skill_state = 3;
                _frames = 0;
                return 0;
            }
        }
        return 1;
    }

    default:
        return 0;
        break;
    }
}

#if 0
int __cdecl skill_fading(int player, int effect_chance)
{
    auto changeling = RVA(0x1a5420).ptr<SkillCall>();
    auto otherdata = &get_battle_data()[!player];
    if((short)otherdata->puppets[otherdata->active_puppet_index].hp > 0)
        return changeling(player, effect_chance);
    auto data = &get_battle_data()[player];
    auto state = &data->state;
    PartyPuppet puppet;
    if(state->field_0x9e == 0)
        state->field_0x9e = 2;
    for(auto i = 0; i < 6; ++i)
    {
        if(i == data->active_puppet_index)
            continue;
        puppet = decrypt_puppet(&data->puppets[i]);
        if((puppet.puppet_id != 0) && ((short)puppet.hp > 0))
            goto do_thing;
    }
    return 0;

do_thing:
    auto func = RVA(0x11ed0).ptr<uint8_t*(*)()>();
    *(RVA(0xc59b2a).ptr<uint8_t*>()) = 0xff;
    auto puVar5 = func();
    puVar5[1] = 10;
    puVar5 = func();
    puVar5[2] = 4;
    puVar5 = func();
    puVar5[3] = 0xb;
    puVar5 = func();
    puVar5[4] = 4;
    puVar5 = func();
    puVar5[5] = 0xc;
    puVar5 = func();
    puVar5[6] = 0xd;
    puVar5 = func();
    puVar5[7] = 0xe;
    puVar5 = func();
    puVar5[8] = 5;
    RVA(0x287c0).ptr<void(*)()>()();
    *(RVA(0x941fa8).ptr<int*>()) = 0;
    return 0;
}
#endif

int __cdecl skill_zephyr(int player, int /*effect_chance*/)
{
    auto& skill_state = get_skill_state();
    auto state = get_battle_state(player);
    auto otherstate = get_battle_state(!player);
    static int _frames = 0;
    switch(skill_state)
    {
    case 0:
        if(!skill_succeeded())
        {
            skill_state = 3;
            return 0;
        }
        skill_state = 1;
        return 1;

    case 1:
        state->num_mine_trap = 0;
        state->num_poison_trap = 0;
        state->stealth_trap = false;
        state->bind_trap = false;
        state->trap_turns = 0;
        state->drain_seed = false;
        state->field_barrier_turns = 0;
        state->field_protect_turns = 0;
        // ---
        otherstate->num_mine_trap = 0;
        otherstate->num_poison_trap = 0;
        otherstate->stealth_trap = false;
        otherstate->bind_trap = false;
        otherstate->trap_turns = 0;
        otherstate->drain_seed = false;
        otherstate->field_barrier_turns = 0;
        otherstate->field_protect_turns = 0;
        clear_battle_text();
        skill_state = 2;
        return 1;

    case 2:
    {
        auto name = std::string(state->active_nickname);
        bool english = tpdp_eng_translation();
        if(player != 0)
            name = (english ? "Enemy " : "相手の　") + name;
        auto msg = name + (english ? " cleared the field of hazards!" : " cleared the field of hazards!");
        if(set_battle_text(msg) != 1)
        {
            if(++_frames > get_game_fps())
            {
                skill_state = 3;
                _frames = 0;
                return 0;
            }
        }
        return 1;
    }

    default:
        return 0;
        break;
    }
}

static void test_skill_loader()
{
    if(g_test_anim_handle == 0)
        g_test_anim_handle = LoadGraph("dat\\gn_dat1\\battle\\skillGrEffect\\mindcontrol.png");
}

static void test_skill_deleter()
{
    if(g_test_anim_handle != 0)
        DeleteSharingGraph(g_test_anim_handle);
    g_test_anim_handle = 0;
}

static int test_skill_tick([[maybe_unused]] int player)
{
    // animate for 1 second
    if(g_test_anim_frames++ >= get_game_fps())
    {
        g_test_anim_frames = 0;
        return 0;
    }
    return 1;
}

static void test_skill_anim([[maybe_unused]] int player)
{
    // rotate 180 degrees both directions while oscillating on both axes and scaling up and down
    double t = std::sin((g_test_anim_frames / (double)get_game_fps()) * (std::numbers::pi_v<double> * 2));
    double angle = t * std::numbers::pi_v<double>;
    double scale = 1.0 + (t * 0.25);
    float offset = t * 50.0f;
    DrawRotaGraphF((960 / 2.0f) + offset, (720 / 2.0f) + offset, scale, angle, g_test_anim_handle, 1, 0);

    // play a sound 10 frames into the animation using dxlib api directly.
    // volume will be inconsistent for sounds that have not been played through normal means already.
    if(g_test_anim_frames == 10)
        PlaySoundMem(get_sfx_handle(3), PlaybackType::BACKGROUND, 1);

    // play another sound using the game's internal queue.
    // this method should be preferred for sfx.
    if(g_test_anim_frames == 40)
        play_sfx(4);
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

    g_id_tabula = IniFile::global.get_uint("skills", "effect_id_tabula");
    if (g_id_tabula != ID_NONE)
    {
        init_new_skill(g_id_tabula);
        register_custom_skill(g_id_tabula, &skill_tabula);
    }

    g_id_sting = IniFile::global.get_uint("skills", "effect_id_sting");
    if(g_id_sting != ID_NONE)
    {
        init_new_skill(g_id_sting);
        register_custom_skill(g_id_sting, &skill_sting);
    }

    g_id_refresh = IniFile::global.get_uint("skills", "effect_id_refresh");
    if(g_id_refresh != ID_NONE)
    {
        init_new_skill(g_id_refresh);
        register_custom_skill(g_id_refresh, &skill_refresh);
    }

    g_id_steps = IniFile::global.get_uint("skills", "effect_id_steps");
    if (g_id_refresh != ID_NONE)
    {
        init_new_skill(g_id_steps);
        register_custom_skill(g_id_steps, &skill_steps);
    }

    g_id_steps2 = IniFile::global.get_uint("skills", "effect_id_steps2");
    if (g_id_refresh != ID_NONE)
    {
        init_new_skill(g_id_steps2);
        register_custom_skill(g_id_steps2, &skill_steps2);
    }

#if 0
    g_id_solar = IniFile::global.get_uint("skills", "effect_id_solar");
    if(g_id_solar != ID_NONE)
    {
        init_new_skill(g_id_solar);
        register_custom_skill(g_id_solar, &skill_solar);
        register_custom_skill_activator(g_id_solar, &solar_activator);
    }
#endif

    g_id_future = IniFile::global.get_uint("skills", "effect_id_future");
    if(g_id_future != ID_NONE)
    {
        init_new_skill(g_id_future);
        register_custom_skill(g_id_future, &skill_future);
        register_custom_skill_activator(g_id_future, &future_activator);
    }

    g_id_fading = IniFile::global.get_uint("skills", "effect_id_fading");
    if(g_id_fading != ID_NONE)
    {
        copy_skill_effect(60, g_id_fading);
        //register_custom_skill(g_id_fading, RVA(0x1a5420).ptr<SkillCall>());
        register_custom_skill_activator(g_id_fading, &fading_activator);
    }

    g_id_zephyr = IniFile::global.get_uint("skills", "effect_id_zephyr");
    if(g_id_zephyr != ID_NONE)
    {
        init_new_skill(g_id_zephyr);
        register_custom_skill(g_id_zephyr, &skill_zephyr);
    }

        g_id_spiritdance = IniFile::global.get_uint("skills", "effect_id_spiritdance");
    if (g_id_refresh != ID_NONE)
    {
        init_new_skill(g_id_spiritdance);
        register_custom_skill(g_id_spiritdance, &skill_spiritdance);
    }

    g_id_hearth = IniFile::global.get_uint("skills", "effect_id_hearth");
    if (g_id_refresh != ID_NONE)
    {
        init_new_skill(g_id_hearth);
        register_custom_skill(g_id_hearth, &skill_hearth);
    }

    g_id_verdant_border = IniFile::global.get_uint("skills", "effect_id_verdant_border");
    if (g_id_verdant_border != ID_NONE)
    {
        init_new_skill(g_id_verdant_border);
        register_custom_skill(g_id_verdant_border, &skill_verdant_border);
        register_custom_skill_activator(g_id_verdant_border, &verdant_activator);
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

    if(IniFile::global.get_bool("skills", "enable_example_animation"))
    {
        register_custom_skill_anim(56, test_skill_loader, test_skill_deleter, test_skill_anim, nullptr, test_skill_tick); // yin energy
        register_custom_skill_anim(58, test_skill_loader, test_skill_deleter, test_skill_anim, nullptr, test_skill_tick); // yang energy
    }
}


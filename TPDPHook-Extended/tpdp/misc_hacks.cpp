#include "misc_hacks.h"
#include "tpdp_data.h"
#include "../ini_parser.h"
#include "../hook.h"
#include "../log.h"
#include "hook_tpdp.h"
#include <cassert>
#include <algorithm>
#include <set>

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
static auto g_id_anti_status = ID_NONE;

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

// ability IDs
static auto g_id_stacking = ID_NONE;
static auto g_id_light_wings = ID_NONE;
static auto g_id_astronomy = ID_NONE;
static auto g_id_empowered = ID_NONE;
static auto g_id_drain_abl = ID_NONE;
static auto g_id_calm_traveler = ID_NONE;

// ability mods
static auto g_mod_class_abl = 0.1;
static auto g_mod_drain_heal = 1.0 / 8.0;
static auto g_mod_drain_def = 0.8;
static auto g_mod_calm_traveler = 2.0;

// skills
static auto g_id_blitzkrieg = ID_NONE;
static double g_mod_blitzkrieg = 2.0;

// NOTE: we technically should be doing some XSAVE and FXSAVEs in here
// but the game does not use SSE and our code does not use x87 (except floating point return values)
// so we can avoid clobbering eachother if we're careful

/*
struct alignas(16) FXSaveData
{
    uint8_t buf[512];
};
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

/*
// called when the battle state is reset
// before a new battle begins
void do_battlestate_reset()
{
    // call the original function first
    auto func = RVA(0x1bf70).ptr<VoidCall>();
    func();

    // reset any custom state here
}
*/

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
    else if((puppet.held_item == g_id_seiryu_seed) && (get_terrain_state()->terrain_type == TERRAIN_SEIRYU) && can_use_items(state))
    {
        dmg *= g_mod_seiryu_seed; // Yggdrasil Seed
    }

    if((otherpuppet.held_item == g_id_seiryu_seed) && (get_terrain_state()->terrain_type == TERRAIN_SEIRYU) && can_use_items(otherstate))
    {
        dmg *= g_mod_seiryu_seed; // also Yggdrasil Seed
    }

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
    case STAT_FODEF:
    case STAT_SPDEF:
        if((uint)state->active_ability == g_id_drain_abl) // Cursed Being def drop
            result *= g_mod_drain_def;
        break;

    case STAT_SPEED:
        if((puppet.held_item == g_id_kohryu_seed) && (terrain_state->terrain_type == TERRAIN_KOHRYU) && (state->active_ability != 245) && (state->active_ability != 390))
        {
            result *= g_mod_kohryu_seed; // Izanagi Object
        }

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

// override type/power/prio/etc changes
SkillData *__fastcall do_adjusted_skill_data(int player, ushort skill_id)
{
    auto data = get_adjusted_skill_data(player, skill_id);

    auto state = get_battle_state(player);
    auto otherstate = get_battle_state(!player);
    auto terrain_state = get_terrain_state();
    const auto& basedata = get_skill_data()[(skill_id != 0) ? skill_id : state->active_skill_id];
    if((data->effect_id == g_id_blitzkrieg) && ((state->turn_order == 0) || (otherstate->switched_puppet)))
        data->power = (byte)std::clamp((double)data->power * g_mod_blitzkrieg, 0.0, 255.0);

    if(((uint)state->active_ability == g_id_light_wings) && (basedata.element == ELEMENT_LIGHT))
    {
        if((data->priority < 6) && ((uint)otherstate->active_ability != 341)) // Grand Opening
            data->priority += 1;
    }
    else if(((uint)state->active_ability == g_id_astronomy) && (data->classification == SKILL_CLASS_BU))
    {
        auto power = (double)data->power;
        data->power = (byte)std::clamp(power + (power * g_mod_class_abl), 0.0, 255.0); // Astronomy
    }
    else if(((uint)state->active_ability == g_id_empowered) && (data->classification == SKILL_CLASS_EN))
    {
        auto power = (double)data->power;
        data->power = (byte)std::clamp(power + (power * g_mod_class_abl), 0.0, 255.0); // Empowered
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
        if(player == 0)
            name = "Enemy " + name;
        if(set_battle_text(name + " took damage from the\\nSpirit Torch!") != 1)
        {
            if(++_frames > 60)
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
        if(do_item(player)) // do our items
            return 1;
        _state = 1;
    }

    if(RVA(0x33470).ptr<int(__cdecl*)(int)>()(player) == 0) // do original items
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
        if(player != 0)
            name = "Enemy " + name;
        if(set_battle_text(name + " took damage from the\\nGiant Bit!") != 1)
        {
            if(++_frames > 60)
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
            bool do_heal = true;
            for(auto i : otherpuppet.status_effects)
                if((i == STATUS_WEAK) || (i == STATUS_HVYWEAK))
                    do_heal = false;
            if(do_heal)
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
        if(do_heal_anim() == 0)
            _state = 2;
        return 1;

    case 2:
    {
        if(state->num_attacks > 0)
        {
            _frames = 0;
            _state = 0;
            return 0;
        }
        auto name = std::string(otherstate->active_nickname);
        if(player == 0)
            name = "Enemy " + name;
        if(set_battle_text(name + " recovered HP with\\nAbsorber!") != 1)
        {
            if(++_frames > 60)
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
            if(++_frames > 60)
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
        if(has_held_item(otherstate, g_id_anti_status) && can_use_items(otherstate) && (skilldata.type == SKILL_TYPE_STATUS))
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
    //only the hold2 pocket, delegate rest to original sorter function
    auto func = RVA(0x17c540).ptr<void(__cdecl*)(unsigned int)>();
    if(pocket != 3)
        return func(pocket);

    // collect and sort unique IDs from pocket
    std::set<uint16_t> ids;
    auto pocketbuf = RVA(0xD7C147).ptr<uint16_t*>();
    for(auto i = 0; i < 0x80u; ++i)
        if(pocketbuf[i] != 0)
            ids.insert(pocketbuf[i]);

    // clear pocket
    memset(pocketbuf, 0, 0x80u * sizeof(uint16_t));

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

    // unused stuff you can enable if you like
    //scan_and_replace_call(RVA(0x1bf70), &do_battlestate_reset);
    //patch_call(RVA(0x1c714), &do_exp_yield);

    if(IniFile::global.get_bool("general", "override_bag_sort", true))
        scan_and_replace_call(RVA(0x17c540), &do_bag_sort);

    // change alt-color text in the costume shop
    if(tpdp_eng_translation())
        patch_memory(RVA(0x4335e8), "Alternate", 10);
    else
        patch_memory(RVA(0x4335e8), "\x83\x58\x83\x79\x83\x56\x83\x83\x83\x8B", 11);

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
    g_id_anti_status = IniFile::global.get_uint("items", "id_anti_status");

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

    // config ability IDs
    g_id_stacking = IniFile::global.get_uint("abilities", "id_stacking");
    g_id_light_wings = IniFile::global.get_uint("abilities", "id_light_wings");
    g_id_astronomy = IniFile::global.get_uint("abilities", "id_bu_abl");
    g_id_empowered = IniFile::global.get_uint("abilities", "id_en_abl");
    g_id_drain_abl = IniFile::global.get_uint("abilities", "id_drain_abl");
    g_id_calm_traveler = IniFile::global.get_uint("abilities", "id_calm_traveler");

    // config ability mods
    g_mod_class_abl = IniFile::global.get_double("abilities", "mod_class_abl", g_mod_class_abl);
    g_mod_drain_heal = IniFile::global.get_double("abilities", "mod_drain_heal", g_mod_drain_heal);
    g_mod_drain_def = IniFile::global.get_double("abilities", "mod_drain_def", g_mod_drain_def);
    g_mod_calm_traveler = IniFile::global.get_double("abilities", "mod_calm_traveler", g_mod_calm_traveler);

    // config skills
    g_id_blitzkrieg = IniFile::global.get_uint("skills", "effect_id_blitzkrieg");
    g_mod_blitzkrieg = IniFile::global.get_double("skills", "mod_blitzkrieg", g_mod_blitzkrieg);

    if(g_id_boots != ID_NONE)
        patch_boots();

    if(g_id_byakko_seed != ID_NONE)
        patch_byakko_seed();
}

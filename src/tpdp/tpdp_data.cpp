#include <cstring>
#include "tpdp_data.h"
#include "misc_hacks.h"
#include "../log.h"

static bool stat_mod_bool = false;

static void decrypt_puppet(void *src, const void *rand_data)
{
    uint8_t *buf = (uint8_t*)src;
    const uint8_t *randbuf = (const uint8_t*)rand_data;
    constexpr std::size_t len = 0x91;

    for(unsigned int i = 0; i < (len / 3); ++i)
    {
        int index = (i * 3) % len;
        uint32_t crypto = *(uint32_t*)(&randbuf[(i * 4) & 0x3fff]);

        /* need the higher half of this multiplication, right shifted 1 */
        uint32_t temp = (uint64_t(uint64_t(0xAAAAAAABu) * uint64_t(crypto)) >> 33);
        temp *= 3;

        if(crypto - temp == 0)
            buf[index] = ~buf[index];
        buf[index] -= uint8_t(crypto);
    }
}

static void encrypt_puppet(void *src, const void *rand_data)
{
    uint8_t *buf = (uint8_t*)src;
    const uint8_t *randbuf = (const uint8_t*)rand_data;
    constexpr std::size_t len = 0x91;

    for(unsigned int i = 0; i < (len / 3); ++i)
    {
        int index = (i * 3) % len;
        uint32_t crypto = *(uint32_t*)(&randbuf[(i * 4) & 0x3fff]);

        uint32_t temp = (uint64_t(uint64_t(0xAAAAAAABu) * uint64_t(crypto)) >> 33);
        temp *= 3;

        buf[index] += uint8_t(crypto);
        if(crypto - temp == 0)
            buf[index] = ~buf[index];
    }
}

PartyPuppet decrypt_puppet(const PartyPuppet *puppet)
{
    if(puppet == nullptr)
    {
        log_debug(L"Nullptr passed to decrypt_puppet.");
        return {};
    }
    PartyPuppet tmp;
    auto rand_data = RVA(0x8d40c0).ptr<void*>(); // efile.bin
    memcpy(&tmp, puppet, sizeof(PartyPuppet));
    decrypt_puppet(&tmp, rand_data);
    return tmp;
}

PartyPuppet encrypt_puppet(const PartyPuppet *puppet)
{
    if(puppet == nullptr)
    {
        log_debug(L"Nullptr passed to encrypt_puppet.");
        return {};
    }
    PartyPuppet tmp;
    auto rand_data = RVA(0x8d40c0).ptr<void*>(); // efile.bin
    memcpy(&tmp, puppet, sizeof(PartyPuppet));
    encrypt_puppet(&tmp, rand_data);
    return tmp;
}

void decrypt_puppet_nocopy(PartyPuppet *puppet)
{
    if(puppet == nullptr)
    {
        log_debug(L"Nullptr passed to decrypt_puppet_nocopy.");
        return;
    }
    decrypt_puppet(puppet, RVA(0x8d40c0));
}
void encrypt_puppet_nocopy(PartyPuppet *puppet)
{
    if(puppet == nullptr)
    {
        log_debug(L"Nullptr passed to encrypt_puppet_nocopy.");
        return;
    }
    encrypt_puppet(puppet, RVA(0x8d40c0));
}

int apply_status_effect(int player, Status status1, Status status2)
{
    auto do_status = (uintptr_t)RVA(0x27560);
    int tmp = 0;

    // fix-up for calling convention clobbered by
    // compiler optimizations (param passed in esi)
    __asm
    {
        push status1
        mov ecx, status2
        mov esi, player
        call do_status
        add esp, 4
        mov tmp, eax
    }

    return tmp;
}

unsigned int calculate_stat(unsigned int level, unsigned int stat_index, EncryptedPuppet *puppet, unsigned int alt_form)
{
    if(puppet == nullptr)
    {
        log_debug(L"Nullptr passed to calculate_stat1.");
        return 0;
    }

    //just delegate this to the in-game function for now
    auto func = (uintptr_t)RVA(0x22f0);
    int tmp = 0;

    __asm
    {
        push alt_form
        push stat_index
        push level
        mov esi, puppet
        call func
        mov tmp, eax
    }

    return tmp;
}
unsigned int calculate_stat(unsigned int stat_index, PartyPuppet *puppet, unsigned int alt_form)
{
    if(puppet == nullptr)
    {
        log_debug(L"Nullptr passed to calculate_stat2.");
        return 0;
    }

    return calculate_stat(puppet->level, stat_index, (EncryptedPuppet*)puppet, alt_form);
}

BattleData *get_battle_data()
{
    return RVA(0xc59b60).ptr<BattleData*>();
}

BattleState *get_battle_state(int player)
{
    auto data = get_battle_data();
    return &data[player].state;
}

int do_weather_msg()
{
    auto func = RVA(0x202f0).ptr<int(*)(void)>();
    auto result = func();
    if(result == 0)
        do_terrain_hook();
    return result;
}

void reset_weather_msg()
{
    *RVA(0x93C06C).ptr<uint32_t*>() = 0;
}

int do_terrain_msg()
{
    auto func = RVA(0x20590).ptr<int(*)(void)>();
    auto result = func();
    if(result == 0)
        do_terrain_hook();
    return result;
}

void reset_terrain_msg()
{
    *RVA(0x93c704).ptr<uint32_t*>() = 0;
}

int set_battle_text(const char* msg, bool unknown_arg, bool unknown_arg2)
{
    auto func = RVA(0x1b0ee0).ptr<int(__cdecl*)(const char*, bool, bool)>();
    return func(msg, unknown_arg, unknown_arg2);
}

void clear_battle_text()
{
    auto func = RVA(0x287c0).ptr<void*(*)(void)>();
    func();
}

bool is_game_60fps()
{
    return *RVA(0xc5960b).ptr<bool*>();
}
int get_game_fps()
{
    return is_game_60fps() ? 60 : 30;
}

TerrainState *get_terrain_state()
{
    auto func = RVA(0x9690).ptr<TerrainState*(*)(void)>();
    return func();
}

bool& skill_succeeded()
{
    return *RVA(0x93c897).ptr<bool*>();
}

uint32_t& get_skill_state()
{
    return *RVA(0x941fa4).ptr<uint32_t*>();
}

SkillData* get_skill_data()
{
    return RVA(0x8b64c0).ptr<SkillData*>();
}

PuppetData* get_puppet_data()
{
    return RVA(0x8f20e8).ptr<PuppetData*>();
}

void reset_status_anim()
{
    *RVA(0x93c890).ptr<uint32_t*>() = 0;
}

SkillCall get_default_activator()
{
    return RVA(0x1a2ee0).ptr<SkillCall>();
}

int apply_stat_mod(StatIndex stat_index, int stat_mod, bool param_3, bool param_4, bool param_5, bool *param_6, int player)
{
    void *func = RVA(0x26c60);
    int result;

    int p5 = param_5;
    int p4 = param_4;
    int p3 = param_3;

    __asm
    {
        push param_6
        push p5
        push p4
        push p3
        push stat_mod
        mov ecx, stat_index
        mov esi, player
        call func
        add esp, 0x14
        mov result, eax
    }

    return result;
}

int apply_stat_mod(StatIndex stat_index, int stat_mod, int player)
{
    return apply_stat_mod(stat_index, stat_mod, true, !stat_mod_bool, true, &stat_mod_bool, player);
}

int get_stat_mod(StatIndex index, BattleState *state)
{
    switch(index)
    {
    case STAT_FOATK:
    case STAT_FODEF:
    case STAT_SPATK:
    case STAT_SPDEF:
    case STAT_SPEED:
        return state->stat_modifiers[index];
    case STAT_ACC:
        return state->accuracy;
    case STAT_EVASION:
        return state->evasion;
    case STAT_CRIT_RATE:
        return state->crit_rate;
    default:
        return 0;
    }
}

void reset_stat_mod(void)
{
    *RVA(0x93c170).ptr<uint32_t*>() = 0;
    stat_mod_bool = false;
}

uint32_t& get_ability_state()
{
    return *RVA(0x93bb8c).ptr<uint32_t*>();
}

int show_ability_activation(int player, bool param_2)
{
    void *func = RVA(0x268d0);
    int result;

    __asm
    {
        mov edx, player
        movzx eax, param_2
        push eax
        call func
        add esp, 4
        mov result, eax
    }

    return result;
}

void reset_ability_activation(int player)
{
    RVA(0x93c648).ptr<uint32_t*>()[player] = 0;
}

bool has_held_item(BattleState *state, int item)
{
    if((state == nullptr) || (state->active_puppet == nullptr))
        return false;
    return (decrypt_puppet(state->active_puppet).held_item == item);
}

unsigned int get_held_item(BattleState *state)
{
    if((state == nullptr) || (state->active_puppet == nullptr))
        return 0;
    return decrypt_puppet(state->active_puppet).held_item;
}

int show_item_consume(void)
{
    return RVA(0x26230).ptr<int(*)(void)>()();
}

void reset_item_consume(int player)
{
    void *func = RVA(0x26200);

    __asm
    {
        mov eax, player
        call func
    }
}

unsigned int get_rng(int max)
{
    void *func = RVA(0x1cee0);
    unsigned int result;

    __asm
    {
        mov eax, max
        call func
        mov result, eax
    }

    return result;
}

unsigned int calculate_damage(BattleState *state, BattleState *otherstate, int player, uint32_t accuracy,
                              bool crit, BattleData *bdata, BattleData *otherbdata, uint16_t skill_id)
{
    void *func = RVA(0x2dc60);

    unsigned int result;
    __asm
    {
        mov ecx, state
        mov edx, otherstate
        movzx eax, skill_id
        push eax
        push bdata
        movzx eax, crit
        push eax
        push accuracy
        push player
        mov eax, otherbdata
        call func
        add esp, 0x14
        mov result, eax
    }

    return result;
}

SkillData *get_adjusted_skill_data(int player, uint16_t skill_id)
{
    auto func = RVA(0x28850).ptr<SkillData*(__fastcall*)(int, uint16_t)>();
    return func(player, skill_id);
}

bool can_use_items(BattleState *state)
{
    auto abl = state->active_ability;
    auto terrain = get_terrain_state()->terrain_type;
    return (abl != 245) && (abl != 390) && (terrain != TERRAIN_KOHRYU);
}

double get_type_multiplier(int player, BattleState *state, uint16_t skill_id, BattleState *otherstate)
{
    auto func = RVA(0x2d8c0).ptr<double(*)()>();
    double result;

    __asm
    {
        mov ecx, player
        mov edx, state
        push otherstate
        movzx eax, skill_id
        push eax
        call func
        add esp, 8
        fstp result;
    }

    return result;
}

int do_heal_anim()
{
    auto func = RVA(0x26710).ptr<int(*)()>();
    return func();
}

void reset_heal_anim(int player)
{
    *RVA(0x93cf44).ptr<uint32_t*>() = player;
    *RVA(0x93c67c).ptr<uint32_t*>() = 0;
    *RVA(0x93c870).ptr<uint32_t*>() = 0;
    *RVA(0x93ccbc).ptr<uint32_t*>() = 0;
}

void load_puppet_sprite(int puppet_id)
{
    auto loadsprite = RVA(0x15ba30).ptr<void*>();
    __asm
    {
        mov edi, puppet_id
        call loadsprite
    }
}

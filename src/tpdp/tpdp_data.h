#pragma once
#include <string>
#include "../typedefs.h"
#include "mem_structs.h"
#include "../hook.h"

// Definitions for various data types and accessors for
// known TPDP functions and memory structures.

enum PuppetStyleType
{
    STYLE_NONE = 0,
    STYLE_NORMAL,
    STYLE_POWER,
    STYLE_DEFENSE,
    STYLE_ASSIST,
    STYLE_SPEED,
    STYLE_EXTRA,
    STYLE_MAX
};

enum ElementType
{
    ELEMENT_NONE = 0,
    ELEMENT_VOID,
    ELEMENT_FIRE,
    ELEMENT_WATER,
    ELEMENT_NATURE,
    ELEMENT_EARTH,
    ELEMENT_STEEL,
    ELEMENT_WIND,
    ELEMENT_ELECTRIC,
    ELEMENT_LIGHT,
    ELEMENT_DARK,
    ELEMENT_NETHER,
    ELEMENT_POISON,
    ELEMENT_FIGHTING,
    ELEMENT_ILLUSION,
    ELEMENT_SOUND,
    ELEMENT_DREAM,
    ELEMENT_WARPED,
    ELEMENT_MAX
};

enum SkillType
{
    SKILL_TYPE_FOCUS = 0,
    SKILL_TYPE_SPREAD,
    SKILL_TYPE_STATUS,
    SKILL_TYPE_MAX
};

enum SkillClass
{
    SKILL_CLASS_NONE = 0,
    SKILL_CLASS_BU,
    SKILL_CLASS_EN
};

enum PuppetMark
{
    MARK_NONE = 0,
    MARK_RED,
    MARK_BLUE,
    MARK_BLACK,
    MARK_WHITE,
    MARK_GREEN,
    MARK_MAX
};

enum CostumeType
{
    COSTUME_NORMAL = 0,
    COSTUME_ALT_COLOR,
    COSTUME_ALT_OUTFIT,
    COSTUME_WEDDING_DRESS,
    COSTUME_MAX
};

enum Terrain
{
    TERRAIN_NONE = 0,
    TERRAIN_SEIRYU,
    TERRAIN_SUZAKU,
    TERRAIN_BYAKKO,
    TERRAIN_GENBU,
    TERRAIN_KOHRYU
};

enum Weather
{
    WEATHER_NONE = 0,
    WEATHER_CALM,
    WEATHER_AURORA,
    WEATHER_HEAVYFOG,
    WEATHER_DUSTSTORM,
    WEATHER_SUNSHOWER
};

enum Status
{
    STATUS_NONE = 0,
    STATUS_POIS,
    STATUS_BURN,
    STATUS_BLIND,
    STATUS_FEAR,
    STATUS_WEAK,
    STATUS_PARA,
    STATUS_HVYPOIS,
    STATUS_HVYBURN,
    STATUS_STOP,
    STATUS_FAINT,
    STATUS_SHOCK,
    STATUS_HVYWEAK,
    STATUS_CONFUSE
};

enum StatIndex
{
    STAT_HP = 0,
    STAT_FOATK,
    STAT_FODEF,
    STAT_SPATK,
    STAT_SPDEF,
    STAT_SPEED,

    // only used for stat modifiers
    STAT_ACC = 7,
    STAT_EVASION = 8,
    STAT_CRIT_RATE = 9
};

// Returns pointer to array of 2 BattleData structures representing the each players side of the field.
// battle_data[0] is the local player, battle_data[1] is the opponent.
//
// IMPORTANT: the players party may be accessed from here (battle_data.puppets[6]), but the puppets must be decrypted first.
// The level, hp, sp, status_effects, and stopped_turns fields of party puppets are unencrypted and may be accessed directly.
HOOKAPI BattleData *get_battle_data();

// Returns state member of BattleData for the given player.
HOOKAPI BattleState *get_battle_state(int player);

// Show message that weather has changed.
// Call each frame until zero is returned.
HOOKAPI int do_weather_msg();

// Call once before beginning a new weather message.
HOOKAPI void reset_weather_msg();

// Show message that terrain has changed.
// Call each frame until zero is returned.
HOOKAPI int do_terrain_msg();

// Call once before beginning a new terrain message.
HOOKAPI void reset_terrain_msg();

// Show arbitrary message during battle.
// Call each frame for at least a second.
HOOKAPI int set_battle_text(const char* msg, bool unknown_arg = false, bool unknown_arg2 = false);
static inline int set_battle_text(std::string msg, bool unknown_arg = false, bool unknown_arg2 = false)
{
    return set_battle_text(msg.c_str(), unknown_arg, unknown_arg2);
}

// Call once before starting a new message.
HOOKAPI void clear_battle_text();

// Returns true if game is set to 60 fps, false if 30.
HOOKAPI bool is_game_60fps();
HOOKAPI int get_game_fps();

// Returns pointer to struct containing weather/terrain state.
// Has side effects aside from just returning the pointer, call each time you
// want to modify weather/terrain state.
HOOKAPI TerrainState *get_terrain_state();

// True if the current skill activated successfully.
HOOKAPI bool& skill_succeeded();

// Returns state value for the current skill.
// Value is writable and persists between calls.
// Custom skills should use this to maintain their internal state.
HOOKAPI uint32_t& get_skill_state();

// Returns pointer to array of 1024 SkillData.
HOOKAPI SkillData *get_skill_data();

// Returns pointer to array of 512 PuppetData.
HOOKAPI PuppetData *get_puppet_data();

HOOKAPI PartyPuppet decrypt_puppet(const PartyPuppet *puppet);
HOOKAPI PartyPuppet encrypt_puppet(const PartyPuppet *puppet);
HOOKAPI void decrypt_puppet_nocopy(PartyPuppet *puppet);
HOOKAPI void encrypt_puppet_nocopy(PartyPuppet *puppet);

// Apply status effect(s) and display the animation for them.
// Call each frame until zero is returned.
HOOKAPI int apply_status_effect(int player, Status status1, Status status2);

// Resets animation when setting multiple statuses on the same turn.
HOOKAPI void reset_status_anim();

// stat_index = hp, f.atk, f.def, s.atk, s.def, speed starting from 0.
HOOKAPI unsigned int calculate_stat(unsigned int level, unsigned int stat_index, EncryptedPuppet *puppet, unsigned int alt_form = 0);
HOOKAPI unsigned int calculate_stat(unsigned int stat_index, PartyPuppet *puppet, unsigned int alt_form = 0);

// Returns activator function for normal skills
HOOKAPI SkillCall get_default_activator();

// Call every frame till 0 returned
HOOKAPI int apply_stat_mod(StatIndex stat_index, int stat_mod, bool param_3, bool param_4, bool param_5, bool *param_6, int player);
HOOKAPI int apply_stat_mod(StatIndex stat_index, int stat_mod, int player);

HOOKAPI int get_stat_mod(StatIndex index, BattleState *state);

// Reset animation for stat drops/increases
// Call BEFORE calling apply_stat_mod
HOOKAPI void reset_stat_mod(void);

HOOKAPI uint32_t& get_ability_state();
HOOKAPI int show_ability_activation(int player, bool param_2 = false); // Call every frame till 0 returned
HOOKAPI void reset_ability_activation(int player);

HOOKAPI bool has_held_item(BattleState *state, int item);
HOOKAPI unsigned int get_held_item(BattleState *state);

HOOKAPI int show_item_consume(void); // Call every frame till 0 returned
HOOKAPI void reset_item_consume(int player);

// Call the games built-in PRNG
// Returns a value between 0 and 'max' (inclusive)
// This function MUST be used for all rng in netplay
HOOKAPI unsigned int get_rng(int max);

// Probably should not be called directly
HOOKAPI unsigned int calculate_damage(BattleState *state, BattleState *otherstate, int player, uint32_t acc,
                                      bool crit, BattleData *bdata, BattleData *otherbdata, uint16_t skill_id);

// Get SkillData for given skill after type/power/prio/etc changes
// from abilities/weather/terrain/etc
HOOKAPI SkillData *get_adjusted_skill_data(int player, uint16_t skill_id);

// Checks for conditions that would prevent use of hold2 items (e.g. kohryu, wasteful)
HOOKAPI bool can_use_items(BattleState *state);

// Compute type interaction between given skill and active
// puppet of given BattleStates
// BattleState params may be null if player is given
HOOKAPI double get_type_multiplier(int player, BattleState *state, uint16_t skill_id, BattleState *otherstate);

// Call every frame till 0 returned
HOOKAPI int do_heal_anim();
HOOKAPI void reset_heal_anim(int player); // call before calling do_heal_anim

HOOKAPI void load_puppet_sprite(int puppet_id);

// check if player has any living puppets remaining
HOOKAPI bool has_live_puppets(int player);

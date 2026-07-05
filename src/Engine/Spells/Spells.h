#pragma once

#include <cstdint>
#include <array>
#include <string>

#include "Application/Paths/GameVersion.h"

#include "Engine/Objects/ItemEnums.h"
#include "Engine/Objects/CharacterEnums.h"
#include "Engine/Objects/SpriteEnums.h"
#include "Engine/Time/Duration.h"
#include "Library/Geometry/Vec.h"

#include "Utility/IndexedArray.h"

#include "SpellEnums.h"

class Blob;

struct SpellInfo {
    std::string name;
    std::string pShortName;
    std::string pDescription;
    std::string pBasicSkillDesc;
    std::string pExpertSkillDesc;
    std::string pMasterSkillDesc;
    std::string pGrandmasterSkillDesc;
    DamageType damageType;
    int field_20;
};

struct SpellStats {
    /**
     * @offset 0x45384A
     */
    void Initialize(const Blob &spells, GameVersion version);

    IndexedArray<SpellInfo, SPELL_FIRST_REGULAR, SPELL_LAST_REGULAR> pInfos;
};

class SpellData {
 public:
    SpellData():SpellData(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, MASTERY_NONE) {}
    SpellData(int16_t inNormalMana, int16_t inExpertLevelMana,
              int16_t inMasterLevelMana, int16_t inMagisterLevelMana,
              int16_t inNormalLevelRecovery, int16_t inExpertLevelRecovery,
              int16_t inMasterLevelRecovery, int16_t inMagisterLevelRecovery,
              int8_t inBaseDamage, int8_t inBonusSkillDamage, SpellFlags inStats,
              Mastery inSkillMastery);
    IndexedArray<uint16_t, MASTERY_FIRST, MASTERY_LAST> mana_per_skill;
    IndexedArray<Duration, MASTERY_FIRST, MASTERY_LAST> recovery_per_skill;
    int8_t baseDamage;
    int8_t bonusSkillDamage;
    SpellFlags flags;
    Mastery skillMastery;
    // char field_12;
    // char field_13;
    // int16_t field_14;
};

struct SpellBookIconPos {
    int32_t Xpos;
    int32_t Ypos;
};

extern SpellStats *pSpellStats;

extern IndexedArray<std::array<struct SpellBookIconPos, 12>, MAGIC_SCHOOL_FIRST, MAGIC_SCHOOL_LAST> pIconPos;

extern const IndexedArray<SpriteId, SPELL_FIRST_WITH_SPRITE, SPELL_LAST_WITH_SPRITE> SpellSpriteMapping;  // 4E3ACC
extern IndexedArray<SpellData, SPELL_FIRST_REGULAR, SPELL_LAST_REGULAR> pSpellDatas;
extern const IndexedArray<uint16_t, SPELL_FIRST_WITH_SPRITE, SPELL_LAST_WITH_SPRITE> SpellSoundIds;

/**
 * @offset 0x43AFE3
 */
int CalcSpellDamage(SpellId uSpellID, int spellLevel, Mastery skillMastery, int currentHp);

/**
 * @offset 0x427769
 */
bool IsSpellQuickCastableOnShiftClick(SpellId uSpellID);

/**
 * Translates a native spell id into the `SpellId` whose EFFECT should run when casting it.
 *
 * MM6 and MM7 both have 99 regular spells packed into the identical 9-school x 11-spell layout at ids
 * 1..99, but the spell that sits at a given id often differs between the two games. The cast dispatch in
 * `castSpell()` switches on MM7-named `SpellId` constants, so an MM6 spell has to be mapped to the MM7
 * spell that produces the matching effect before it's dispatched.
 *
 * For MM7 (and for any non-regular spell id, or any id with no MM6 remap) this is the identity function,
 * so MM7 behavior is entirely unchanged.
 *
 * @param nativeId                      Native spell id, as it appears in the running game's data.
 * @param version                       Which game is running.
 * @return                              MM7 `SpellId` whose effect should be cast.
 */
SpellId translateForCast(SpellId nativeId, GameVersion version);

/**
 * Function for processing spells cast from game scripts.
 */
void eventCastSpell(SpellId uSpellID, Mastery skillMastery, int skillLevel, Vec3f from, Vec3f to);  // sub_448DF8

void armageddonProgress();

/**
 * Overwrites the mana costs and recovery times in `pSpellDatas` with the MM6 values extracted from
 * MM6.EXE, and clamps `skillMastery` down from Grandmaster to Master (MM6 has no Grandmaster tier).
 * `pSpellDatas` is statically initialized with MM7 numbers; this must be called once at engine init
 * when running MM6 (right after `SpellStats::Initialize`), and must not be called for MM7.
 * `baseDamage`, `bonusSkillDamage` and `flags` are left untouched (MM6's spell table carries no such
 * fields, so those keep their MM7 approximations).
 */
void applyMm6SpellDatas();

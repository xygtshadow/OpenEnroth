#pragma once

#include <array>
#include <cassert>
#include <utility>

#include "Engine/Spells/SpellEnums.h"

#include "Utility/Segment.h"

#include "CharacterEnums.h"

//
// Condition
//

PortraitId portraitForCondition(Condition condition);

/**
 * @return                              List of conditions that can be applied to the character.
 */
inline Segment<Condition> allConditions() {
    return {CONDITION_FIRST, CONDITION_LAST};
}

//
// CharacterSkillMastery
//

inline Segment<Mastery> allSkillMasteries() {
    return {MASTERY_FIRST, MASTERY_LAST};
}

inline int spellCountForMastery(Mastery maxMastery) {
    switch(maxMastery) {
    case MASTERY_NOVICE:
        return 4;
    case MASTERY_EXPERT:
        return 7;
    case MASTERY_MASTER:
        return 10;
    case MASTERY_GRANDMASTER:
        return 11;
    default:
        assert(false);
        return 0;
    }
}


//
// CharacterSkillType
//

inline Segment<Skill> allSkills() {
    return {SKILL_FIRST, SKILL_LAST};
}

/**
 * @return                              List of skills that are visible to the player and that are stored in a savegame.
 */
inline Segment<Skill> allVisibleSkills() {
    return {SKILL_FIRST_VISIBLE, SKILL_LAST_VISIBLE};
}

/**
 * @return                              List of skills that are drawn in the "Armor" section of the character
 *                                      screen's skills tab.
 */
inline std::initializer_list<Skill> allArmorSkills() {
    static constexpr std::initializer_list<Skill> result = {
        SKILL_LEATHER, SKILL_CHAIN, SKILL_PLATE,
        SKILL_SHIELD,  SKILL_DODGE
    };

    return result;
}

/**
 * @return                              List of skills that are drawn in the "Weapons" section of the character
 *                                      screen's skills tab.
 */
inline std::initializer_list<Skill> allWeaponSkills() {
    static constexpr std::initializer_list<Skill> result = {
        SKILL_AXE,   SKILL_BOW,     SKILL_DAGGER,
        SKILL_MACE,  SKILL_SPEAR,   SKILL_STAFF,
        SKILL_SWORD, SKILL_UNARMED, SKILL_BLASTER
        // CHARACTER_SKILL_CLUB is not displayed in skills.
    };

    return result;
}

/**
 * @return                              List of skills that are drawn in the "Misc" section of the character
 *                                      screen's skills tab.
 */
inline std::initializer_list<Skill> allMiscSkills() {
    static constexpr std::initializer_list<Skill> result = {
        SKILL_ALCHEMY,      SKILL_ARMSMASTER,
        SKILL_BODYBUILDING, SKILL_ITEM_ID,
        SKILL_MONSTER_ID,   SKILL_LEARNING,
        SKILL_TRAP_DISARM,  SKILL_MEDITATION,
        SKILL_MERCHANT,     SKILL_PERCEPTION,
        SKILL_REPAIR,       SKILL_STEALING
    };

    return result;
}

/**
 * @return                              List of skills that are drawn in the "Magic" section of the character
 *                                      screen's skills tab.
 */
inline std::initializer_list<Skill> allMagicSkills() {
    static constexpr std::initializer_list<Skill> result = {
        SKILL_FIRE,  SKILL_AIR,    SKILL_WATER,
        SKILL_EARTH, SKILL_SPIRIT, SKILL_MIND,
        SKILL_BODY,  SKILL_LIGHT,  SKILL_DARK
    };

    return result;
}

Skill skillForMagicSchool(MagicSchool school);

Skill skillForSpell(SpellId spell);


//
// CharacterClass
//

inline Class getTier1Class(Class classType) {
    return static_cast<Class>(std::to_underlying(classType) & ~3);
}

inline Class getTier2Class(Class classType) {
    return static_cast<Class>((std::to_underlying(classType) & ~3) + 1);
}

inline Class getTier3LightClass(Class classType) {
    return static_cast<Class>((std::to_underlying(classType) & ~3) + 2);
}

inline Class getTier3DarkClass(Class classType) {
    return static_cast<Class>((std::to_underlying(classType) & ~3) + 3);
}

inline int getClassTier(Class classType) {
    int index = (std::to_underlying(classType) & 3);
    return index == 3 ? 3 : index + 1;
}

/**
 * Translates an MM6 class byte onto the engine's `Class` enum, which follows MM7's numbering.
 *
 * MM6 stores a character's class as a single byte, `base * 3 + tier`, with the six base classes
 * ordered Knight(0), Cleric(3), Sorcerer(6), Paladin(9), Archer(12), Druid(15) and tiers 0/1/2
 * (e.g. Paladin/Crusader/Hero). The `Class` enum instead lays each line out as `base * 4 + tier`
 * (there is a fourth MM7-only tier per line), so MM6 promotion events - which drive `Set(Class, N)`
 * and `If(Class == N)` with these bytes - must be translated or they misfire. MM6's three tiers
 * occupy the first three of each MM7 line.
 *
 * @param mm6ClassByte                  MM6 class byte, 0..17.
 * @return                              Matching `Class` enum value (identity for out-of-range input).
 */
inline Class classFromMm6ClassByte(int mm6ClassByte) {
    static constexpr std::array<Class, 6> mm6BaseClasses = {
        CLASS_KNIGHT, CLASS_CLERIC, CLASS_SORCERER, CLASS_PALADIN, CLASS_ARCHER, CLASS_DRUID};
    if (mm6ClassByte < 0 || mm6ClassByte > 17)
        return static_cast<Class>(mm6ClassByte); // Not one of MM6's 18 classes; leave untranslated.
    return static_cast<Class>(std::to_underlying(mm6BaseClasses[mm6ClassByte / 3]) + mm6ClassByte % 3);
}

/**
 * Get promotions of higher tier class relative to given one.
 *
 * Base class is of tier 1.
 * After initial promotion class becomes tier 2.
 * Tier 2 class is promoted through light or dark path to tier 3 class.
 *
 * @param classType                     Character class.
 * @return                              All classes that the given class can be promoted to.
 */
inline Segment<Class> promotionsForClass(Class classType) {
    int tier = getClassTier(classType);

    if (tier == 1) {
        return {getTier2Class(classType), getTier3DarkClass(classType)};
    } else if (tier == 2) {
        return {getTier3LightClass(classType), getTier3DarkClass(classType)};
    } else {
        return {}; // tier 3 max
    }
}


//
// CharacterAttributeType
//

/**
 * @return                              All attributes that can be improved though attribute item enchantments, like
 *                                      "of Might".
 */
inline Segment<Attribute> allEnchantableAttributes() {
    return {ATTRIBUTE_FIRST_ENCHANTABLE, ATTRIBUTE_LAST_ENCHANTABLE};
}

/**
 * @return                              Segment containing the 7 basic character stats (starting with might & ending
 *                                      with luck).
 */
inline Segment<Attribute> allStatAttributes() {
    return {ATTRIBUTE_FIRST_STAT, ATTRIBUTE_LAST_STAT};
}

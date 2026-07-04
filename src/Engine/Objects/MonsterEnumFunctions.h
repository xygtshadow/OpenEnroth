#pragma once

#include <span>
#include <utility>

#include "Application/Paths/GameVersion.h"

#include "Engine/Data/HouseEnums.h"

#include "Utility/Segment.h"
#include "Library/Color/ColorTable.h"

#include "ItemEnums.h"
#include "CharacterEnums.h"
#include "MonsterEnums.h"
#include "SpriteEnums.h"

//
// MonsterId
//

inline Segment<MonsterId> allMonsters() {
    return {MONSTER_FIRST, MONSTER_LAST};
}

/**
 * @return                              A span of all monsters that can appear in Arena.
 */
std::span<const MonsterId> allArenaMonsters();


//
// MonsterType
//

inline Segment<MonsterType> allMonsterTypes() {
    return {MONSTER_TYPE_FIRST, MONSTER_TYPE_LAST};
}

inline MonsterType monsterTypeForMonsterId(MonsterId monsterId) {
    return static_cast<MonsterType>((std::to_underlying(monsterId) - 1) / 3 + 1);
}

inline Segment<MonsterId> monsterIdsForMonsterType(MonsterType monsterType) {
    MonsterId first = static_cast<MonsterId>((std::to_underlying(monsterType) - 1) * 3 + 1);
    MonsterId last = static_cast<MonsterId>(std::to_underlying(first) + 2);
    return {first, last};
}

inline bool isPeasant(MonsterType monsterType) {
    return
        (monsterType >= MONSTER_TYPE_FIRST_PEASANT_DWARF && monsterType <= MONSTER_TYPE_LAST_PEASANT_DWARF) ||
        (monsterType >= MONSTER_TYPE_FIRST_PEASANT_ELF && monsterType <= MONSTER_TYPE_LAST_PEASANT_ELF) ||
        (monsterType >= MONSTER_TYPE_FIRST_PEASANT_HUMAN && monsterType <= MONSTER_TYPE_LAST_PEASANT_HUMAN) ||
        (monsterType >= MONSTER_TYPE_FIRST_PEASANT_GOBLIN && monsterType <= MONSTER_TYPE_LAST_PEASANT_GOBLIN);
}

inline bool isPeasant(MonsterId monsterId) {
    return isPeasant(monsterTypeForMonsterId(monsterId));
}

/**
 * Version-aware variant of isPeasant(). MM6 peasants are monsters.txt rows 121-144
 * (PeasantF1A..PeasantM4C), i.e. 3-tier families 41-48; MM7's own peasant id ranges would also
 * swallow MM6's Oozes, Ogres, Rats, Robots, Skeletons, Sorcerers, Titans and Werewolves.
 *
 * @param monsterType                   Monster type (3-tier family index) to check.
 * @param version                       Game version the type belongs to.
 * @return                              Whether monsters of this type are peasants.
 */
inline bool isPeasant(MonsterType monsterType, GameVersion version) {
    if (version != GAME_VERSION_MM6)
        return isPeasant(monsterType);
    return std::to_underlying(monsterType) >= 41 && std::to_underlying(monsterType) <= 48;
}

inline bool isPeasant(MonsterId monsterId, GameVersion version) {
    return isPeasant(monsterTypeForMonsterId(monsterId), version);
}

Sex sexForMonsterType(MonsterType monsterType);

Race raceForMonsterType(MonsterType monsterType);

bool isBountyHuntable(MonsterType monsterType, HouseId townHall);

ItemId itemDropForMonsterType(MonsterType monsterType);


//
// MonsterTier
//

inline MonsterTier monsterTierForMonsterId(MonsterId monsterId) {
    return static_cast<MonsterTier>((std::to_underlying(monsterId) - std::to_underlying(MONSTER_FIRST)) % 3);
}


//
// MonsterSupertype
//

/**
 * @offset 0x00438BDF
 *
 * @param monsterType                   Monster type to check.
 * @return                              Supertype for the provided monster type.
 */
MonsterSupertype supertypeForMonsterType(MonsterType monsterType);

inline MonsterSupertype supertypeForMonsterId(MonsterId monsterId) {
    return supertypeForMonsterType(monsterTypeForMonsterId(monsterId));
}

/**
 * Version-aware variant of supertypeForMonsterId(). In MM6 sessions monster ids index MM6's
 * monsters.txt, so the MM7 MonsterType-derived classification above doesn't apply and the
 * MM6 rows are classified directly.
 *
 * @param monsterId                     Monster id to check.
 * @param version                       Game version the id belongs to.
 * @return                              Supertype for the provided monster id.
 */
MonsterSupertype supertypeForMonsterId(MonsterId monsterId, GameVersion version);


//
// MonsterAttackPreference
//

std::span<const MonsterAttackPreference> allMonsterAttackPreferences();


//
// MonsterProjectile
//

/**
 * @param projectile                    Monster projectile to get a sprite id for.
 * @return                              Sprite id to use for the given monster projectile.
 * @see isMonsterProjectileSprite
 */
SpriteId spriteForMonsterProjectile(MonsterProjectile projectile);

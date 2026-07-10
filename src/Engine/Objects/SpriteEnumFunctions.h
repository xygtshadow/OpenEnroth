#pragma once

#include <cassert>


#include "SpriteEnums.h"

inline SpriteId impactSprite(SpriteId projectile) {
    assert(std::to_underlying(projectile) % 5 == 0);
    return static_cast<SpriteId>(std::to_underlying(projectile) + 1);
}

/**
 * @param sprite                        Sprite id to check.
 * @return                              Whether `sprite` is a sprite for a monster projectile attack. Note that monsters
 *                                      can also attack with spells, and this function only checks for non-spell
 *                                      projectile attacks.
 * @see spriteForMonsterProjectile
 */
inline bool isMonsterProjectileSprite(SpriteId sprite) {
    switch (sprite) {
    case SPRITE_PROJECTILE_ARROW:
    case SPRITE_PROJECTILE_FLAMING_ARROW:
    case SPRITE_PROJECTILE_FIRE_BOLT:
    case SPRITE_PROJECTILE_AIR_BOLT:
    case SPRITE_PROJECTILE_WATER_BOLT:
    case SPRITE_PROJECTILE_EARTH_BOLT:
    case SPRITE_PROJECTILE_SPIRIT_BOLT:
    case SPRITE_PROJECTILE_MIND_BOLT:
    case SPRITE_PROJECTILE_BODY_BOLT:
    case SPRITE_PROJECTILE_LIGHT_BOLT:
    case SPRITE_PROJECTILE_DARK_BOLT:
    case SPRITE_PROJECTILE_BLASTER:
    // MM6's projectile bank: ids 500-550 are already covered by the colliding MM7 names above,
    // these are the four values MM7 doesn't use.
    case SPRITE_MM6_PROJECTILE_ENERGY:
    case SPRITE_MM6_PROJECTILE_MAGIC:
    case SPRITE_MM6_PROJECTILE_ROCK:
    case SPRITE_MM6_PROJECTILE_LASER:
        return true;
    default:
        return false;
    }
}

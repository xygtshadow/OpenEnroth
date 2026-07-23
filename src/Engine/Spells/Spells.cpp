#include "Engine/Spells/Spells.h"

#include <algorithm>
#include <array>
#include <map>
#include <string>

#include "Engine/Engine.h"
#include "Engine/Party.h"
#include "Engine/Graphics/Indoor.h"
#include "Engine/Graphics/Overlays.h"
#include "Engine/Random/Random.h"
#include "Engine/Objects/Actor.h"
#include "Engine/Objects/ObjectList.h"
#include "Engine/Objects/SpriteObject.h"
#include "Engine/SpellFxRenderer.h"
#include "Engine/TurnEngine/TurnEngine.h"
#include "Engine/Spells/SpellEnumFunctions.h"

#include "Library/Logger/Logger.h"
#include "Library/Serialization/Serialization.h"

#include "Media/Audio/AudioPlayer.h"

#include "Utility/Math/TrigLut.h"
#include "Utility/Memory/Blob.h"
#include "Utility/String/Ascii.h"
#include "Utility/String/Split.h"
#include "Utility/String/Transformations.h"
#include "Utility/MapAccess.h"

SpellFxRenderer *spell_fx_renderer = EngineIocContainer::ResolveSpellFxRenderer();

SpellStats *pSpellStats = nullptr;

/**
 * @offset 0x4E3ACC
 */
const IndexedArray<SpriteId, SPELL_FIRST_WITH_SPRITE, SPELL_LAST_WITH_SPRITE> SpellSpriteMapping = {
    {SPELL_FIRE_TORCH_LIGHT, SPRITE_SPELL_FIRE_TORCH_LIGHT},
    {SPELL_FIRE_FIRE_BOLT, SPRITE_SPELL_FIRE_FIRE_BOLT},
    {SPELL_FIRE_PROTECTION_FROM_FIRE, SPRITE_SPELL_FIRE_PROTECTION_FROM_FIRE},
    {SPELL_FIRE_FIRE_AURA, SPRITE_SPELL_FIRE_FIRE_AURA},
    {SPELL_FIRE_HASTE, SPRITE_SPELL_FIRE_HASTE},
    {SPELL_FIRE_FIREBALL, SPRITE_SPELL_FIRE_FIREBALL},
    {SPELL_FIRE_FIRE_SPIKE, SPRITE_SPELL_FIRE_FIRE_SPIKE},
    {SPELL_FIRE_IMMOLATION, SPRITE_SPELL_FIRE_IMMOLATION},
    {SPELL_FIRE_METEOR_SHOWER, SPRITE_SPELL_FIRE_METEOR_SHOWER},
    {SPELL_FIRE_INFERNO, SPRITE_SPELL_FIRE_INFERNO},
    {SPELL_FIRE_INCINERATE, SPRITE_SPELL_FIRE_INCINERATE},

    {SPELL_AIR_WIZARD_EYE, SPRITE_SPELL_AIR_WIZARD_EYE},
    {SPELL_AIR_FEATHER_FALL, SPRITE_SPELL_AIR_FEATHER_FALL},
    {SPELL_AIR_PROTECTION_FROM_AIR, SPRITE_SPELL_AIR_PROTECTION_FROM_AIR},
    {SPELL_AIR_SPARKS, SPRITE_SPELL_AIR_SPARKS},
    {SPELL_AIR_JUMP, SPRITE_SPELL_AIR_JUMP},
    {SPELL_AIR_SHIELD, SPRITE_SPELL_AIR_SHIELD},
    {SPELL_AIR_LIGHTNING_BOLT, SPRITE_SPELL_AIR_LIGHTNING_BOLT},
    {SPELL_AIR_INVISIBILITY, SPRITE_SPELL_AIR_INVISIBILITY},
    {SPELL_AIR_IMPLOSION, SPRITE_SPELL_AIR_IMPLOSION},
    {SPELL_AIR_FLY, SPRITE_SPELL_AIR_FLY},
    {SPELL_AIR_STARBURST, SPRITE_SPELL_AIR_STARBURST},

    {SPELL_WATER_AWAKEN, SPRITE_SPELL_WATER_AWAKEN},
    {SPELL_WATER_POISON_SPRAY, SPRITE_SPELL_WATER_POISON_SPRAY},
    {SPELL_WATER_PROTECTION_FROM_WATER, SPRITE_SPELL_WATER_PROTECTION_FROM_WATER},
    {SPELL_WATER_ICE_BOLT, SPRITE_SPELL_WATER_ICE_BOLT},
    {SPELL_WATER_WATER_WALK, SPRITE_SPELL_WATER_WATER_WALK},
    {SPELL_WATER_RECHARGE_ITEM, SPRITE_SPELL_WATER_RECHARGE_ITEM},
    {SPELL_WATER_ACID_BURST, SPRITE_SPELL_WATER_ACID_BURST},
    {SPELL_WATER_ENCHANT_ITEM, SPRITE_SPELL_WATER_ENCHANT_ITEM},
    {SPELL_WATER_TOWN_PORTAL, SPRITE_SPELL_WATER_TOWN_PORTAL},
    {SPELL_WATER_ICE_BLAST, SPRITE_SPELL_WATER_ICE_BLAST},
    {SPELL_WATER_LLOYDS_BEACON, SPRITE_SPELL_WATER_LLOYDS_BEACON},

    {SPELL_EARTH_STUN, SPRITE_SPELL_EARTH_STUN},
    {SPELL_EARTH_SLOW, SPRITE_SPELL_EARTH_SLOW},
    {SPELL_EARTH_PROTECTION_FROM_EARTH, SPRITE_SPELL_EARTH_PROTECTION_FROM_EARTH},
    {SPELL_EARTH_DEADLY_SWARM, SPRITE_SPELL_EARTH_DEADLY_SWARM},
    {SPELL_EARTH_STONESKIN, SPRITE_SPELL_EARTH_STONESKIN},
    {SPELL_EARTH_BLADES, SPRITE_SPELL_EARTH_BLADES},
    {SPELL_EARTH_STONE_TO_FLESH, SPRITE_SPELL_EARTH_STONE_TO_FLESH},
    {SPELL_EARTH_ROCK_BLAST, SPRITE_SPELL_EARTH_ROCK_BLAST},
    {SPELL_EARTH_TELEKINESIS, SPRITE_SPELL_EARTH_TELEKINESIS},
    {SPELL_EARTH_DEATH_BLOSSOM, SPRITE_SPELL_EARTH_DEATH_BLOSSOM},
    {SPELL_EARTH_MASS_DISTORTION, SPRITE_SPELL_EARTH_MASS_DISTORTION},

    {SPELL_SPIRIT_DETECT_LIFE, SPRITE_SPELL_SPIRIT_DETECT_LIFE},
    {SPELL_SPIRIT_BLESS, SPRITE_SPELL_SPIRIT_BLESS},
    {SPELL_SPIRIT_FATE, SPRITE_SPELL_SPIRIT_FATE},
    {SPELL_SPIRIT_TURN_UNDEAD, SPRITE_SPELL_SPIRIT_TURN_UNDEAD},
    {SPELL_SPIRIT_REMOVE_CURSE, SPRITE_SPELL_SPIRIT_REMOVE_CURSE},
    {SPELL_SPIRIT_PRESERVATION, SPRITE_SPELL_SPIRIT_PRESERVATION},
    {SPELL_SPIRIT_HEROISM, SPRITE_SPELL_SPIRIT_HEROISM},
    {SPELL_SPIRIT_SPIRIT_LASH, SPRITE_SPELL_SPIRIT_SPIRIT_LASH},
    {SPELL_SPIRIT_RAISE_DEAD, SPRITE_SPELL_SPIRIT_RAISE_DEAD},
    {SPELL_SPIRIT_SHARED_LIFE, SPRITE_SPELL_SPIRIT_SHARED_LIFE},
    {SPELL_SPIRIT_RESSURECTION, SPRITE_SPELL_SPIRIT_RESSURECTION},

    {SPELL_MIND_REMOVE_FEAR, SPRITE_SPELL_MIND_REMOVE_FEAR},
    {SPELL_MIND_MIND_BLAST, SPRITE_SPELL_MIND_MIND_BLAST},
    {SPELL_MIND_PROTECTION_FROM_MIND, SPRITE_SPELL_MIND_PROTECTION_FROM_MIND},
    {SPELL_MIND_TELEPATHY, SPRITE_SPELL_MIND_TELEPATHY},
    {SPELL_MIND_CHARM, SPRITE_SPELL_MIND_CHARM},
    {SPELL_MIND_CURE_PARALYSIS, SPRITE_SPELL_MIND_CURE_PARALYSIS},
    {SPELL_MIND_BERSERK, SPRITE_SPELL_MIND_BERSERK},
    {SPELL_MIND_MASS_FEAR, SPRITE_SPELL_MIND_MASS_FEAR},
    {SPELL_MIND_CURE_INSANITY, SPRITE_SPELL_MIND_CURE_INSANITY},
    {SPELL_MIND_PSYCHIC_SHOCK, SPRITE_SPELL_MIND_PSYCHIC_SHOCK},
    {SPELL_MIND_ENSLAVE, SPRITE_SPELL_MIND_ENSLAVE},

    {SPELL_BODY_CURE_WEAKNESS, SPRITE_SPELL_BODY_CURE_WEAKNESS},
    {SPELL_BODY_FIRST_AID, SPRITE_SPELL_BODY_FIRST_AID},
    {SPELL_BODY_PROTECTION_FROM_BODY, SPRITE_SPELL_BODY_PROTECTION_FROM_BODY},
    {SPELL_BODY_HARM, SPRITE_SPELL_BODY_HARM},
    {SPELL_BODY_REGENERATION, SPRITE_SPELL_BODY_REGENERATION},
    {SPELL_BODY_CURE_POISON, SPRITE_SPELL_BODY_CURE_POISON},
    {SPELL_BODY_HAMMERHANDS, SPRITE_SPELL_BODY_HAMMERHANDS},
    {SPELL_BODY_CURE_DISEASE, SPRITE_SPELL_BODY_CURE_DISEASE},
    {SPELL_BODY_PROTECTION_FROM_MAGIC, SPRITE_SPELL_BODY_PROTECTION_FROM_MAGIC},
    {SPELL_BODY_FLYING_FIST, SPRITE_SPELL_BODY_FLYING_FIST},
    {SPELL_BODY_POWER_CURE, SPRITE_SPELL_BODY_POWER_CURE},

    {SPELL_LIGHT_LIGHT_BOLT, SPRITE_SPELL_LIGHT_LIGHT_BOLT},
    {SPELL_LIGHT_DESTROY_UNDEAD, SPRITE_SPELL_LIGHT_DESTROY_UNDEAD},
    {SPELL_LIGHT_DISPEL_MAGIC, SPRITE_SPELL_LIGHT_DISPEL_MAGIC},
    {SPELL_LIGHT_PARALYZE, SPRITE_SPELL_LIGHT_PARALYZE},
    {SPELL_LIGHT_SUMMON_ELEMENTAL, SPRITE_SPELL_LIGHT_SUMMON_ELEMENTAL},
    {SPELL_LIGHT_DAY_OF_THE_GODS, SPRITE_SPELL_LIGHT_DAY_OF_THE_GODS},
    {SPELL_LIGHT_PRISMATIC_LIGHT, SPRITE_SPELL_LIGHT_PRISMATIC_LIGHT},
    {SPELL_LIGHT_DAY_OF_PROTECTION, SPRITE_SPELL_LIGHT_DAY_OF_PROTECTION},
    {SPELL_LIGHT_HOUR_OF_POWER, SPRITE_SPELL_LIGHT_HOUR_OF_POWER},
    {SPELL_LIGHT_SUNRAY, SPRITE_SPELL_LIGHT_SUNRAY},
    {SPELL_LIGHT_DIVINE_INTERVENTION, SPRITE_SPELL_LIGHT_DIVINE_INTERVENTION},

    {SPELL_DARK_REANIMATE, SPRITE_SPELL_DARK_REANIMATE},
    {SPELL_DARK_TOXIC_CLOUD, SPRITE_SPELL_DARK_TOXIC_CLOUD},
    {SPELL_DARK_VAMPIRIC_WEAPON, SPRITE_SPELL_DARK_VAMPIRIC_WEAPON},
    {SPELL_DARK_SHRINKING_RAY, SPRITE_SPELL_DARK_SHRINKING_RAY},
    {SPELL_DARK_SHARPMETAL, SPRITE_SPELL_DARK_SHARPMETAL},
    {SPELL_DARK_CONTROL_UNDEAD, SPRITE_SPELL_DARK_CONTROL_UNDEAD},
    {SPELL_DARK_PAIN_REFLECTION, SPRITE_SPELL_DARK_PAIN_REFLECTION},
    {SPELL_DARK_SACRIFICE, SPRITE_SPELL_DARK_SACRIFICE},
    {SPELL_DARK_DRAGON_BREATH, SPRITE_SPELL_DARK_DRAGON_BREATH},
    {SPELL_DARK_ARMAGEDDON, SPRITE_SPELL_DARK_ARMAGEDDON},
    {SPELL_DARK_SOULDRINKER, SPRITE_SPELL_DARK_SOULDRINKER},

    {SPELL_BOW_ARROW, SPRITE_PROJECTILE_ARROW},
    {SPELL_101, SPRITE_PROJECTILE_ARROW}, // TODO(captainurist): Looks like this is a flaming arrow spell, should map to SPRITE_PROJECTILE_FLAMING_ARROW?
    {SPELL_LASER_PROJECTILE, SPRITE_PROJECTILE_BLASTER}};

SpellData::SpellData(int16_t inNormalMana,
                     int16_t inExpertLevelMana,
                     int16_t inMasterLevelMana,
                     int16_t inMagisterLevelMana,
                     int16_t inNormalLevelRecovery,
                     int16_t inExpertLevelRecovery,
                     int16_t inMasterLevelRecovery,
                     int16_t inMagisterLevelRecovery,
                     int8_t inBaseDamage,
                     int8_t inBonusSkillDamage,
                     SpellFlags inFlags,
                     Mastery inSkillMastery) {
    mana_per_skill[MASTERY_NOVICE] = inNormalMana;
    mana_per_skill[MASTERY_EXPERT] = inExpertLevelMana;
    mana_per_skill[MASTERY_MASTER] = inMasterLevelMana;
    mana_per_skill[MASTERY_GRANDMASTER] = inMagisterLevelMana;
    recovery_per_skill[MASTERY_NOVICE] = Duration::fromTicks(inNormalLevelRecovery);
    recovery_per_skill[MASTERY_EXPERT] = Duration::fromTicks(inExpertLevelRecovery);
    recovery_per_skill[MASTERY_MASTER] = Duration::fromTicks(inMasterLevelRecovery);
    recovery_per_skill[MASTERY_GRANDMASTER] = Duration::fromTicks(inMagisterLevelRecovery);
    baseDamage = inBaseDamage;
    bonusSkillDamage = inBonusSkillDamage;
    flags = inFlags;
    skillMastery = inSkillMastery;
}

/**
 * Description of spells.
 *
 *                                                 Mana Novice
 *                                                 |   Mana Expert
 *                                                 |   |   Mana Master
 *                                                 |   |   |   Mana Grandmaster
 *                                                 |   |   |   |     Recovery Novice
 *                                                 |   |   |   |     |     Recovery Expert
 *                                                 |   |   |   |     |     |     Recovery Master
 *                                                 |   |   |   |     |     |     |     Recovery Grandmaster
 *                                                 |   |   |   |     |     |     |     |   Base Damage
 *                                                 |   |   |   |     |     |     |     |   |   Bonus Skill Damage
 *                                                 |   |   |   |     |     |     |     |   |   |  Flags
 *                                                 |   |   |   |     |     |     |     |   |   |  |  Required skill mastery
 *                                                 |   |   |   |     |     |     |     |   |   |  |  |
 */
IndexedArray<SpellData, SPELL_FIRST_REGULAR, SPELL_LAST_REGULAR> pSpellDatas = {
    {SPELL_FIRE_TORCH_LIGHT,            SpellData( 1,  1,  1,  1,   60,   60,   60,   40,  0,  0, 0, MASTERY_NOVICE)},
    {SPELL_FIRE_FIRE_BOLT,              SpellData( 2,  2,  2,  2,  110,  110,  100,   90,  0,  3, 0, MASTERY_NOVICE)},
    {SPELL_FIRE_PROTECTION_FROM_FIRE,   SpellData( 3,  3,  3,  3,  120,  120,  120,  120,  0,  0, 0, MASTERY_NOVICE)},
    {SPELL_FIRE_FIRE_AURA,              SpellData( 4,  4,  4,  4,  120,  120,  120,  120,  0,  0, 0, MASTERY_NOVICE)},
    {SPELL_FIRE_HASTE,                  SpellData( 5,  5,  5,  5,  120,  120,  120,  120,  0,  0, 0, MASTERY_EXPERT)},
    {SPELL_FIRE_FIREBALL,               SpellData( 8 , 8,  8,  8,  100,  100,   90,   80,  0,  6, 0, MASTERY_EXPERT)},
    {SPELL_FIRE_FIRE_SPIKE,             SpellData(10, 10, 10, 10,  150,  150,  150,  150,  0,  6, 0, MASTERY_EXPERT)},
    {SPELL_FIRE_IMMOLATION,             SpellData(15, 15, 15, 15,  120,  120,  120,  120,  0,  6, 0, MASTERY_MASTER)},
    {SPELL_FIRE_METEOR_SHOWER,          SpellData(20, 20, 20, 20,  100,  100,  100,   90,  0,  8, 0, MASTERY_MASTER)},
    {SPELL_FIRE_INFERNO,                SpellData(25, 25, 25, 25,  100,  100,  100,   90, 12,  1, 0, MASTERY_MASTER)},
    {SPELL_FIRE_INCINERATE,             SpellData(30, 30, 30, 30,   90,   90,   90,   90, 15, 15, 0, MASTERY_GRANDMASTER)},

    {SPELL_AIR_WIZARD_EYE,              SpellData( 1,  1,  1,  0,   60,   60,   60,   60,  0,  0, 0, MASTERY_NOVICE)},
    {SPELL_AIR_FEATHER_FALL,            SpellData( 2,  2,  2,  2,  120,  120,  120,  100,  0,  0, 0, MASTERY_NOVICE)},
    {SPELL_AIR_PROTECTION_FROM_AIR,     SpellData( 3,  3,  3,  3,  120,  120,  120,  120,  0,  0, 0, MASTERY_NOVICE)},
    {SPELL_AIR_SPARKS,                  SpellData( 4,  4,  4,  4,  110,  100,   90,   80,  2,  1, 0, MASTERY_NOVICE)},
    {SPELL_AIR_JUMP,                    SpellData( 5,  5,  5,  5,   90,   90,   70,   50,  0,  0, 0, MASTERY_EXPERT)},
    {SPELL_AIR_SHIELD,                  SpellData( 8,  8,  8,  8,  120,  120,  120,  120,  0,  0, 0, MASTERY_EXPERT)},
    {SPELL_AIR_LIGHTNING_BOLT,          SpellData(10, 10, 10, 10,  100,  100,   90,   70,  0,  8, 0, MASTERY_EXPERT)},
    {SPELL_AIR_INVISIBILITY,            SpellData(15, 15, 15, 15,  200,  200,  200,  200,  0,  0, 0, MASTERY_MASTER)},
    {SPELL_AIR_IMPLOSION,               SpellData(20, 20, 20, 20,  100,  100,  100,   90, 10, 10, 0, MASTERY_MASTER)},
    {SPELL_AIR_FLY,                     SpellData(25, 25, 25, 25,  250,  250,  250,  250,  0,  0, 0, MASTERY_MASTER)},
    {SPELL_AIR_STARBURST,               SpellData(30, 30, 30, 30,   90,   90,   90,   90, 20,  1, 0, MASTERY_GRANDMASTER)},

    {SPELL_WATER_AWAKEN,                SpellData( 1,  1,  1,  1,   60,   60,   60,   20,  0,  0, 0, MASTERY_NOVICE)},
    {SPELL_WATER_POISON_SPRAY,          SpellData( 2,  2,  2,  2,  110,  100,   90,   70,  2,  2, 0, MASTERY_NOVICE)},
    {SPELL_WATER_PROTECTION_FROM_WATER, SpellData( 3,  3,  3,  3,  120,  120,  120,  120,  0,  0, 0, MASTERY_NOVICE)},
    {SPELL_WATER_ICE_BOLT,              SpellData( 4,  4,  4,  4,  110,  100,   90,   80,  0,  4, 0, MASTERY_NOVICE)},
    {SPELL_WATER_WATER_WALK,            SpellData( 5,  5,  5,  5,  150,  150,  150,  150,  0,  0, 0, MASTERY_EXPERT)},
    {SPELL_WATER_RECHARGE_ITEM,         SpellData( 8,  8,  8,  8,  200,  200,  200,  200,  0,  0, 0, MASTERY_EXPERT)},
    {SPELL_WATER_ACID_BURST,            SpellData(10, 10, 10, 10,  100,  100,   90,   80,  9,  9, 0, MASTERY_EXPERT)},
    {SPELL_WATER_ENCHANT_ITEM,          SpellData(15, 15, 15, 15,  140,  140,  140,  140,  0,  0, 0, MASTERY_MASTER)},
    {SPELL_WATER_TOWN_PORTAL,           SpellData(20, 20, 20, 20,  200,  200,  200,  200,  0,  0, 0, MASTERY_MASTER)},
    {SPELL_WATER_ICE_BLAST,             SpellData(25, 25, 25, 25,   80,   80,   80,   80, 12,  3, 0, MASTERY_MASTER)},
    {SPELL_WATER_LLOYDS_BEACON,         SpellData(30, 30, 30, 30,  250,  250,  250,  250,  0,  0, 0, MASTERY_GRANDMASTER)},

    {SPELL_EARTH_STUN,                  SpellData( 1,  1,  1,  1,   80,   80,   80,   80,  0,  0, 0, MASTERY_NOVICE)},
    {SPELL_EARTH_SLOW,                  SpellData( 2,  2,  2,  2,  100,  100,  100,  100,  0,  0, 0, MASTERY_NOVICE)},
    {SPELL_EARTH_PROTECTION_FROM_EARTH, SpellData( 3,  3,  3,  3,  120,  120,  120,  120,  0,  0, 0, MASTERY_NOVICE)},
    {SPELL_EARTH_DEADLY_SWARM,          SpellData( 4,  4,  4,  4,  110,  100,   90,   80,  5,  3, 0, MASTERY_NOVICE)},
    {SPELL_EARTH_STONESKIN,             SpellData( 5,  5,  5,  5,  120,  120,  120,  120,  0,  0, 0, MASTERY_EXPERT)},
    {SPELL_EARTH_BLADES,                SpellData( 8,  8,  8,  8,  100,  100,   90,   80,  0,  9, 0, MASTERY_EXPERT)},
    {SPELL_EARTH_STONE_TO_FLESH,        SpellData(10, 10, 10, 10,  140,  140,  140,  140,  0,  0, 0, MASTERY_EXPERT)},
    {SPELL_EARTH_ROCK_BLAST,            SpellData(15, 15, 15, 15,   90,   90,   90,   80,  0,  8, 0, MASTERY_MASTER)},
    {SPELL_EARTH_TELEKINESIS,           SpellData(20, 20, 20, 20,  150,  150,  150,  150,  0,  0, 0, MASTERY_MASTER)},
    {SPELL_EARTH_DEATH_BLOSSOM,         SpellData(25, 25, 25, 25,  100,  100,  100,   90, 20,  1, 0, MASTERY_MASTER)},
    {SPELL_EARTH_MASS_DISTORTION,       SpellData(30, 30, 30, 30,   90,   90,   90,   90, 25,  2, 0, MASTERY_GRANDMASTER)},

    {SPELL_SPIRIT_DETECT_LIFE,          SpellData( 1,  1,  1,  1,  100,  100,  100,  100,  0,  0, 0, MASTERY_NOVICE)},
    {SPELL_SPIRIT_BLESS,                SpellData( 2,  2,  2,  2,  100,  100,  100,  100,  0,  0, 0, MASTERY_NOVICE)},
    {SPELL_SPIRIT_FATE,                 SpellData( 3,  3,  3,  3,   90,   90,   90,   90,  0,  0, 0, MASTERY_NOVICE)},
    {SPELL_SPIRIT_TURN_UNDEAD,          SpellData( 4,  4,  4,  4,  120,  120,  120,  120,  0,  0, 0, MASTERY_NOVICE)},
    {SPELL_SPIRIT_REMOVE_CURSE,         SpellData( 5,  5,  5,  5,  120,  120,  120,  120,  0,  0, 0, MASTERY_EXPERT)},
    {SPELL_SPIRIT_PRESERVATION,         SpellData( 8,  8,  8,  8,  120,  120,  120,  120,  0,  0, 0, MASTERY_EXPERT)},
    {SPELL_SPIRIT_HEROISM,              SpellData(10, 10, 10, 10,  120,  120,  120,  120,  0,  0, 0, MASTERY_EXPERT)},
    {SPELL_SPIRIT_SPIRIT_LASH,          SpellData(15, 15, 15, 15,  100,  100,  100,  100, 10,  8, 0, MASTERY_MASTER)},
    {SPELL_SPIRIT_RAISE_DEAD,           SpellData(20, 20, 20, 20,  240,  240,  240,  240,  0,  0, 0, MASTERY_MASTER)},
    {SPELL_SPIRIT_SHARED_LIFE,          SpellData(25, 25, 25, 25,  150,  150,  150,  150,  0,  0, 0, MASTERY_MASTER)},
    {SPELL_SPIRIT_RESSURECTION,         SpellData(30, 30, 30, 30, 1000, 1000, 1000, 1000,  0,  0, 0, MASTERY_GRANDMASTER)},

    {SPELL_MIND_REMOVE_FEAR,            SpellData( 1,  1,  1,  1,  120,  120,  120,  120,  0,  0, 0, MASTERY_NOVICE)},
    {SPELL_MIND_MIND_BLAST,             SpellData( 2,  2,  2,  2,  110,  110,  110,  110,  3,  3, 0, MASTERY_NOVICE)},
    {SPELL_MIND_PROTECTION_FROM_MIND,   SpellData( 3,  3,  3,  3,  120,  120,  120,  120,  0,  0, 0, MASTERY_NOVICE)},
    {SPELL_MIND_TELEPATHY,              SpellData( 4,  4,  4,  4,  110,  100,   90,   80,  0,  0, 0, MASTERY_NOVICE)},
    {SPELL_MIND_CHARM,                  SpellData( 5,  5,  5,  5,  100,  100,  100,  100,  0,  0, 0, MASTERY_EXPERT)},
    {SPELL_MIND_CURE_PARALYSIS,         SpellData( 8,  8,  8,  8,  120,  120,  120,  120,  0,  0, 0, MASTERY_EXPERT)},
    {SPELL_MIND_BERSERK,                SpellData(10, 10, 10, 10,  120,  120,  120,  120,  0,  0, 0, MASTERY_EXPERT)},
    {SPELL_MIND_MASS_FEAR,              SpellData(15, 15, 15, 15,   80,   80,   80,   80,  0,  0, 0, MASTERY_MASTER)},
    {SPELL_MIND_CURE_INSANITY,          SpellData(20, 20, 20, 20,  120,  120,  120,  120,  0,  0, 0, MASTERY_MASTER)},
    {SPELL_MIND_PSYCHIC_SHOCK,          SpellData(25, 25, 25, 25,  110,  110,  110,  100, 12, 12, 0, MASTERY_MASTER)},
    {SPELL_MIND_ENSLAVE,                SpellData(30, 30, 30, 30,  120,  120,  120,  120,  0,  0, 0, MASTERY_GRANDMASTER)},

    {SPELL_BODY_CURE_WEAKNESS,          SpellData( 1,  1,  1,  1,  120,  120,  120,  120,  0,  0, 0, MASTERY_NOVICE)},
    {SPELL_BODY_FIRST_AID,              SpellData( 2,  2,  2,  2,  100,  100,  100,  100,  0,  0, 0, MASTERY_NOVICE)},
    {SPELL_BODY_PROTECTION_FROM_BODY,   SpellData( 3,  3,  3,  3,  120,  120,  120,  120,  0,  0, 0, MASTERY_NOVICE)},
    {SPELL_BODY_HARM,                   SpellData( 4,  4,  4,  4,  110,  100,   90,   80,  8,  2, 0, MASTERY_NOVICE)},
    {SPELL_BODY_REGENERATION,           SpellData( 5,  5,  5,  5,  110,  110,  110,  110,  0,  0, 0, MASTERY_EXPERT)},
    {SPELL_BODY_CURE_POISON,            SpellData( 8,  8,  8,  8,  120,  120,  120,  120,  0,  0, 0, MASTERY_EXPERT)},
    {SPELL_BODY_HAMMERHANDS,            SpellData(10, 10, 10, 10,  120,  120,  120,  120,  0,  0, 0, MASTERY_EXPERT)},
    {SPELL_BODY_CURE_DISEASE,           SpellData(15, 15, 15, 15,  120,  120,  120,  120,  0,  0, 0, MASTERY_MASTER)},
    {SPELL_BODY_PROTECTION_FROM_MAGIC,  SpellData(20, 20, 20, 20,  120,  120,  120,  120,  0,  0, 0, MASTERY_MASTER)},
    {SPELL_BODY_FLYING_FIST,            SpellData(25, 25, 25, 25,  110,  110,  110,  100, 30,  5, 0, MASTERY_MASTER)},
    {SPELL_BODY_POWER_CURE,             SpellData(30, 30, 30, 30,  100,  100,  100,  100,  0,  0, 0, MASTERY_GRANDMASTER)},

    {SPELL_LIGHT_LIGHT_BOLT,            SpellData( 5,  5,  5,  5,  110,  100,   90,   80,  0,  4, 0, MASTERY_NOVICE)},
    {SPELL_LIGHT_DESTROY_UNDEAD,        SpellData(10, 10, 10, 10,  120,  110,  100,   90, 16, 16, 0, MASTERY_NOVICE)},
    {SPELL_LIGHT_DISPEL_MAGIC,          SpellData(15, 15, 15, 15,  120,  110,  100,   90,  0,  0, 0, MASTERY_NOVICE)},
    {SPELL_LIGHT_PARALYZE,              SpellData(20, 20, 20, 20,  160,  140,  120,  100,  0,  0, 0, MASTERY_NOVICE)},
    {SPELL_LIGHT_SUMMON_ELEMENTAL,      SpellData(25, 25, 25, 25,  140,  140,  140,  140,  0,  0, 0, MASTERY_EXPERT)},
    {SPELL_LIGHT_DAY_OF_THE_GODS,       SpellData(30, 30, 30, 30,  500,  500,  500,  500,  0,  0, 0, MASTERY_EXPERT)},
    {SPELL_LIGHT_PRISMATIC_LIGHT,       SpellData(35, 35, 35, 35,  135,  135,  120,  100, 25,  1, 0, MASTERY_EXPERT)},
    {SPELL_LIGHT_DAY_OF_PROTECTION,     SpellData(40, 40, 40, 40,  500,  500,  500,  500,  0,  0, 0, MASTERY_MASTER)},
    {SPELL_LIGHT_HOUR_OF_POWER,         SpellData(45, 45, 45, 45,  250,  250,  250,  250,  0,  0, 0, MASTERY_MASTER)},
    {SPELL_LIGHT_SUNRAY,                SpellData(50, 50, 50, 50,  150,  150,  150,  135, 20, 20, 0, MASTERY_MASTER)},
    {SPELL_LIGHT_DIVINE_INTERVENTION,   SpellData(55, 55, 55, 55,  300,  300,  300,  300,  0,  0, 0, MASTERY_GRANDMASTER)},

    {SPELL_DARK_REANIMATE,              SpellData(10, 10, 10, 10,  140,  140,  140,  140,  0,  0, 0, MASTERY_NOVICE)},
    {SPELL_DARK_TOXIC_CLOUD,            SpellData(15, 15, 15, 15,  120,  110,  100,   90, 25, 10, 0, MASTERY_NOVICE)},
    {SPELL_DARK_VAMPIRIC_WEAPON,        SpellData(20, 20, 20, 20,  120,  100,   90,  120,  0,  0, 0, MASTERY_NOVICE)},
    {SPELL_DARK_SHRINKING_RAY,          SpellData(25, 25, 25, 25,  120,  120,  120,  120,  0,  0, 0, MASTERY_NOVICE)},
    {SPELL_DARK_SHARPMETAL,             SpellData(30, 30, 30, 30,   90,   90,   80,   70,  6,  6, 0, MASTERY_EXPERT)},
    {SPELL_DARK_CONTROL_UNDEAD,         SpellData(35, 35, 35, 35,  120,  120,  100,   80,  0,  0, 0, MASTERY_EXPERT)},
    {SPELL_DARK_PAIN_REFLECTION,        SpellData(40, 40, 40, 40,  110,  110,  110,  110,  0,  0, 0, MASTERY_EXPERT)},
    {SPELL_DARK_SACRIFICE,              SpellData(45, 45, 45, 45,  200,  200,  200,  150,  0,  0, 0, MASTERY_MASTER)},
    {SPELL_DARK_DRAGON_BREATH,          SpellData(50, 50, 50, 50,  120,  120,  120,  100,  0, 25, 0, MASTERY_MASTER)},
    {SPELL_DARK_ARMAGEDDON,             SpellData(55, 55, 55, 55,  250,  250,  250,  250, 50,  1, 0, MASTERY_MASTER)},
    {SPELL_DARK_SOULDRINKER,            SpellData(60, 60, 60, 60,  300,  300,  300,  300, 25,  8, 0, MASTERY_GRANDMASTER)}
};

namespace {

/** Per-mastery MM6 spell point cost / recovery time and MM6 damage numbers for a single spell. See `applyMm6SpellDatas`. */
struct Mm6SpellData {
    std::array<uint16_t, 3> mana;      // [Novice, Expert, Master] spell points.
    std::array<uint16_t, 3> recovery;  // [Novice, Expert, Master] recovery time, in game ticks.
    uint8_t baseDamage = 0;            // Flat damage added once.
    uint8_t skillDiceSides = 0;        // One d(sides) die rolled per point of skill.
};

/**
 * MM6 per-mastery spell point costs, recovery times and damage numbers. Mana and recovery are extracted
 * verbatim from MM6.EXE's SpellInfo table (MMExtension `Game.Spells`, VA 0x4BDD70; 99 entries of 0xE
 * bytes each: `SpellPoints[3]` int16 @0x0, `Delay[3]` int16 @0x6, `Bits` uint16 @0xC). MM6 has only
 * three skill masteries (Novice/Expert/Master), so there is no Grandmaster tier in the source data.
 *
 * The damage numbers come from MM6.EXE's own CalcSpellDamage (VA 0x47F0A0, MMExtension hook name;
 * a jump table over spell ids 2..99): damage = baseDamage + one d(skillDiceSides) die per point of
 * skill, INDEPENDENT of mastery. `skillDiceSides == 1` is a deterministic "+1 per point of skill"
 * (matching e.g. "does six points of damage plus one per point of skill" in spells.txt). Spells whose
 * EXE formula doesn't fit this shape (the flat-dice arrows, Acid Burst's 0-based die, Mass Distortion's
 * percent-of-hp) carry 0 here and are special-cased in `CalcSpellDamage`. Every value cross-checks
 * against the MM6 spells.txt damage descriptions except Acid Burst (see `CalcSpellDamage`). Native id
 * 96 (Moon Ray) is the one spells.txt-sourced entry: the EXE computes its all-in-sight damage/heal at
 * the cast site, not in CalcSpellDamage, but the engine routes the analog's impact through this table.
 *
 * The keys are MM7 `SpellId` names, but they denote the *native* MM6 spell in that id slot (which is
 * how `pSpellDatas` is indexed). The MM6 spell at a given id is often a different spell than the MM7
 * name suggests (e.g. native id 2 is "Flame Arrow" in MM6, not Fire Bolt) - only the stats matter.
 *
 * Cross-validated: decoding MM7.EXE's SpellInfo table (VA 0x4E3C48, 0x14-byte entries) with the same
 * reader reproduces the mana and recovery of all 99 `pSpellDatas` rows exactly. The table is
 * regenerable from the VA + struct layout above (dumpers kept in docs/scratch/dump_spelldata.py and
 * docs/scratch/dump_calcdmg.py during development).
 */
const IndexedArray<Mm6SpellData, SPELL_FIRST_REGULAR, SPELL_LAST_REGULAR> kMm6SpellDatas = {
    {SPELL_FIRE_TORCH_LIGHT,             {{   1,   1,   1}, {   60,   60,   60}}},
    {SPELL_FIRE_FIRE_BOLT,               {{   2,   1,   0}, {  100,   90,   80}}},
    {SPELL_FIRE_PROTECTION_FROM_FIRE,    {{   3,   3,   3}, {  120,  120,  120}}},
    {SPELL_FIRE_FIRE_AURA,               {{   4,   4,   4}, {  110,  100,   90},  0,  4}},  // MM6 Fire Bolt: 1-4 per skill.
    {SPELL_FIRE_HASTE,                   {{   5,   5,   5}, {  120,  120,  120}}},
    {SPELL_FIRE_FIREBALL,                {{   8,   8,   8}, {  110,  100,   90},  0,  6}},  // Fireball: 1-6 per skill.
    {SPELL_FIRE_FIRE_SPIKE,              {{  10,  10,  10}, {  110,  100,   90},  6,  1}},  // MM6 Ring of Fire: 6 + skill.
    {SPELL_FIRE_IMMOLATION,              {{  15,  15,  15}, {  110,   90,   70},  4,  3}},  // MM6 Fire Blast: 4 + 1-3 per skill.
    {SPELL_FIRE_METEOR_SHOWER,           {{  20,  20,  20}, {  120,  110,  100},  8,  1}},  // Meteor Shower: 8 + skill per meteor.
    {SPELL_FIRE_INFERNO,                 {{  25,  25,  25}, {  140,  120,  100}, 12,  1}},  // Inferno: 12 + skill.
    {SPELL_FIRE_INCINERATE,              {{  30,  30,  30}, {  150,  130,  110}, 15, 15}},  // Incinerate: 15 + 1-15 per skill.

    {SPELL_AIR_WIZARD_EYE,               {{   1,   1,   1}, {   60,   60,   60}}},
    {SPELL_AIR_FEATHER_FALL,             {{   2,   2,   0}, {  100,   90,   90}}},
    {SPELL_AIR_PROTECTION_FROM_AIR,      {{   3,   3,   3}, {  120,  120,  120}}},
    {SPELL_AIR_SPARKS,                   {{   4,   4,   4}, {  110,  100,   90},  2,  1}},  // Sparks: 2 + skill per spark.
    {SPELL_AIR_JUMP,                     {{   5,   5,   5}, {  120,  120,  120}}},
    {SPELL_AIR_SHIELD,                   {{   8,   8,   8}, {  120,  120,  120}}},
    {SPELL_AIR_LIGHTNING_BOLT,           {{  10,  10,  10}, {  110,  100,   90},  0,  8}},  // Lightning Bolt: 1-8 per skill.
    {SPELL_AIR_INVISIBILITY,             {{  15,  15,  15}, {  110,   90,   70}}},
    {SPELL_AIR_IMPLOSION,                {{  20,  20,  20}, {  120,  110,  100}, 10, 10}},  // Implosion: 10 + 1-10 per skill.
    {SPELL_AIR_FLY,                      {{  25,  25,  25}, {  250,  250,  250}}},
    {SPELL_AIR_STARBURST,                {{  30,  30,  30}, {  150,  130,  110}, 20,  1}},  // Starburst: 20 + skill per star.

    {SPELL_WATER_AWAKEN,                 {{   1,   1,   1}, {   60,   60,   60}}},
    {SPELL_WATER_POISON_SPRAY,           {{   2,   1,   0}, {   90,   80,   80}}},
    {SPELL_WATER_PROTECTION_FROM_WATER,  {{   3,   3,   3}, {  120,  120,  120}}},
    {SPELL_WATER_ICE_BOLT,               {{   4,   4,   4}, {  110,  100,   90},  2,  2}},  // MM6 Poison Spray: 2 + 1-2 per skill.
    {SPELL_WATER_WATER_WALK,             {{   5,   5,   5}, {  150,  120,  120}}},
    {SPELL_WATER_RECHARGE_ITEM,          {{   8,   8,   8}, {  110,  100,   90},  0,  7}},  // MM6 Ice Bolt: 1-7 per skill.
    {SPELL_WATER_ACID_BURST,             {{  10,  10,  10}, {  140,  140,  140}}},
    {SPELL_WATER_ENCHANT_ITEM,           {{  15,  15,  15}, {  110,  100,   90}}},          // MM6 Acid Burst: special-cased.
    {SPELL_WATER_TOWN_PORTAL,            {{  20,  20,  20}, {  200,  200,  200}}},
    {SPELL_WATER_ICE_BLAST,              {{  25,  25,  25}, {  120,  100,   80}, 12,  2}},  // Ice Blast: 12 + 1-2 per skill per shard.
    {SPELL_WATER_LLOYDS_BEACON,          {{  30,  30,  30}, {  250,  250,  250}}},

    {SPELL_EARTH_STUN,                   {{   1,   1,   1}, {   80,   80,   80}}},
    {SPELL_EARTH_SLOW,                   {{   2,   1,   0}, {  100,   90,   80}}},
    {SPELL_EARTH_PROTECTION_FROM_EARTH,  {{   3,   3,   3}, {  120,  120,  120}}},
    {SPELL_EARTH_DEADLY_SWARM,           {{   4,   4,   4}, {  110,  100,   90},  5,  3}},  // Deadly Swarm: 5 + 1-3 per skill.
    {SPELL_EARTH_STONESKIN,              {{   5,   5,   5}, {  120,  120,  120}}},
    {SPELL_EARTH_BLADES,                 {{   8,   8,   8}, {  110,  100,   90},  0,  5}},  // Blades: 1-5 per skill.
    {SPELL_EARTH_STONE_TO_FLESH,         {{  10,  10,  10}, {  140,  140,  140}}},
    {SPELL_EARTH_ROCK_BLAST,             {{  15,  15,  15}, {  110,  100,   90},  0,  8}},  // Rock Blast: 1-8 per skill.
    {SPELL_EARTH_TELEKINESIS,            {{  20,  20,  20}, {  130,  130,  130}}},
    {SPELL_EARTH_DEATH_BLOSSOM,          {{  25,  25,  25}, {  120,  110,  100}, 20,  1}},  // Death Blossom: 20 + skill.
    {SPELL_EARTH_MASS_DISTORTION,        {{  30,  30,  30}, {  140,  120,  100}}},

    {SPELL_SPIRIT_DETECT_LIFE,           {{   1,   1,   0}, {   90,   80,   80}}},
    {SPELL_SPIRIT_BLESS,                 {{   2,   2,   2}, {  100,  100,  100}}},
    {SPELL_SPIRIT_FATE,                  {{   3,   3,   3}, {  100,  100,  100}}},
    {SPELL_SPIRIT_TURN_UNDEAD,           {{   4,   4,   4}, {  120,  120,  120}}},
    {SPELL_SPIRIT_REMOVE_CURSE,          {{   5,   5,   5}, {  120,  120,  120}}},
    {SPELL_SPIRIT_PRESERVATION,          {{   8,   8,   8}, {  120,  120,  120}}},
    {SPELL_SPIRIT_HEROISM,               {{  10,  10,  10}, {  120,  120,  120}}},
    {SPELL_SPIRIT_SPIRIT_LASH,           {{  15,  15,  15}, {  140,  120,  100}}},
    {SPELL_SPIRIT_RAISE_DEAD,            {{  20,  20,  20}, {  240,  240,  240}}},
    {SPELL_SPIRIT_SHARED_LIFE,           {{  25,  25,  25}, {  150,  150,  150}}},
    {SPELL_SPIRIT_RESSURECTION,          {{  30,  30,  30}, { 1000, 1000, 1000}}},

    {SPELL_MIND_REMOVE_FEAR,             {{   1,   1,   1}, {  120,  120,  120}}},
    {SPELL_MIND_MIND_BLAST,              {{   2,   2,   2}, {  120,  120,  120}}},
    {SPELL_MIND_PROTECTION_FROM_MIND,    {{   3,   3,   3}, {  110,  100,   90},  5,  2}},  // MM6 Mind Blast: 5 + 1-2 per skill.
    {SPELL_MIND_TELEPATHY,               {{   4,   4,   4}, {  120,  120,  120}}},
    {SPELL_MIND_CHARM,                   {{   5,   5,   5}, {  120,  120,  120}}},
    {SPELL_MIND_CURE_PARALYSIS,          {{   8,   8,   8}, {  100,  100,  100}}},
    {SPELL_MIND_BERSERK,                 {{  10,  10,  10}, {  100,   90,   80}}},
    {SPELL_MIND_MASS_FEAR,               {{  15,  15,  15}, {  110,  100,   90}}},
    {SPELL_MIND_CURE_INSANITY,           {{  20,  20,  20}, {  120,  120,  120}}},
    {SPELL_MIND_PSYCHIC_SHOCK,           {{  25,  25,  25}, {  130,  120,  110}, 12, 12}},  // Psychic Shock: 12 + 1-12 per skill.
    {SPELL_MIND_ENSLAVE,                 {{  30,  30,  30}, {  250,  250,  250}}},

    {SPELL_BODY_CURE_WEAKNESS,           {{   1,   1,   1}, {  120,  120,  120}}},
    {SPELL_BODY_FIRST_AID,               {{   2,   2,   2}, {   80,   80,   80}}},
    {SPELL_BODY_PROTECTION_FROM_BODY,    {{   3,   3,   3}, {  120,  120,  120}}},
    {SPELL_BODY_HARM,                    {{   4,   4,   4}, {  110,  100,   90},  8,  2}},  // Harm: 8 + 1-2 per skill.
    {SPELL_BODY_REGENERATION,            {{   5,   5,   5}, {  100,   90,   80}}},
    {SPELL_BODY_CURE_POISON,             {{   8,   8,   8}, {  120,  120,  120}}},
    {SPELL_BODY_HAMMERHANDS,             {{  10,  10,  10}, {  120,  120,  120}}},
    {SPELL_BODY_CURE_DISEASE,            {{  15,  15,  15}, {  120,  120,  120}}},
    {SPELL_BODY_PROTECTION_FROM_MAGIC,   {{  20,  20,  20}, {  120,  120,  120}}},
    {SPELL_BODY_FLYING_FIST,             {{  25,  25,  25}, {  130,  120,  110}, 30,  5}},  // Flying Fist: 30 + 1-5 per skill.
    {SPELL_BODY_POWER_CURE,              {{  30,  30,  30}, {  150,  125,  100}}},

    {SPELL_LIGHT_LIGHT_BOLT,             {{  20,  20,  20}, {  100,  100,  100}}},
    {SPELL_LIGHT_DESTROY_UNDEAD,         {{  25,  25,  25}, {  100,  100,  100}}},
    {SPELL_LIGHT_DISPEL_MAGIC,           {{  30,  30,  30}, {  120,  110,  100}}},
    {SPELL_LIGHT_PARALYZE,               {{  35,  35,  35}, {  120,  100,   80}}},
    {SPELL_LIGHT_SUMMON_ELEMENTAL,       {{  40,  40,  40}, {  120,  110,  100}, 16, 16}},  // MM6 Destroy Undead: 16 + 1-16 per skill.
    {SPELL_LIGHT_DAY_OF_THE_GODS,        {{  45,  45,  45}, {  500,  500,  500}}},
    {SPELL_LIGHT_PRISMATIC_LIGHT,        {{  50,  50,  50}, {  150,  135,  120}, 25,  1}},  // Prismatic Light: 25 + skill.
    {SPELL_LIGHT_DAY_OF_PROTECTION,      {{  55,  55,  55}, {  250,  250,  250}}},
    {SPELL_LIGHT_HOUR_OF_POWER,          {{  60,  60,  60}, {  160,  140,  120}}},
    {SPELL_LIGHT_SUNRAY,                 {{  65,  65,  65}, {  180,  165,  150}, 20, 20}},  // MM6 Sun Ray: 20 + 1-20 per skill.
    {SPELL_LIGHT_DIVINE_INTERVENTION,    {{  70,  70,  70}, {  300,  300,  300}}},

    {SPELL_DARK_REANIMATE,               {{  20,  20,  20}, {  100,  100,  100}}},
    {SPELL_DARK_TOXIC_CLOUD,             {{  30,  30,  30}, {  120,  110,  100}, 25, 10}},  // Toxic Cloud: 25 + 1-10 per skill.
    {SPELL_DARK_VAMPIRIC_WEAPON,         {{  40,  40,  40}, {  120,  120,  120}}},
    {SPELL_DARK_SHRINKING_RAY,           {{  50,  50,  50}, {  100,   90,   80},  6,  6}},  // MM6 Shrapmetal: 6 + 1-6 per skill per piece.
    {SPELL_DARK_SHARPMETAL,              {{  60,  60,  60}, {  120,  120,  120}}},
    {SPELL_DARK_CONTROL_UNDEAD,          {{  70,  70,  70}, {  500,  500,  500}}},
    {SPELL_DARK_PAIN_REFLECTION,         {{  80,  80,  80}, {  130,  130,  130}}},
    {SPELL_DARK_SACRIFICE,               {{  90,  90,  90}, {  150,  140,  130},  0,  4}},  // MM6 Moon Ray: 1-4 per skill (spells.txt).
    {SPELL_DARK_DRAGON_BREATH,           {{ 100, 100, 100}, {  160,  140,  120},  0, 25}},  // Dragon Breath: 1-25 per skill.
    {SPELL_DARK_ARMAGEDDON,              {{ 150, 150, 150}, {  250,  250,  250}, 50,  1}},  // Armageddon: 50 + skill.
    {SPELL_DARK_SOULDRINKER,             {{ 200, 200, 200}, {  300,  300,  300}, 50,  1}}   // MM6 Dark Containment: 50 + skill.
};

} // namespace

void applyMm6SpellDatas() {
    for (SpellId spell : pSpellDatas.indices()) {
        const Mm6SpellData &cost = kMm6SpellDatas[spell];
        SpellData &data = pSpellDatas[spell];

        data.mana_per_skill[MASTERY_NOVICE] = cost.mana[0];
        data.mana_per_skill[MASTERY_EXPERT] = cost.mana[1];
        data.mana_per_skill[MASTERY_MASTER] = cost.mana[2];
        data.mana_per_skill[MASTERY_GRANDMASTER] = cost.mana[2];  // MM6 has no Grandmaster tier.

        data.recovery_per_skill[MASTERY_NOVICE] = Duration::fromTicks(cost.recovery[0]);
        data.recovery_per_skill[MASTERY_EXPERT] = Duration::fromTicks(cost.recovery[1]);
        data.recovery_per_skill[MASTERY_MASTER] = Duration::fromTicks(cost.recovery[2]);
        data.recovery_per_skill[MASTERY_GRANDMASTER] = Duration::fromTicks(cost.recovery[2]);

        // MM6's highest mastery is Master, so the 11th spell of each school (Grandmaster in MM7) must be
        // learnable at Master - otherwise the spellbook learn gate (Character.cpp, requiredMastery >
        // val.mastery()) leaves those 9 spells permanently unlearnable in MM6.
        if (data.skillMastery == MASTERY_GRANDMASTER)
            data.skillMastery = MASTERY_MASTER;

        // MM6's own damage numbers, keyed by the NATIVE spell id (see kMm6SpellDatas / CalcSpellDamage's
        // MM6 branch). flags are left unchanged (MM6's spell table carries no flags column).
        data.baseDamage = cost.baseDamage;
        data.bonusSkillDamage = cost.skillDiceSides;
    }
}

IndexedArray<std::array<struct SpellBookIconPos, 12>, MAGIC_SCHOOL_FIRST, MAGIC_SCHOOL_LAST> pIconPos = {
    {MAGIC_SCHOOL_FIRE, {{{0,   0},   {17,  13},  {115, 2},   {217, 15},  {299, 6},   {28,  125},
                          {130, 133}, {294, 114}, {11,  232}, {134, 233}, {237, 171}, {296, 231}}}},

    {MAGIC_SCHOOL_AIR, {{{0,   0},   {19,  9},   {117, 3},   {206, 13},  {285, 7},   {16,  123},
                         {113, 101}, {201, 118}, {317, 110}, {11,  230}, {149, 236}, {296, 234}}}},

    {MAGIC_SCHOOL_WATER, {{{0,  0},   {17,  9},   {140, 0},   {210, 34},  {293, 5},   {15,  98},
                           {78, 121}, {175, 136}, {301, 115}, {15,  226}, {154, 225}, {272, 220}}}},

    {MAGIC_SCHOOL_EARTH, {{{0,   0},   {7,   9},   {156, 2},   {277, 9},   {11,  117}, {111, 82},
                           {180, 102}, {303, 108}, {10,  229}, {120, 221}, {201, 217}, {296, 225}}}},

    {MAGIC_SCHOOL_SPIRIT, {{{0,   0},   {18,  8},   {89,  15},  {192, 14},  {292, 7},   {22,  129},
                            {125, 146}, {217, 136}, {305, 115}, {22,  226}, {174, 237}, {290, 231}}}},

    {MAGIC_SCHOOL_MIND, {{{0,   0},  {18,  12},  {148, 9},   {292, 7},   {17,  122}, {121, 99},
                          {220, 87}, {293, 112}, {13,  236}, {128, 213}, {220, 223}, {315, 223}}}},

    {MAGIC_SCHOOL_BODY, {{{0,   0},   {23,  14},  {127, 8},   {204, 0},   {306, 8},   {14,  115},
                          {122, 132}, {200, 116}, {293, 122}, {20,  228}, {154, 228}, {294, 239}}}},

    {MAGIC_SCHOOL_LIGHT, {{{0,   0},  {19,  14},  {124, 10},  {283, 12},  {8,   105}, {113, 89},
                           {190, 82}, {298, 108}, {18,  181}, {101, 204}, {204, 203}, {285, 218}}}},

    {MAGIC_SCHOOL_DARK, {{{0,   0},   {18,  17},  {110, 16},  {201, 15},  {307, 15},  {18,  148},
                          {125, 166}, {201, 123}, {275, 120}, {28,  235}, {217, 222}, {324, 216}}}}
};

// TODO: use SoundID not uint16_t
// TODO(captainurist): Originally the array was two elements shorter, last two zeros are my addition. Can we drop elements for non-regular spells?
const IndexedArray<uint16_t, SPELL_FIRST_WITH_SPRITE, SPELL_LAST_WITH_SPRITE> SpellSoundIds = {{
    {SPELL_FIRE_TORCH_LIGHT, 10000},
    {SPELL_FIRE_FIRE_BOLT, 10010},
    {SPELL_FIRE_PROTECTION_FROM_FIRE, 10020},
    {SPELL_FIRE_FIRE_AURA, 10030},
    {SPELL_FIRE_HASTE, 10040},
    {SPELL_FIRE_FIREBALL, 10050},
    {SPELL_FIRE_FIRE_SPIKE, 10060},
    {SPELL_FIRE_IMMOLATION, 10070},
    {SPELL_FIRE_METEOR_SHOWER, 10080},
    {SPELL_FIRE_INFERNO, 10090},
    {SPELL_FIRE_INCINERATE, 10100},
    {SPELL_AIR_WIZARD_EYE, 11000},
    {SPELL_AIR_FEATHER_FALL, 11010},
    {SPELL_AIR_PROTECTION_FROM_AIR, 11020},
    {SPELL_AIR_SPARKS, 11030},
    {SPELL_AIR_JUMP, 11040},
    {SPELL_AIR_SHIELD, 11050},
    {SPELL_AIR_LIGHTNING_BOLT, 11060},
    {SPELL_AIR_INVISIBILITY, 11070},
    {SPELL_AIR_IMPLOSION, 11080},
    {SPELL_AIR_FLY, 11090},
    {SPELL_AIR_STARBURST, 11100},
    {SPELL_WATER_AWAKEN, 12000},
    {SPELL_WATER_POISON_SPRAY, 12010},
    {SPELL_WATER_PROTECTION_FROM_WATER, 12020},
    {SPELL_WATER_ICE_BOLT, 12030},
    {SPELL_WATER_WATER_WALK, 12040},
    {SPELL_WATER_RECHARGE_ITEM, 12050},
    {SPELL_WATER_ACID_BURST, 12060},
    {SPELL_WATER_ENCHANT_ITEM, 12070},
    {SPELL_WATER_TOWN_PORTAL, 12080},
    {SPELL_WATER_ICE_BLAST, 12090},
    {SPELL_WATER_LLOYDS_BEACON, 12100},
    {SPELL_EARTH_STUN, 13000},
    {SPELL_EARTH_SLOW, 13010},
    {SPELL_EARTH_PROTECTION_FROM_EARTH, 13020},
    {SPELL_EARTH_DEADLY_SWARM, 13030},
    {SPELL_EARTH_STONESKIN, 13040},
    {SPELL_EARTH_BLADES, 13050},
    {SPELL_EARTH_STONE_TO_FLESH, 13060},
    {SPELL_EARTH_ROCK_BLAST, 13070},
    {SPELL_EARTH_TELEKINESIS, 13080},
    {SPELL_EARTH_DEATH_BLOSSOM, 13090},
    {SPELL_EARTH_MASS_DISTORTION, 13100},
    {SPELL_SPIRIT_DETECT_LIFE, 14000},
    {SPELL_SPIRIT_BLESS, 14010},
    {SPELL_SPIRIT_FATE, 14020},
    {SPELL_SPIRIT_TURN_UNDEAD, 14030},
    {SPELL_SPIRIT_REMOVE_CURSE, 14040},
    {SPELL_SPIRIT_PRESERVATION, 14050},
    {SPELL_SPIRIT_HEROISM, 14060},
    {SPELL_SPIRIT_SPIRIT_LASH, 14070},
    {SPELL_SPIRIT_RAISE_DEAD, 14080},
    {SPELL_SPIRIT_SHARED_LIFE, 14090},
    {SPELL_SPIRIT_RESSURECTION, 14100},
    {SPELL_MIND_REMOVE_FEAR, 15000},
    {SPELL_MIND_MIND_BLAST, 15010},
    {SPELL_MIND_PROTECTION_FROM_MIND, 15020},
    {SPELL_MIND_TELEPATHY, 15030},
    {SPELL_MIND_CHARM, 15040},
    {SPELL_MIND_CURE_PARALYSIS, 15050},
    {SPELL_MIND_BERSERK, 15060},
    {SPELL_MIND_MASS_FEAR, 15070},
    {SPELL_MIND_CURE_INSANITY, 15080},
    {SPELL_MIND_PSYCHIC_SHOCK, 15090},
    {SPELL_MIND_ENSLAVE, 15100},
    {SPELL_BODY_CURE_WEAKNESS, 16000},
    {SPELL_BODY_FIRST_AID, 16010},
    {SPELL_BODY_PROTECTION_FROM_BODY, 16020},
    {SPELL_BODY_HARM, 16030},
    {SPELL_BODY_REGENERATION, 16040},
    {SPELL_BODY_CURE_POISON, 16050},
    {SPELL_BODY_HAMMERHANDS, 16060},
    {SPELL_BODY_CURE_DISEASE, 16070},
    {SPELL_BODY_PROTECTION_FROM_MAGIC, 16080},
    {SPELL_BODY_FLYING_FIST, 16090},
    {SPELL_BODY_POWER_CURE, 16100},
    {SPELL_LIGHT_LIGHT_BOLT, 17000},
    {SPELL_LIGHT_DESTROY_UNDEAD, 17010},
    {SPELL_LIGHT_DISPEL_MAGIC, 17020},
    {SPELL_LIGHT_PARALYZE, 17030},
    {SPELL_LIGHT_SUMMON_ELEMENTAL, 17040},
    {SPELL_LIGHT_DAY_OF_THE_GODS, 17050},
    {SPELL_LIGHT_PRISMATIC_LIGHT, 17060},
    {SPELL_LIGHT_DAY_OF_PROTECTION, 17070},
    {SPELL_LIGHT_HOUR_OF_POWER, 17080},
    {SPELL_LIGHT_SUNRAY, 17090},
    {SPELL_LIGHT_DIVINE_INTERVENTION, 17100},
    {SPELL_DARK_REANIMATE, 18000},
    {SPELL_DARK_TOXIC_CLOUD, 18010},
    {SPELL_DARK_VAMPIRIC_WEAPON, 18020},
    {SPELL_DARK_SHRINKING_RAY, 18030},
    {SPELL_DARK_SHARPMETAL, 18040},
    {SPELL_DARK_CONTROL_UNDEAD, 18050},
    {SPELL_DARK_PAIN_REFLECTION, 18060},
    {SPELL_DARK_SACRIFICE, 18070},
    {SPELL_DARK_DRAGON_BREATH, 18080},
    {SPELL_DARK_ARMAGEDDON, 18090},
    {SPELL_DARK_SOULDRINKER, 18100},
    {SPELL_BOW_ARROW, 00001},
    {SPELL_101, 00000},
    {SPELL_LASER_PROJECTILE, 00000}
}};

// Frees the pOverlays slot owned by a buff, if any. overlayId is not guaranteed to be a valid
// 1-based slot index: savegames carry the field verbatim, and old Hammerhands casts stored the
// caster's skill level in it - drop out-of-range ids instead of indexing past pOverlays[50].
static void freeBuffOverlaySlot(uint16_t *overlayId) {
    if (*overlayId) {
        if (*overlayId <= pActiveOverlayList->pOverlays.size())
            pActiveOverlayList->pOverlays[*overlayId - 1].Reset();
        *overlayId = 0;
    }
}

void SpellBuff::Reset() {
    skillMastery = MASTERY_NONE;
    power = 0;
    expireTime = Time();
    caster = 0;
    isGM = false;
    freeBuffOverlaySlot(&overlayId);
}

bool SpellBuff::IsBuffExpiredToTime(Time time) {
    if (this->expireTime && (this->expireTime < time)) {
        expireTime.SetExpired();
        power = 0;
        skillMastery = MASTERY_NONE;
        // Free the owned overlay slot like Reset() does - just zeroing the id would leave a buff-owned
        // (never-expiring) ActiveOverlay on screen forever when the buff runs out on its own.
        freeBuffOverlaySlot(&overlayId);
        return true;
    }
    return false;
}

bool SpellBuff::Apply(Time expire_time, Mastery uSkillMastery,
                      int uPower, int uOverlayID,
                      uint8_t caster) {
    // For bug catching
    assert(uSkillMastery >= MASTERY_NOVICE && uSkillMastery <= MASTERY_GRANDMASTER);

    if (this->expireTime && (expire_time < this->expireTime)) {
        return false;
    }

    this->skillMastery = uSkillMastery;
    this->power = uPower;
    this->expireTime = expire_time;
    if (this->overlayId != uOverlayID)
        freeBuffOverlaySlot(&this->overlayId);
    this->overlayId = uOverlayID;
    this->caster = caster;

    return true;
}

void SpellStats::Initialize(const Blob &spells, GameVersion version) {
    static const std::map<std::string, DamageType, ascii::NoCaseLess> spellSchoolMaps = { // TODO(captainurist): #enum, use enum serialization
        {"fire", DAMAGE_FIRE},
        {"air", DAMAGE_AIR},
        {"water", DAMAGE_WATER},
        {"earth", DAMAGE_EARTH},
        {"spirit", DAMAGE_SPIRIT},
        {"mind", DAMAGE_MIND},
        {"body", DAMAGE_BODY},
        {"light", DAMAGE_LIGHT},
        {"dark", DAMAGE_DARK},
        {"magic", DAMAGE_MAGIC},
    };

    if (version == GAME_VERSION_MM6) {
        // MM6's spells.txt keeps MM7's 9-school x 11-spell layout (ids 1..99), so the native MM6 spell id
        // (token[0]) is the SpellId directly. Only the columns differ: MM6 has extra A/X/M columns before
        // the description, and no Grand Master tier / Stats-flags columns.
        //   cols: 0=id 1=level 2=name 3=school 4=short 5=A 6=X 7=M 8=description 9=Normal 10=Expert 11=Master
        // The file has two blank ruler rows, then a '#' column-header row, then the data (with per-school
        // section-header rows whose first column is empty). We drop the two ruler rows and skip both the
        // section headers (empty first column) and the '#' header row - more robust than a fixed .drop(3),
        // which would break if the header layout shifts.
        for (std::string_view line : split(spells.str()).by("\r\n").drop(2).skip("")) {
            std::array<std::string_view, 12> tokens = split(line).by('\t');
            if (tokens[0].empty() || tokens[0] == "#")
                continue; // Skip section headers and the '#' column-header row.

            SpellId uSpellID = static_cast<SpellId>(fromString<int>(tokens[0]));
            pInfos[uSpellID].name = removeQuotes(tokens[2]);
            pInfos[uSpellID].damageType = valueOr(spellSchoolMaps, tokens[3], DAMAGE_PHYSICAL);
            pInfos[uSpellID].pShortName = removeQuotes(tokens[4]);
            pInfos[uSpellID].pDescription = removeQuotes(tokens[8]);
            pInfos[uSpellID].pBasicSkillDesc = removeQuotes(tokens[9]);
            pInfos[uSpellID].pExpertSkillDesc = removeQuotes(tokens[10]);
            pInfos[uSpellID].pMasterSkillDesc = removeQuotes(tokens[11]);
            // MM6 has no Grand Master tier and no Stats/flags column, so pGrandmasterSkillDesc is left empty
            // and no spell flags are parsed here. The MM7-only shift-click / Control Undead patches below
            // must not run for MM6, hence the early return.
        }
        return;
    }

    // spells.txt table structure: index | ... | name (localized) | school (not localized) | ...
    // Section header lines have an empty first column and are skipped.
    for (std::string_view line : split(spells.str()).by("\r\n").drop(2).skip("")) {
        std::array<std::string_view, 11> tokens = split(line).by('\t');
        if (tokens[0].empty())
            continue; // Skip section headers.

        SpellId uSpellID = static_cast<SpellId>(fromString<int>(tokens[0]));
        pInfos[uSpellID].name = removeQuotes(tokens[2]);
        pInfos[uSpellID].damageType = valueOr(spellSchoolMaps, tokens[3], DAMAGE_PHYSICAL);
        pInfos[uSpellID].pShortName = removeQuotes(tokens[4]);
        pInfos[uSpellID].pDescription = removeQuotes(tokens[5]);
        pInfos[uSpellID].pBasicSkillDesc = removeQuotes(tokens[6]);
        pInfos[uSpellID].pExpertSkillDesc = removeQuotes(tokens[7]);
        pInfos[uSpellID].pMasterSkillDesc = removeQuotes(tokens[8]);
        pInfos[uSpellID].pGrandmasterSkillDesc = removeQuotes(tokens[9]);
        pSpellDatas[uSpellID].flags |= tokens[10].contains('m') || tokens[10].contains('M') ? SPELL_CASTABLE_BY_MONSTER : SpellFlag();
        pSpellDatas[uSpellID].flags |= tokens[10].contains('e') || tokens[10].contains('E') ? SPELL_CASTABLE_BY_EVENT : SpellFlag();
        pSpellDatas[uSpellID].flags |= tokens[10].contains('c') || tokens[10].contains('C') ? SPELL_SHIFT_CLICK_CASTABLE : SpellFlag();
        pSpellDatas[uSpellID].flags |= tokens[10].contains('x') || tokens[10].contains('X') ? SPELL_FLAG_8 : SpellFlag();
    }

    // Patch SPELL_SHIFT_CLICK_CASTABLE flags that are bogus in vanilla spells.txt. See issues #1494, #1495, #1496.
    // TODO(captainurist): move these flag patches into the patched data tables instead of hardcoding them here.
    // These spells target a single actor and should be quick-castable on shift+click.
    for (SpellId spell : {SPELL_WATER_POISON_SPRAY, SPELL_LIGHT_LIGHT_BOLT, SPELL_LIGHT_DESTROY_UNDEAD, SPELL_LIGHT_PARALYZE}) {
        pSpellDatas[spell].flags |= SPELL_SHIFT_CLICK_CASTABLE;
    }
    // These spells target the casting party or a party member and should NOT be quick-castable on shift+click.
    // For party-target spells the shift+click cast is also confusing because nothing visible happens at the
    // clicked actor; for character-target spells (Cure Paralysis) the player gets a target-character prompt
    // that doesn't relate to the clicked actor.
    for (SpellId spell : {SPELL_MIND_CURE_PARALYSIS, SPELL_LIGHT_DAY_OF_PROTECTION, SPELL_LIGHT_HOUR_OF_POWER,
                          SPELL_MIND_MASS_FEAR, SPELL_LIGHT_PRISMATIC_LIGHT, SPELL_DARK_ARMAGEDDON}) {
        pSpellDatas[spell].flags &= ~SPELL_SHIFT_CLICK_CASTABLE;
    }

    // Vanilla bug fix: Control Undead is a Dark Magic spell, but its description in spells.txt
    // states the duration scales with "Mind Magic". See issue #1698. The replacement is a no-op
    // for localizations that don't contain the English wording.
    // TODO(captainurist): these strings are translatable; move this fix into the patched data files.
    SpellInfo &controlUndead = pInfos[SPELL_DARK_CONTROL_UNDEAD];
    controlUndead.pDescription = replaceAll(controlUndead.pDescription, "Mind Magic", "Dark Magic");
    controlUndead.pBasicSkillDesc = replaceAll(controlUndead.pBasicSkillDesc, "Mind Magic", "Dark Magic");
    controlUndead.pExpertSkillDesc = replaceAll(controlUndead.pExpertSkillDesc, "Mind Magic", "Dark Magic");
    controlUndead.pMasterSkillDesc = replaceAll(controlUndead.pMasterSkillDesc, "Mind Magic", "Dark Magic");
    controlUndead.pGrandmasterSkillDesc = replaceAll(controlUndead.pGrandmasterSkillDesc, "Mind Magic", "Dark Magic");
}

SpellId translateForCast(SpellId nativeId, GameVersion version) {
    // MM7 is a pure identity, and any non-regular spell id (bow/laser projectiles, quest markers, ...)
    // has no MM6 remap - pass it straight through.
    if (version != GAME_VERSION_MM6 || !isRegularSpell(nativeId))
        return nativeId;

    // MM6 spell id -> MM7 SpellId whose effect matches the MM6 spell. Only ids that differ are listed;
    // identity is the default, so an MM6 spell that already lines up with the MM7 spell at the same id
    // (Torch Light@1, Flame Arrow->Fire Bolt@2, Protection buffs@3/14/25/69, Guardian Angel->Preservation@50,
    // Meditation->Remove Fear@56 [none], First Aid->Heal@68, ...) needs no entry here.
    static const std::map<SpellId, SpellId> mm6CastEffects = {
        // 22 remaps - the MM6 spell exists in MM7, just at a different id.
        {SPELL_FIRE_FIRE_AURA,               SPELL_FIRE_FIRE_BOLT},              // id4  MM6 Fire Bolt
        {SPELL_AIR_JUMP,                     SPELL_AIR_FEATHER_FALL},            // id16 MM6 Feather Fall
        {SPELL_AIR_INVISIBILITY,             SPELL_AIR_JUMP},                    // id19 MM6 Jump
        {SPELL_WATER_ICE_BOLT,               SPELL_WATER_POISON_SPRAY},          // id26 MM6 Poison Spray
        {SPELL_WATER_RECHARGE_ITEM,          SPELL_WATER_ICE_BOLT},              // id28 MM6 Ice Bolt
        {SPELL_WATER_ACID_BURST,             SPELL_WATER_ENCHANT_ITEM},          // id29 MM6 Enchant Item
        {SPELL_WATER_ENCHANT_ITEM,           SPELL_WATER_ACID_BURST},            // id30 MM6 Acid Burst
        {SPELL_EARTH_PROTECTION_FROM_EARTH,  SPELL_BODY_PROTECTION_FROM_MAGIC},  // id36 MM6 Protection from Magic
        {SPELL_SPIRIT_SPIRIT_LASH,           SPELL_SPIRIT_TURN_UNDEAD},          // id52 MM6 Turn Undead
        {SPELL_MIND_MIND_BLAST,              SPELL_MIND_REMOVE_FEAR},            // id57 MM6 Remove Fear
        {SPELL_MIND_PROTECTION_FROM_MIND,    SPELL_MIND_MIND_BLAST},             // id58 MM6 Mind Blast
        {SPELL_MIND_CHARM,                   SPELL_MIND_CURE_PARALYSIS},         // id60 MM6 Cure Paralysis
        {SPELL_MIND_CURE_PARALYSIS,          SPELL_MIND_CHARM},                  // id61 MM6 Charm
        {SPELL_MIND_BERSERK,                 SPELL_MIND_MASS_FEAR},              // id62 MM6 Mass Fear
        {SPELL_MIND_ENSLAVE,                 SPELL_EARTH_TELEKINESIS},           // id66 MM6 Telekinesis
        {SPELL_LIGHT_PARALYZE,               SPELL_EARTH_SLOW},                  // id81 MM6 Slow
        {SPELL_LIGHT_SUMMON_ELEMENTAL,       SPELL_LIGHT_DESTROY_UNDEAD},        // id82 MM6 Destroy Undead
        {SPELL_LIGHT_DAY_OF_PROTECTION,      SPELL_LIGHT_HOUR_OF_POWER},         // id85 MM6 Hour of Power
        {SPELL_LIGHT_HOUR_OF_POWER,          SPELL_LIGHT_PARALYZE},              // id86 MM6 Paralyze
        {SPELL_DARK_SHRINKING_RAY,           SPELL_DARK_SHARPMETAL},             // id92 MM6 Shrapmetal
        {SPELL_DARK_SHARPMETAL,              SPELL_DARK_SHRINKING_RAY},          // id93 MM6 Shrinking Ray
        {SPELL_DARK_CONTROL_UNDEAD,          SPELL_LIGHT_DAY_OF_PROTECTION},     // id94 MM6 Day of Protection

        // 16 analogs - MM6-unique spell mapped to the closest MM7 effect (at a different id).
        {SPELL_FIRE_FIRE_SPIKE,              SPELL_FIRE_INFERNO},                // id7  MM6 Ring of Fire -> Inferno (fire AoE)
        {SPELL_FIRE_IMMOLATION,              SPELL_FIRE_FIREBALL},               // id8  MM6 Fire Blast -> Fireball
        {SPELL_AIR_FEATHER_FALL,             SPELL_AIR_SPARKS},                  // id13 MM6 Static Charge -> Sparks
        {SPELL_WATER_POISON_SPRAY,           SPELL_WATER_ICE_BOLT},              // id24 MM6 Cold Beam -> Ice Bolt (cold bolt)
        {SPELL_EARTH_SLOW,                   SPELL_EARTH_DEADLY_SWARM},          // id35 MM6 Magic Arrow -> Deadly Swarm (single earth proj)
        {SPELL_EARTH_TELEKINESIS,            SPELL_LIGHT_PARALYZE},              // id42 MM6 Turn to Stone -> Paralyze (petrify~paralyze)
        // id45 MM6 Spirit Arrow -> Harm: a plain targeted projectile. MM6.EXE's handler (0x4230e1) is the
        // shared single-projectile launch tail; the earlier Spirit Lash analog forms no projectile at all
        // (it's a close-range direct hit), so Spirit Arrow silently did nothing at range.
        {SPELL_SPIRIT_DETECT_LIFE,           SPELL_BODY_HARM},
        {SPELL_SPIRIT_FATE,                  SPELL_BODY_FIRST_AID},              // id47 MM6 Healing Touch -> Heal (cross-school)
        {SPELL_SPIRIT_TURN_UNDEAD,           SPELL_SPIRIT_FATE},                 // id48 MM6 Lucky Day -> Fate (luck buff)
        {SPELL_MIND_TELEPATHY,               SPELL_SPIRIT_BLESS},                // id59 MM6 Precision -> Bless (attack buff, cross-school)
        {SPELL_MIND_MASS_FEAR,               SPELL_MIND_BERSERK},                // id63 MM6 Feeblemind -> Berserk (mind debuff)
        {SPELL_BODY_REGENERATION,            SPELL_BODY_FIRST_AID},              // id71 MM6 Cure Wounds -> Heal
        {SPELL_BODY_HAMMERHANDS,             SPELL_FIRE_HASTE},                  // id73 MM6 Speed -> Haste (cross-school)
        {SPELL_BODY_PROTECTION_FROM_MAGIC,   SPELL_BODY_HAMMERHANDS},            // id75 MM6 Power -> Hammerhands (self dmg buff)
        {SPELL_DARK_PAIN_REFLECTION,         SPELL_DARK_SOULDRINKER},            // id95 MM6 Finger of Death -> Souldrinker (heavy dark dmg)
        {SPELL_DARK_SACRIFICE,               SPELL_LIGHT_SUNRAY},                // id96 MM6 Moon Ray -> Sunray (ray dmg, cross-school)

        // 4 [none] placeholders - MM6-unique with no real MM7 analog, mapped to the safest nearest effect
        // and tracked as residue.
        {SPELL_LIGHT_LIGHT_BOLT,             SPELL_BODY_FIRST_AID},              // id78 MM6 Create Food -> Heal [NONE-analog placeholder]
        {SPELL_LIGHT_DESTROY_UNDEAD,         SPELL_LIGHT_DISPEL_MAGIC},          // id79 MM6 Golden Touch -> Dispel [NONE-analog placeholder]
        {SPELL_DARK_VAMPIRIC_WEAPON,         SPELL_DARK_TOXIC_CLOUD},            // id91 MM6 Mass Curse -> Toxic Cloud [NONE-analog placeholder]
        {SPELL_DARK_SOULDRINKER,             SPELL_DARK_TOXIC_CLOUD},            // id99 MM6 Dark Containment -> Toxic Cloud [NONE-analog placeholder]
    };

    return valueOr(mm6CastEffects, nativeId, nativeId);
}

void eventCastSpell(SpellId uSpellID, Mastery skillMastery, int skillLevel, Vec3f from, Vec3f to) {
    // For bug catching
    assert(skillMastery >= MASTERY_NOVICE && skillMastery <= MASTERY_GRANDMASTER);

    Vec3f coord_delta;
    if (to.lengthSqr() > 1.0f) {
        coord_delta = to - from;
    } else {
        coord_delta = pParty->pos - from + Vec3f(0, 0, pParty->eyeLevel);
    }

    int yaw = 0;
    int pitch = 0;
    float distance_to_target = coord_delta.length();
    if (distance_to_target <= 1.0) {
        distance_to_target = 1;
    } else {
        int64_t ySquared = coord_delta.y * coord_delta.y;
        int64_t xSquared = coord_delta.x * coord_delta.x;
        int xy_distance = (int)std::sqrt((double)(xSquared + ySquared));
        yaw = TrigLUT.atan2(coord_delta.x, coord_delta.y);
        pitch = TrigLUT.atan2(xy_distance, coord_delta.z);
    }

    SpriteObject spell_sprites;

    switch (uSpellID) {
        case SPELL_FIRE_FIRE_BOLT:
        case SPELL_FIRE_FIREBALL:
        case SPELL_AIR_LIGHTNING_BOLT:
        case SPELL_WATER_ICE_BOLT:
        case SPELL_WATER_ACID_BURST:
        case SPELL_WATER_ICE_BLAST:
        case SPELL_EARTH_BLADES:
        case SPELL_EARTH_ROCK_BLAST:
        case SPELL_WATER_POISON_SPRAY:
        case SPELL_AIR_SPARKS:
        case SPELL_EARTH_DEATH_BLOSSOM:
            spell_sprites.spriteId = SpellSpriteMapping[uSpellID];
            spell_sprites.containing_item.Reset();
            spell_sprites.uSpellID = uSpellID;
            spell_sprites.spell_level = skillLevel;
            spell_sprites.spell_skill = skillMastery;
            spell_sprites.uObjectDescID = pObjectList->ObjectIDByItemID(spell_sprites.spriteId);
            spell_sprites.vPosition = from;
            spell_sprites.uAttributes = SPRITE_IGNORE_RANGE;
            spell_sprites.uSectorID = pIndoor->GetSector(from);
            spell_sprites.field_60_distance_related_prolly_lod = distance_to_target;
            spell_sprites.timeSinceCreated = 0_ticks;
            spell_sprites.spell_caster_pid = Pid(OBJECT_Sprite, 1000); // 8000 | OBJECT_Sprite;
            spell_sprites.uSoundID = 0;
            break;
        default:
            break;
    }

    Duration spell_length;
    int spell_power = 0;
    int launch_angle;
    int launch_speed;
    int spell_num_objects;
    int spell_spray_arc;
    int spell_spray_angles;
    int spriteid;
    PartyBuff buff_id;

    switch (uSpellID) {
        case SPELL_FIRE_FIRE_BOLT:
        case SPELL_FIRE_FIREBALL:
        case SPELL_AIR_LIGHTNING_BOLT:
        case SPELL_WATER_ICE_BOLT:
        case SPELL_WATER_ACID_BURST:
        case SPELL_WATER_ICE_BLAST:
        case SPELL_EARTH_BLADES:
        case SPELL_EARTH_ROCK_BLAST:
            // v20 = yaw;
            spell_sprites.spell_target_pid = Pid();
            spell_sprites.uFacing = yaw;
            spell_sprites.uSoundID = 0;
            launch_speed = pObjectList->pObjects[(int16_t)spell_sprites.uObjectDescID].uSpeed;
            spriteid = spell_sprites.Create(yaw, pitch, launch_speed, 0);
            //    pAudioPlayer->PlaySound(word_4EE088_sound_ids[uSpellID],
            //    0, 0, fromx, fromy, 0, 0, 0);
            pAudioPlayer->playSpellSound(uSpellID, false, SOUND_MODE_PID, Pid(OBJECT_Sprite, spriteid));
            break;
        case SPELL_WATER_POISON_SPRAY:
            spell_num_objects = (std::to_underlying(skillMastery) * 2) - 1;
            spell_sprites.spell_target_pid = Pid();
            spell_sprites.uFacing = yaw;
            if (spell_num_objects == 1) {
                launch_speed = pObjectList->pObjects[(int16_t)spell_sprites.uObjectDescID].uSpeed;
                spriteid = spell_sprites.Create(yaw, pitch, launch_speed, 0);
            } else {
                spell_spray_arc = (signed int)(60 * TrigLUT.uIntegerDoublePi) / 360;
                spell_spray_angles = spell_spray_arc / (spell_num_objects - 1);
                for (int i = spell_spray_arc / -2; i <= spell_spray_arc / 2; i += spell_spray_angles) {
                    spell_sprites.uFacing = i + yaw;
                    spriteid = spell_sprites.Create(i + yaw, pitch, pObjectList->pObjects[spell_sprites.uObjectDescID].uSpeed, 0);
                }
            }
            //    pAudioPlayer->PlaySound(word_4EE088_sound_ids[uSpellID],
            //    0, 0, fromx, fromy, 0, 0, 0);
            pAudioPlayer->playSpellSound(uSpellID, false, SOUND_MODE_PID, Pid(OBJECT_Sprite, spriteid));
            break;
        case SPELL_AIR_SPARKS:
            spell_num_objects = (std::to_underlying(skillMastery) * 2) + 1;
            spell_spray_arc = (signed int)(60 * TrigLUT.uIntegerDoublePi) / 360;
            spell_spray_angles = spell_spray_arc / (spell_num_objects - 1);
            spell_sprites.spell_target_pid = Pid::character(0);
            for (int i = spell_spray_arc / -2; i <= spell_spray_arc / 2; i += spell_spray_angles) {
                spell_sprites.uFacing = i + yaw;
                spriteid = spell_sprites.Create(i + yaw, pitch, pObjectList->pObjects[spell_sprites.uObjectDescID].uSpeed, 0);
            }
            //    pAudioPlayer->PlaySound(word_4EE088_sound_ids[uSpellID],
            //    0, 0, fromx, fromy, 0, 0, 0);
            pAudioPlayer->playSpellSound(uSpellID, false, SOUND_MODE_PID, Pid(OBJECT_Sprite, spriteid));
            break;
        case SPELL_EARTH_DEATH_BLOSSOM:
            if (uCurrentlyLoadedLevelType == LEVEL_INDOOR) {
                return;
            }
            spell_sprites.spell_target_pid = Pid::character(0);
            launch_speed = pObjectList->pObjects[spell_sprites.uObjectDescID].uSpeed;
            launch_angle = TrigLUT.uIntegerHalfPi / 2;
            spriteid = spell_sprites.Create(yaw, launch_angle, launch_speed, 0);
            //    pAudioPlayer->PlaySound(word_4EE088_sound_ids[uSpellID],
            //    0, 0, fromx, fromy, 0, 0, 0);
            pAudioPlayer->playSpellSound(uSpellID, false, SOUND_MODE_PID, Pid(OBJECT_Sprite, spriteid));
            break;

        case SPELL_FIRE_HASTE:
            if (skillMastery >= MASTERY_NOVICE) {
                if (skillMastery <= MASTERY_EXPERT) {
                    spell_length = Duration::fromHours(1) + Duration::fromMinutes(skillLevel);
                } else if (skillMastery == MASTERY_MASTER) {
                    spell_length = Duration::fromHours(1) + Duration::fromMinutes(3 * skillLevel);
                } else if (skillMastery == MASTERY_GRANDMASTER) {
                    spell_length = Duration::fromHours(1) + Duration::fromMinutes(4 * skillLevel);
                }
            }
            for (Character &player : pParty->pCharacters) {
                if (player.IsWeak()) {
                    return;
                }
            }
            pParty->pPartyBuffs[PARTY_BUFF_HASTE].Apply(pParty->GetPlayingTime() + spell_length, skillMastery, 0, 0, 0);
            spell_fx_renderer->SetPartyBuffAnim(uSpellID);
            //    pAudioPlayer->PlaySound(word_4EE088_sound_ids[uSpellID],
            //    0, 0, fromx, fromy, 0, 0, 0);  // звук алтаря
            //    Pid was 0
            pAudioPlayer->playSpellSound(uSpellID, false, SOUND_MODE_EXCLUSIVE);
            break;
        case SPELL_AIR_SHIELD:
        case SPELL_EARTH_STONESKIN:
        case SPELL_SPIRIT_HEROISM:
            switch (skillMastery) {
                case MASTERY_NOVICE:
                case MASTERY_EXPERT:
                    spell_length = Duration::fromHours(1) + Duration::fromMinutes(5 * skillLevel);
                    break;
                case MASTERY_MASTER:
                    spell_length = Duration::fromHours(1) + Duration::fromMinutes(15 * skillLevel);
                    break;
                case MASTERY_GRANDMASTER:
                    spell_length = Duration::fromHours(skillLevel + 1);
                    break;
                default:
                    assert(false);
                    break;
            }
            if (uSpellID == SPELL_AIR_SHIELD) {
                spell_power = 0;
                buff_id = PARTY_BUFF_SHIELD;
            } else if (uSpellID == SPELL_EARTH_STONESKIN) {
                spell_power = skillLevel + 5;
                buff_id = PARTY_BUFF_STONE_SKIN;
            } else {
                assert(uSpellID == SPELL_SPIRIT_HEROISM);
                spell_power = skillLevel + 5;
                buff_id = PARTY_BUFF_HEROISM;
            }
            spell_fx_renderer->SetPartyBuffAnim(uSpellID);
            pParty->pPartyBuffs[buff_id].Apply(pParty->GetPlayingTime() + spell_length, skillMastery, spell_power, 0, 0);
            //    pAudioPlayer->PlaySound(word_4EE088_sound_ids[uSpellID],
            //    0, 0, fromx, fromy, 0, 0, 0);
            //    Pid was 0
            pAudioPlayer->playSpellSound(uSpellID, false, SOUND_MODE_EXCLUSIVE);
            break;
        case SPELL_FIRE_IMMOLATION:
            if (skillMastery == MASTERY_GRANDMASTER) {
                spell_length = Duration::fromMinutes(10 * skillLevel);
            } else {
                spell_length = Duration::fromMinutes(skillLevel);
            }
            spell_fx_renderer->SetPartyBuffAnim(uSpellID);
            pParty->pPartyBuffs[PARTY_BUFF_IMMOLATION].Apply(pParty->GetPlayingTime() + spell_length, skillMastery, skillLevel, 0, 0);
            //    pAudioPlayer->PlaySound(word_4EE088_sound_ids[uSpellID],
            //    0, 0, fromx, fromy, 0, 0, 0);
            //    Pid was 0
            pAudioPlayer->playSpellSound(uSpellID, false, SOUND_MODE_EXCLUSIVE);
            break;
        case SPELL_FIRE_PROTECTION_FROM_FIRE:
        case SPELL_AIR_PROTECTION_FROM_AIR:
        case SPELL_WATER_PROTECTION_FROM_WATER:
        case SPELL_EARTH_PROTECTION_FROM_EARTH:
        case SPELL_MIND_PROTECTION_FROM_MIND:
        case SPELL_BODY_PROTECTION_FROM_BODY:
            spell_length = Duration::fromHours(skillLevel);
            spell_power = skillLevel * std::to_underlying(skillMastery);

            if (uSpellID == SPELL_FIRE_PROTECTION_FROM_FIRE) {
                buff_id = PARTY_BUFF_RESIST_FIRE;
            } else if (uSpellID == SPELL_AIR_PROTECTION_FROM_AIR) {
                buff_id = PARTY_BUFF_RESIST_AIR;
            } else if (uSpellID == SPELL_WATER_PROTECTION_FROM_WATER) {
                buff_id = PARTY_BUFF_RESIST_WATER;
            } else if (uSpellID == SPELL_EARTH_PROTECTION_FROM_EARTH) {
                buff_id = PARTY_BUFF_RESIST_EARTH;
            } else if (uSpellID == SPELL_MIND_PROTECTION_FROM_MIND) {
                buff_id = PARTY_BUFF_RESIST_MIND;
            } else {
                assert(uSpellID == SPELL_BODY_PROTECTION_FROM_BODY);
                buff_id = PARTY_BUFF_RESIST_BODY;
            }

            spell_fx_renderer->SetPartyBuffAnim(uSpellID);
            pParty->pPartyBuffs[buff_id].Apply(pParty->GetPlayingTime() + spell_length, skillMastery, spell_power, 0, 0);
            //    pAudioPlayer->PlaySound(word_4EE088_sound_ids[uSpellID],
            //    0, 0, fromx, fromy, 0, 0, 0);
            //    Pid was 0
            pAudioPlayer->playSpellSound(uSpellID, false, SOUND_MODE_EXCLUSIVE);
            break;
        case SPELL_LIGHT_DAY_OF_THE_GODS:
            // Spell lengths for master and grandmaster were mixed up
            switch (skillMastery) {
                case MASTERY_EXPERT:
                    spell_length = Duration::fromHours(3 * skillLevel);
                    spell_power = 3 * skillLevel + 10;
                    break;
                case MASTERY_MASTER:
                    spell_length = Duration::fromHours(4 * skillLevel);
                    spell_power = 5 * skillLevel + 10;
                    break;
                case MASTERY_GRANDMASTER:
                    spell_length = Duration::fromHours(5 * skillLevel);
                    spell_power = 4 * skillLevel + 10;
                    break;
                default:
                    break;
            }
            spell_fx_renderer->SetPartyBuffAnim(uSpellID);

            pParty->pPartyBuffs[PARTY_BUFF_DAY_OF_GODS].Apply(pParty->GetPlayingTime() + spell_length, skillMastery, spell_power, 0, 0);
            //    pAudioPlayer->PlaySound(word_4EE088_sound_ids[uSpellID],
            //    0, 0, fromx, fromy, 0, 0, 0);
            //    Pid was 0
            pAudioPlayer->playSpellSound(uSpellID, false, SOUND_MODE_EXCLUSIVE);
            break;
        default:
            break;
    }
}

bool IsSpellQuickCastableOnShiftClick(SpellId uSpellID) {
    return pSpellDatas[uSpellID].flags & (SPELL_SHIFT_CLICK_CASTABLE | SPELL_FLAG_8);
}

int CalcSpellDamage(SpellId uSpellID, int spellLevel, Mastery skillMastery, int currentHp) {
    int result;       // eax@1
    unsigned int diceSides;  // [sp-4h] [bp-8h]@9

    // MM6 computes spell damage with its own per-spell formulas, keyed on the NATIVE spell id and independent
    // of skill mastery - decoded verbatim from MM6.EXE's CalcSpellDamage (VA 0x47F0A0, a jump table over spell
    // ids 2..99). The generic shape "baseDamage + one d(bonusSkillDamage) die per point of skill" is loaded
    // into pSpellDatas by applyMm6SpellDatas(); the spells below don't fit that shape and are hardcoded here,
    // exactly like the EXE does. Every CalcSpellDamage caller is a damage sink (projectile/AoE impact, monster
    // spell attack, item damage), and callers derive the damage school from pSpellStats->pInfos separately.
    if (engine->gameVersion() == GAME_VERSION_MM6) {
        switch (uSpellID) {
            case SPELL_FIRE_FIRE_BOLT:        // Native id 2 = MM6 Flame Arrow: a flat 1d8, no skill scaling.
                return grng->randomDice(1, 8);
            case SPELL_AIR_FEATHER_FALL:      // Native id 13 = MM6 Static Charge: flat 2-6.
                return 1 + grng->randomDice(1, 5);
            case SPELL_WATER_POISON_SPRAY:    // Native id 24 = MM6 Cold Beam: flat 2d3 = 2-6.
                return grng->randomDice(2, 3);
            case SPELL_EARTH_SLOW:            // Native id 35 = MM6 Magic Arrow: flat 3-8.
                return 2 + grng->randomDice(1, 6);
            case SPELL_SPIRIT_DETECT_LIFE:    // Native id 45 = MM6 Spirit Arrow: a flat 1d6.
                return grng->randomDice(1, 6);
            case SPELL_WATER_ENCHANT_ITEM:    // Native id 30 = MM6 Acid Burst: 9 + skill x (0..8). The EXE die
                                              // is 0-based, unlike the "1-9 per point" its description claims.
                return 9 - spellLevel + grng->randomDice(spellLevel, 9);
            case SPELL_EARTH_MASS_DISTORTION: // Aligned id 44: 25% of current hp, plus 2% per point of skill.
                return currentHp * (25 + 2 * spellLevel) / 100;
            default:
                return pSpellDatas[uSpellID].baseDamage + grng->randomDice(spellLevel, pSpellDatas[uSpellID].bonusSkillDamage);
        }
    }

    result = 0;
    if (uSpellID == SPELL_FIRE_FIRE_SPIKE) {
        switch (skillMastery) {
            case MASTERY_NOVICE:
            case MASTERY_EXPERT:
                diceSides = 6;
                break;
            case MASTERY_MASTER:
                diceSides = 8;
                break;
            case MASTERY_GRANDMASTER:
                diceSides = 10;
                break;
            default:
                return 0;
        }
        result = grng->randomDice(spellLevel, diceSides);
    } else if (uSpellID == SPELL_EARTH_DEATH_BLOSSOM && skillMastery == MASTERY_GRANDMASTER) {
        result = pSpellDatas[uSpellID].baseDamage + spellLevel * 2; // does 2 damage per point of skill at GM
    } else if (uSpellID == SPELL_EARTH_MASS_DISTORTION) {
        result = currentHp * (pSpellDatas[SPELL_EARTH_MASS_DISTORTION].baseDamage + pSpellDatas[uSpellID].bonusSkillDamage * spellLevel) / 100;
    } else if (uSpellID == SPELL_SPIRIT_SPIRIT_LASH) {
        result = pSpellDatas[uSpellID].baseDamage + spellLevel + grng->randomDice(spellLevel, pSpellDatas[uSpellID].bonusSkillDamage - 1); // damage is 2-8 per point of skill
    } else {
        result = pSpellDatas[uSpellID].baseDamage + grng->randomDice(spellLevel, pSpellDatas[uSpellID].bonusSkillDamage);
    }

    return result;
}

void armageddonProgress() {
    assert(uCurrentlyLoadedLevelType == LEVEL_OUTDOOR && pParty->armageddon_timer > 0_ticks);

    if (pParty->armageddon_timer > 417_ticks) {
        pParty->armageddon_timer = 0_ticks;
        return; // TODO(captainurist): wtf? Looks like a quick hack for some bug.
    }

    if (pTurnEngine->pending_actions) {
        --pTurnEngine->pending_actions;
    }

    pParty->_viewYaw = TrigLUT.uDoublePiMask & (pParty->_viewYaw + grng->randomInSegment(-8, 8)); // Was RandomInSegment(-8, 7)
    pParty->_viewPitch = std::clamp(pParty->_viewPitch + grng->randomInSegment(-8, 8), -128, 128); // Was RandomInSegment(-8, 7)
    pParty->armageddon_timer = std::max(0_ticks, pParty->armageddon_timer - pEventTimer->dt()); // Was pMiscTimer

    // TODO(pskelton): ignore if pEventTimer->uTimeElapsed is zero?
    // TODO(captainurist): See the logic in Outdoor.cpp, right now the force is applied in fixed amounts per frame,
    // while it should be applied in amounts relative to frame time --- basically, armageddon should provide some
    // acceleration, and then this acceleration should be applied to actors over a brief period of time.
    --pParty->armageddonForceCount;
    if (pParty->armageddon_timer) {
        return; // Deal damage only when timer gets to 0.
    }

    int outgoingDamage = pParty->armageddonDamage + 50;

    for (Actor &actor : pActors) {
        if (!actor.CanAct()) {
            continue; // TODO(captainurist): paralyzed & summoned actors should receive damage too!
        }

        int incomingDamage = actor.CalcMagicalDamageToActor(DAMAGE_MAGIC, outgoingDamage);
        if (incomingDamage > 0) {
            actor.hp -= incomingDamage;

            if (actor.hp >= 0) {
                Actor::AI_Stun(actor.id, Pid::character(0), 0);
            } else {
                Actor::Die(actor.id);
                if (actor.monsterInfo.exp) {
                    pParty->GivePartyExp(pMonsterStats->infos[actor.monsterInfo.id].exp);
                }
            }
        }
    }

    for (Character &player : pParty->pCharacters) {
        if (!player.conditions.hasAny({CONDITION_DEAD, CONDITION_PETRIFIED, CONDITION_ERADICATED})) {
            player.receiveDamage(outgoingDamage, DAMAGE_MAGIC);
        }
    }
}

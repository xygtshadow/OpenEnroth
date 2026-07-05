#include <string>

#include "Testing/Game/GameTest.h"

#include "Application/Paths/GameVersion.h"

#include "Engine/Random/Random.h"
#include "Engine/Spells/Spells.h"
#include "Engine/Spells/SpellEnums.h"

#include "Library/Random/MersenneTwisterRandomEngine.h"

#include "Utility/Memory/Blob.h"

// MM6's spells.txt keeps the same 9-school x 11-spell layout as MM7 (ids 1..99), but the column
// order differs: MM6 has extra A/X/M columns, and no Grand Master / Stats columns. The native MM6
// spell id (token[0]) is the SpellId, so parsing must read the MM6 columns and land names/descriptions
// on the right slots.
GAME_TEST(SpellsMm6, ParsesMm6Layout) {
    // MM6 spells.txt: two blank ruler rows, a '#' header row, then data.
    // cols: 0=id 1=lvl 2=name 3=school 4=short 5=A 6=X 7=M 8=desc 9=normal 10=expert 11=master
    std::string text =
        "\t\t\t\t\t\t\t\t\t\t\t\r\n"
        "\t\t\t\t\t\t\t\t\t\t\t\r\n"
        "#\tFire Spells\t\tRes\tShort Name\tA\tX\tM\tSpell Description\tNormal\tExpert\tMaster\r\n"
        "2\t2\tFlame Arrow\tFire\tFlame Arrow\t2\t1\t0\tFires a flaming arrow.\tcost 2\tcost 1\tcost 0\r\n"
        "81\t9\tSlow\tLight\tSlow\t1\t1\t1\tSlows a target.\ta\tb\tc\r\n";
    SpellStats stats;
    stats.Initialize(Blob::fromString(text), GAME_VERSION_MM6);
    EXPECT_EQ(stats.pInfos[SPELL_FIRE_FIRE_BOLT].name, "Flame Arrow");   // MM6 id 2
    EXPECT_EQ(stats.pInfos[SPELL_LIGHT_PARALYZE].name, "Slow");          // MM6 id 81
    EXPECT_EQ(stats.pInfos[SPELL_FIRE_FIRE_BOLT].pShortName, "Flame Arrow");
    EXPECT_EQ(stats.pInfos[SPELL_FIRE_FIRE_BOLT].pDescription, "Fires a flaming arrow.");
    EXPECT_EQ(stats.pInfos[SPELL_FIRE_FIRE_BOLT].pBasicSkillDesc, "cost 2");
    EXPECT_EQ(stats.pInfos[SPELL_FIRE_FIRE_BOLT].pExpertSkillDesc, "cost 1");   // col 10
    EXPECT_EQ(stats.pInfos[SPELL_FIRE_FIRE_BOLT].pMasterSkillDesc, "cost 0");   // col 11
    EXPECT_TRUE(stats.pInfos[SPELL_FIRE_FIRE_BOLT].pGrandmasterSkillDesc.empty()); // MM6 has no GM tier
    EXPECT_EQ(stats.pInfos[SPELL_LIGHT_PARALYZE].damageType, DAMAGE_LIGHT);
}

// Tests for spell damage formulas from MM7 v1.1. Issue: #2055.

GAME_TEST(Spells, SpiritLashDamageFormula) {
    // Spirit Lash does 10 + 2-8 damage per SP (baseDamage=10, bonusSkillDamage=8, dice faces=7).
    // Formula: baseDamage + spellLevel + randomDice(spellLevel, bonusSkillDamage - 1).
    // At spellLevel=5: 10 + 5 + randomDice(5, 7) in [5, 35] => damage in [20, 50].
    MersenneTwisterRandomEngine engine;
    RandomEngine *savedGrng = grng;
    grng = &engine;
    for (int i = 0; i < 100; i++) {
        int damage = CalcSpellDamage(SPELL_SPIRIT_SPIRIT_LASH, 5, MASTERY_NOVICE, 0);
        EXPECT_GE(damage, 20);
        EXPECT_LE(damage, 50);
    }
    grng = savedGrng;
}

GAME_TEST(Spells, DeathBlossomGrandmasterDamage) {
    // Death Blossom at GM does 20 + 2 per SP (no randomness).
    // Formula: baseDamage + spellLevel * 2, where baseDamage=20.
    EXPECT_EQ(CalcSpellDamage(SPELL_EARTH_DEATH_BLOSSOM, 5, MASTERY_GRANDMASTER, 0), 30);   // 20 + 5*2
    EXPECT_EQ(CalcSpellDamage(SPELL_EARTH_DEATH_BLOSSOM, 10, MASTERY_GRANDMASTER, 0), 40);  // 20 + 10*2
    EXPECT_EQ(CalcSpellDamage(SPELL_EARTH_DEATH_BLOSSOM, 1, MASTERY_GRANDMASTER, 0), 22);   // 20 + 1*2
}

GAME_TEST(Spells, MeteorShowerDamageFormula) {
    // Meteor Shower does 1-8 per SP (baseDamage=0, bonusSkillDamage=8).
    // Formula: baseDamage + randomDice(spellLevel, bonusSkillDamage).
    // At spellLevel=5: randomDice(5, 8) in [5, 40] => damage in [5, 40].
    MersenneTwisterRandomEngine engine;
    RandomEngine *savedGrng = grng;
    grng = &engine;
    for (int i = 0; i < 100; i++) {
        int damage = CalcSpellDamage(SPELL_FIRE_METEOR_SHOWER, 5, MASTERY_MASTER, 0);
        EXPECT_GE(damage, 5);
        EXPECT_LE(damage, 40);
    }
    grng = savedGrng;
}

GAME_TEST(Spells, PsychicShockDamageFormula) {
    // Psychic Shock does 12 + 1-12 per SP (baseDamage=12, bonusSkillDamage=12).
    // Formula: baseDamage + randomDice(spellLevel, bonusSkillDamage).
    // At spellLevel=5: 12 + randomDice(5, 12) in [5, 60] => damage in [17, 72].
    MersenneTwisterRandomEngine engine;
    RandomEngine *savedGrng = grng;
    grng = &engine;
    for (int i = 0; i < 100; i++) {
        int damage = CalcSpellDamage(SPELL_MIND_PSYCHIC_SHOCK, 5, MASTERY_NOVICE, 0);
        EXPECT_GE(damage, 17);
        EXPECT_LE(damage, 72);
    }
    grng = savedGrng;
}

// Tests that SPELL_SHIFT_CLICK_CASTABLE is patched up in SpellStats::Initialize. Issues: #1494, #1495, #1496.
GAME_TEST(Spells, ShiftClickCastableFlags) {
    // #1494: these spells target a single actor and must be quick-castable on shift+click.
    EXPECT_TRUE(IsSpellQuickCastableOnShiftClick(SPELL_WATER_POISON_SPRAY));
    EXPECT_TRUE(IsSpellQuickCastableOnShiftClick(SPELL_LIGHT_LIGHT_BOLT));
    EXPECT_TRUE(IsSpellQuickCastableOnShiftClick(SPELL_LIGHT_DESTROY_UNDEAD));
    EXPECT_TRUE(IsSpellQuickCastableOnShiftClick(SPELL_LIGHT_PARALYZE));

    // #1495: these spells target the party or a party member; they must NOT be quick-castable on shift+click.
    EXPECT_FALSE(IsSpellQuickCastableOnShiftClick(SPELL_MIND_CURE_PARALYSIS));
    EXPECT_FALSE(IsSpellQuickCastableOnShiftClick(SPELL_LIGHT_DAY_OF_PROTECTION));
    EXPECT_FALSE(IsSpellQuickCastableOnShiftClick(SPELL_LIGHT_HOUR_OF_POWER));

    // #1496: the all-in-sight AoE spells are inconsistent in vanilla; we standardize on
    // "not castable" since the only way to indicate the target is by clicking an actor,
    // and the spell ignores that target anyway.
    EXPECT_FALSE(IsSpellQuickCastableOnShiftClick(SPELL_MIND_MASS_FEAR));
    EXPECT_FALSE(IsSpellQuickCastableOnShiftClick(SPELL_LIGHT_PRISMATIC_LIGHT));
    EXPECT_FALSE(IsSpellQuickCastableOnShiftClick(SPELL_DARK_ARMAGEDDON));
    // Spells in the same all-in-sight group that were already not quick-castable in vanilla:
    EXPECT_FALSE(IsSpellQuickCastableOnShiftClick(SPELL_SPIRIT_TURN_UNDEAD));
    EXPECT_FALSE(IsSpellQuickCastableOnShiftClick(SPELL_DARK_SOULDRINKER));
    EXPECT_FALSE(IsSpellQuickCastableOnShiftClick(SPELL_LIGHT_DISPEL_MAGIC));
}

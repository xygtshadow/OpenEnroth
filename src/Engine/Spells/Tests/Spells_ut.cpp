#include <string>
#include <utility>
#include <vector>

#include "Testing/Game/GameTest.h"

#include "Application/Paths/GameVersion.h"

#include "Engine/Random/Random.h"
#include "Engine/Spells/Spells.h"
#include "Engine/Spells/SpellEnums.h"

#include "Library/Random/MersenneTwisterRandomEngine.h"

#include "Utility/Memory/Blob.h"

// Joins cells with '\t' and rows with '\r\n' to mimic the on-disk spells.txt table format that
// SpellStats::Initialize parses.
static Blob makeSpellsBlob(const std::vector<std::vector<std::string>> &rows) {
    std::string bytes;
    for (const std::vector<std::string> &row : rows) {
        for (size_t i = 0; i < row.size(); i++) {
            if (i != 0)
                bytes += '\t';
            bytes += row[i];
        }
        bytes += "\r\n";
    }
    return Blob::fromString(std::move(bytes));
}

// MM6's spells.txt is present but its spell SET differs from MM7's (51/99 id positions are a different
// spell; 29 MM6 spells have no MM7 equivalent), so token[0] does not map to the MM7-shaped SpellId
// enum. MM6 spell info is therefore deliberately deferred for now: Initialize(version=MM6) must not
// throw (a naive MM7-layout parse hits the MM6 '#' header row and throws "'#' is not a number") and
// must leave pInfos unpopulated.
GAME_TEST(SpellsMm6, InitializeDefers) {
    SpellStats stats;
    Blob blob = makeSpellsBlob({
        {""}, // MM6 has TWO leading blank rows (MM7 has one)...
        {""},
        // ...then the '#' column header (MM6 layout: extra A/X/M cols, no Grand Master / Stats)...
        {"#", "Fire Spells", "", "Res", "Short Name", "A", "X", "M", "Spell Description", "Normal",
         "Expert", "Master"},
        // ...then data. MM6 id 2 is "Flame Arrow", a spell that does not exist in MM7.
        {"2", "2", "Flame Arrow", "Fire", "Flame Arrow", "2", "1", "0", "desc", "norm", "exp", "mast"},
    });

    stats.Initialize(blob, GAME_VERSION_MM6);

    // Deferred: no MM6 spell names land on (the wrong) MM7 SpellId slots.
    EXPECT_TRUE(stats.pInfos[SPELL_FIRE_FIRE_BOLT].name.empty());
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

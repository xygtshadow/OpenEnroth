#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Testing/Game/GameTest.h"

#include "Application/Paths/GameVersion.h"

#include "Engine/Objects/Monsters.h"
#include "Engine/Objects/MonsterEnums.h"
#include "Engine/Spells/SpellEnums.h"

#include "Utility/Memory/Blob.h"

// Joins cells with '\t' and rows with '\r\n' to mimic the on-disk monsters.txt table format that
// MonsterStats::Initialize parses (it drops the first 4 header rows and splits the rest by "\r\n").
static Blob makeMonstersBlob(const std::vector<std::vector<std::string>> &rows) {
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

// MM6 monsters.txt has a different column layout from MM7: Name and Picture columns are swapped,
// there is only ONE spell attack (MM7 has two), only 6 resistances (Fire/Elec/Cold/Pois/Phys/Mag vs
// MM7's 10), and the Special-ability column sits at index 32. Parsing it with MM7 indices throws
// ("'' is not a number") because a short row tail lands where MM7 expects a numeric cell.
GAME_TEST(MonstersMm6, ParsesMm6ColumnLayout) {
    // Two MM6 rows, 33 columns each (indices 0-32). Resistances use distinct values to pin the
    // 6->10 remapping unambiguously.
    Blob blob = makeMonstersBlob({
        {"header1"},
        {"header2"},
        {"header3"},
        {"header4"},
        // id 1: a spell-caster with distinct resistances.
        {"1", "TestMonA", "Test Monster", "9", " 35 ", "14", " 171 ", "5%3D20+L1Bow", "0", "N",
         "Short", "Normal", "4", "140", "90", "0", "0", "Phys", "1D6+1", "Arrow", "0", "0", "0", "0",
         "40", "\"Fireball,N,5\"", "11", "22", "33", "44", "55", "66", "0"},
        // id 2: no spell, magic immunity ("Imm" -> 200), thousand-separated EXP.
        {"2", "TestMonB", "Big Test", "29", " 171 ", "22", "\" 1,131 \"", "0", "0", "Y", "Long",
         "Aggress", "4", "160", "80", "0", "0", "Phys", "3D6+3", "0", "0", "0", "0", "0", "0", "0",
         "10", "10", "10", "10", "0", "Imm", "0"},
    });

    MonsterStats stats;
    stats.Initialize(blob, GAME_VERSION_MM6);

    const MonsterInfo &a = stats.infos[static_cast<MonsterId>(1)];
    EXPECT_EQ(a.id, static_cast<MonsterId>(1));
    EXPECT_EQ(a.internalName, "TestMonA"); // MM6 col 1 (Picture) -> internalName.
    EXPECT_EQ(a.name, "Test Monster"); // MM6 col 2 (Name) -> display name.
    EXPECT_EQ(static_cast<int>(a.level), 9);
    EXPECT_EQ(a.hp, 35u);
    EXPECT_EQ(a.ac, 14u);
    EXPECT_EQ(a.exp, 171u);
    EXPECT_FALSE(a.flying);
    // One spell only; spell2 must be cleared.
    EXPECT_EQ(static_cast<int>(a.spell1UseChance), 40);
    EXPECT_EQ(a.spell1Id, SPELL_FIRE_FIREBALL);
    EXPECT_EQ(static_cast<int>(a.spell2UseChance), 0);
    EXPECT_EQ(a.spell2Id, SPELL_NONE);
    // 6 MM6 resistances remapped onto the 10 MM7 fields:
    // Fire->resFire, Elec->resAir, Cold->resWater, Pois->resEarth, Phys->resPhysical,
    // Mag->{resMind,resSpirit,resBody}; resLight = resDark = 0.
    EXPECT_EQ(static_cast<int>(a.resFire), 11);
    EXPECT_EQ(static_cast<int>(a.resAir), 22);
    EXPECT_EQ(static_cast<int>(a.resWater), 33);
    EXPECT_EQ(static_cast<int>(a.resEarth), 44);
    EXPECT_EQ(static_cast<int>(a.resPhysical), 55);
    EXPECT_EQ(static_cast<int>(a.resMind), 66);
    EXPECT_EQ(static_cast<int>(a.resSpirit), 66);
    EXPECT_EQ(static_cast<int>(a.resBody), 66);
    EXPECT_EQ(static_cast<int>(a.resLight), 0);
    EXPECT_EQ(static_cast<int>(a.resDark), 0);

    const MonsterInfo &b = stats.infos[static_cast<MonsterId>(2)];
    EXPECT_EQ(b.internalName, "TestMonB");
    EXPECT_EQ(b.name, "Big Test");
    EXPECT_EQ(b.exp, 1131u); // Quoted, thousand-separated.
    EXPECT_TRUE(b.flying);
    EXPECT_EQ(static_cast<int>(b.spell1UseChance), 0);
    EXPECT_EQ(b.spell1Id, SPELL_NONE);
    EXPECT_EQ(static_cast<int>(b.resFire), 10);
    EXPECT_EQ(static_cast<int>(b.resMind), 200); // "Imm" -> 200.
    EXPECT_EQ(static_cast<int>(b.resSpirit), 200);
    EXPECT_EQ(static_cast<int>(b.resBody), 200);
    EXPECT_EQ(static_cast<int>(b.resLight), 0);
    EXPECT_EQ(static_cast<int>(b.resDark), 0);
}

// Guards the MM7 parse path through the version-parameter refactor: the same parser must still read
// MM7's column layout (Name at 1, Picture at 2, two spell attacks at 24-27, 10 resistances at 28-37,
// Special at 38).
GAME_TEST(MonstersMm7, ParsesMm7ColumnLayout) {
    // A real MM7 row (Angel, id 1), 39 columns (indices 0-38).
    Blob blob = makeMonstersBlob({
        {"header1"},
        {"header2"},
        {"header3"},
        {"header4"},
        {"1", "Angel", "Angel A", "30", "180", "25", "1200", "5%50D20+L4Sword", "1", "Y", "Free",
         "Aggress", "3", "250", "60", "0", "0", "Phys", "2D8+10", "0", "0", "0", "0", "0",
         "30", "\"Light Bolt,M,8\"", "20", "\"Dispel Magic,M,8\"",
         "20", "20", "20", "20", "30", "15", "30", "Imm", "10", "20", "0"},
    });

    MonsterStats stats;
    stats.Initialize(blob, GAME_VERSION_MM7);

    const MonsterInfo &angel = stats.infos[MONSTER_ANGEL_A];
    EXPECT_EQ(angel.name, "Angel"); // MM7 col 1 (Name).
    EXPECT_EQ(angel.internalName, "Angel A"); // MM7 col 2 (Picture).
    EXPECT_EQ(static_cast<int>(angel.level), 30);
    EXPECT_EQ(angel.hp, 180u);
    EXPECT_EQ(angel.exp, 1200u);
    // MM7 has two spell attacks.
    EXPECT_EQ(static_cast<int>(angel.spell1UseChance), 30);
    EXPECT_EQ(angel.spell1Id, SPELL_LIGHT_LIGHT_BOLT);
    EXPECT_EQ(static_cast<int>(angel.spell2UseChance), 20);
    EXPECT_EQ(angel.spell2Id, SPELL_LIGHT_DISPEL_MAGIC);
    // MM7's full 10-resistance layout must be untouched by the MM6 branch.
    EXPECT_EQ(static_cast<int>(angel.resFire), 20);
    EXPECT_EQ(static_cast<int>(angel.resAir), 20);
    EXPECT_EQ(static_cast<int>(angel.resWater), 20);
    EXPECT_EQ(static_cast<int>(angel.resEarth), 20);
    EXPECT_EQ(static_cast<int>(angel.resMind), 30);
    EXPECT_EQ(static_cast<int>(angel.resSpirit), 15);
    EXPECT_EQ(static_cast<int>(angel.resBody), 30);
    EXPECT_EQ(static_cast<int>(angel.resLight), 200); // "Imm".
    EXPECT_EQ(static_cast<int>(angel.resDark), 10);
    EXPECT_EQ(static_cast<int>(angel.resPhysical), 20);
}

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Testing/Game/GameTest.h"

#include "Application/Paths/GameVersion.h"

#include "Engine/MapInfo.h"
#include "Engine/MapEnums.h"

#include "Media/Audio/SoundEnums.h"

#include "Utility/Memory/Blob.h"

// Joins cells with '\t' and rows with '\r\n' to mimic the on-disk *.txt table format that
// MapStats::Initialize parses (it drops the first 3 header rows and splits the rest by "\r\n").
static Blob makeMapStatsBlob(const std::vector<std::vector<std::string>> &rows) {
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

// MM6 mapstats.txt has a different column layout from MM7: no Per/Alert/Steal columns, per-monster
// blocks ordered (Pic, Name, Difficulty, Range) starting at index 13, music ("Redbook Track") at 25,
// and no EAX-environment column. Parsing it with MM7 indices throws ("'Demon' is not a number")
// because the Mon1 Pic string lands where MM7 expects encounter1Chance.
GAME_TEST(MapStatsMm6, ParsesMm6ColumnLayout) {
    // Two real MM6 rows (Sweet Water, Paradise Valley), 27 columns each (indices 0-26).
    Blob blob = makeMapStatsBlob({
        {"header1"},
        {"header2"},
        {"header3"},
        {"1", "Sweet Water", "OutA1.Odm", "0", "0", "224", "8", "9", "6", "40", "50", "50", "0",
         "Demon", "Devil Spawn", "3", " 2-4", "DemonFly", "Devil Captain", "3", " 2-4",
         "0", "0", "1", " 1-4", "5", "Peter"},
        {"2", "Paradise Valley", "OutA2.Odm", "0", "0", "168", "7", "8", "6", "20", "40", "30", "30",
         "DragonCover", "Red Dragon", "3", " 1-3", "Hydra", "Hydra", "3", " 2-4",
         "Titan", "Titan", "3", " 1-3", "16", "Peter"},
    });

    MapStats stats;
    stats.Initialize(blob, GAME_VERSION_MM6);

    const MapInfo &sweetWater = stats.pInfos[MAP_EMERALD_ISLAND]; // id 1.
    EXPECT_EQ(sweetWater.name, "Sweet Water");
    EXPECT_EQ(sweetWater.fileName, "outa1.odm"); // Lowercased on parse.
    EXPECT_EQ(sweetWater.respawnIntervalDays, 224u); // "Refil Days" (MM6 col 5).
    EXPECT_EQ(static_cast<int>(sweetWater.disarmDifficulty), 8); // "Lock 0-10" (MM6 col 6).
    EXPECT_EQ(static_cast<int>(sweetWater.trapDamageD20DiceCount), 9); // "Trap 0-10" (MM6 col 7).
    EXPECT_EQ(sweetWater.mapTreasureLevel, MAP_TREASURE_LEVEL_7); // "Tres 0-6" = 6 (MM6 col 8).
    EXPECT_EQ(static_cast<int>(sweetWater.encounterChance), 40); // "Enc %" (MM6 col 9).
    EXPECT_EQ(static_cast<int>(sweetWater.encounter1Chance), 50); // "M1 %" (MM6 col 10).
    EXPECT_EQ(static_cast<int>(sweetWater.encounter2Chance), 50); // "M2 %" (MM6 col 11).
    EXPECT_EQ(static_cast<int>(sweetWater.encounter3Chance), 0); // "M3 %" (MM6 col 12).
    // Encounters: MM6 stores (Pic, Name, Dif, Range); the Pic column is the internal name.
    EXPECT_EQ(sweetWater.encounter1MonsterInternalName, "Demon");
    EXPECT_EQ(static_cast<int>(sweetWater.Dif_M1), 3);
    EXPECT_EQ(sweetWater.encounter1MinCount, 2u);
    EXPECT_EQ(sweetWater.encounter1MaxCount, 4u);
    EXPECT_EQ(sweetWater.encounter2MonsterInternalName, "DemonFly");
    EXPECT_EQ(static_cast<int>(sweetWater.Dif_M2), 3);
    EXPECT_EQ(sweetWater.encounter2MinCount, 2u);
    EXPECT_EQ(sweetWater.encounter2MaxCount, 4u);
    EXPECT_EQ(sweetWater.encounter3MonsterInternalName, "0"); // No third monster.
    EXPECT_EQ(sweetWater.musicId, static_cast<MusicId>(5)); // "Redbook Track" (MM6 col 25).

    const MapInfo &paradise = stats.pInfos[MAP_HARMONDALE]; // id 2.
    EXPECT_EQ(paradise.name, "Paradise Valley");
    EXPECT_EQ(paradise.fileName, "outa2.odm");
    EXPECT_EQ(paradise.respawnIntervalDays, 168u);
    EXPECT_EQ(paradise.encounter1MonsterInternalName, "DragonCover");
    EXPECT_EQ(paradise.encounter3MonsterInternalName, "Titan");
    EXPECT_EQ(paradise.encounter3MinCount, 1u);
    EXPECT_EQ(paradise.encounter3MaxCount, 3u);
    EXPECT_EQ(paradise.musicId, static_cast<MusicId>(16));
}

// MM6 mapstats.txt pads the tail of the table with all-blank rows (tab-only cells). Because those
// rows are not literally empty, the "\r\n".skip("") filter does not drop them, so the parser used to
// crash on them with "'' is not a number" (an empty map-id cell hitting fromString<int>). The parser
// must skip rows whose map-id cell is blank and keep going.
GAME_TEST(MapStatsMm6, SkipsBlankSeparatorRows) {
    std::vector<std::string> blankRow(27, ""); // 27 tab-joined empty cells, mimicking the real padding.
    Blob blob = makeMapStatsBlob({
        {"header1"},
        {"header2"},
        {"header3"},
        {"1", "Sweet Water", "OutA1.Odm", "0", "0", "224", "8", "9", "6", "40", "50", "50", "0",
         "Demon", "Devil Spawn", "3", " 2-4", "DemonFly", "Devil Captain", "3", " 2-4",
         "0", "0", "1", " 1-4", "5", "Peter"},
        blankRow,
        {"2", "Paradise Valley", "OutA2.Odm", "0", "0", "168", "7", "8", "6", "20", "40", "30", "30",
         "DragonCover", "Red Dragon", "3", " 1-3", "Hydra", "Hydra", "3", " 2-4",
         "Titan", "Titan", "3", " 1-3", "16", "Peter"},
        blankRow,
        blankRow,
    });

    MapStats stats;
    stats.Initialize(blob, GAME_VERSION_MM6); // Must not throw on the blank rows.

    // Both real rows still parse; the blank rows between/after them are skipped.
    EXPECT_EQ(stats.pInfos[MAP_EMERALD_ISLAND].name, "Sweet Water"); // id 1.
    EXPECT_EQ(stats.pInfos[MAP_HARMONDALE].name, "Paradise Valley"); // id 2.
}

// Guards the MM7 parse path through the version-parameter refactor: the same parser must still read
// MM7's column layout (Per/Alert/Steal columns, per-monster Pic at 16/20/24, music at 28, EAX at 29).
GAME_TEST(MapStatsMm7, ParsesMm7ColumnLayout) {
    // A real MM7 row (Emerald Island), 34 columns (indices 0-33).
    Blob blob = makeMapStatsBlob({
        {"header1"},
        {"header2"},
        {"header3"},
        {"1", "Emerald Island", "Out01.Odm", "0", "0", "0", "672", "7", "0", "0", "1", "0", "10",
         "100", "0", "0", "Dragonfly", "Dragonfly", "1", " 2-5", "0", "0", "1", " 1-3",
         "0", "0", "1", " 1-3", "20", "FOREST", "0", "Training Island", "", "x"},
    });

    MapStats stats;
    stats.Initialize(blob, GAME_VERSION_MM7);

    const MapInfo &emerald = stats.pInfos[MAP_EMERALD_ISLAND];
    EXPECT_EQ(emerald.name, "Emerald Island");
    EXPECT_EQ(emerald.fileName, "out01.odm");
    EXPECT_EQ(emerald.respawnIntervalDays, 672u); // MM7 "Refil Days" (col 6).
    EXPECT_EQ(static_cast<int>(emerald.trapDamageD20DiceCount), 1); // MM7 "Trap" (col 10).
    EXPECT_EQ(static_cast<int>(emerald.encounterChance), 10); // MM7 "Enc %" (col 12).
    EXPECT_EQ(static_cast<int>(emerald.encounter1Chance), 100); // MM7 "M1 %" (col 13).
    EXPECT_EQ(emerald.encounter1MonsterInternalName, "Dragonfly"); // MM7 Mon1 Pic (col 16).
    EXPECT_EQ(static_cast<int>(emerald.Dif_M1), 1); // MM7 (col 18).
    EXPECT_EQ(emerald.encounter1MinCount, 2u);
    EXPECT_EQ(emerald.encounter1MaxCount, 5u);
    EXPECT_EQ(emerald.musicId, static_cast<MusicId>(20)); // MM7 "Redbook Track" (col 28).
    EXPECT_EQ(static_cast<int>(emerald.uEAXEnv), 15); // MM7 EAX "FOREST" (col 29).
}

#include "MapInfo.h"

#include <array>
#include <map>
#include <string>

#include "Library/Serialization/Serialization.h"

#include "Utility/MapAccess.h"
#include "Utility/Memory/Blob.h"
#include "Utility/String/Ascii.h"
#include "Utility/String/Split.h"
#include "Utility/String/Transformations.h"

MapStats *pMapStats;

void MapStats::Initialize(const Blob &mapStats, GameVersion version) {
    // mapstats.txt table structure: map id | name (localized) | file name | ... |
    //                               map designer (set only in mm6, not used) | dev notes | parent map (not used).
    static const std::map<std::string, uint8_t, ascii::NoCaseLess> eaxEnvMap = {
        {"GENERIC", 0},
        {"PADDEDCELL", 1},
        {"ROOM", 2},
        {"BATHROOM", 3},
        {"LIVINGROOM", 4},
        {"STONEROOM", 5},
        {"AUDITORIUM", 6},
        {"CONCERTHALL", 7},
        {"CAVE", 8},
        {"ARENA", 9},
        {"HANGAR", 10},
        {"CARPETEDHALLWAY", 11},
        {"HALLWAY", 12},
        {"STONECORRIDOR", 13},
        {"ALLEY", 14},
        {"FOREST", 15},
        {"CITY", 16},
        {"MOUNTAIN", 17},
        {"QUARRY", 18},
        {"PLAINS", 19},
        {"PARKINGLOT", 20},
        {"SEWERPIPE", 21},
        {"UNDERWATER", 22},
        {"DRUGGED", 23},
        {"DIZZY", 24},
        {"PSYCHOTIC", 25},
    };

    auto parseRange = [](std::string_view s, uint8_t *minOut, uint8_t *maxOut) {
        // Range cells can have leading whitespace (e.g. " 2-5"), so trim before each fromString.
        auto dash = s.find('-');
        if (dash == std::string_view::npos) {
            *minOut = fromString<int>(trim(s));
            *maxOut = *minOut;
        } else {
            *minOut = fromString<int>(trim(s.substr(0, dash)));
            *maxOut = fromString<int>(trim(s.substr(dash + 1)));
        }
    };

    for (std::string_view line : split(mapStats.str()).by("\r\n").drop(3).skip("")) {
        std::array<std::string_view, 30> tokens = split(line).by('\t');
        MapId mapId = static_cast<MapId>(fromString<int>(tokens[0]));
        MapInfo &info = pInfos[mapId];
        info.name = removeQuotes(tokens[1]);
        info.fileName = ascii::toLower(removeQuotes(tokens[2]));
        info.numResets = fromString<int>(tokens[3]);
        info.firstVisitedAt = fromString<int>(tokens[4]);

        if (version == GAME_VERSION_MM6) {
            // MM6 mapstats.txt has fewer columns and a different layout than MM7: no Per/Alert/Steal
            // columns, treasure/encounter chances shifted up, per-monster blocks ordered
            // (Pic, Name, Difficulty, Range) starting at index 13, music ("Redbook Track") at 25,
            // and no EAX-environment column. The Pic column is the encounter monster's internal name.
            info.respawnIntervalDays = fromString<int>(tokens[5]); // "Refil Days".
            info.disarmDifficulty = fromString<int>(tokens[6]); // "Lock 0-10".
            info.trapDamageD20DiceCount = fromString<int>(tokens[7]); // "Trap 0-10".
            info.mapTreasureLevel = static_cast<MapTreasureLevel>(fromString<int>(tokens[8])); // "Tres 0-6".
            info.encounterChance = fromString<int>(tokens[9]); // "Enc %".
            info.encounter1Chance = fromString<int>(tokens[10]); // "M1 %".
            info.encounter2Chance = fromString<int>(tokens[11]); // "M2 %".
            info.encounter3Chance = fromString<int>(tokens[12]); // "M3 %".
            info.encounter1MonsterInternalName = removeQuotes(tokens[13]); // "Mon1 Pic".
            info.Dif_M1 = fromString<int>(tokens[15]);
            parseRange(tokens[16], &info.encounter1MinCount, &info.encounter1MaxCount);
            info.encounter2MonsterInternalName = removeQuotes(tokens[17]); // "Mon2 Pic".
            info.Dif_M2 = fromString<int>(tokens[19]);
            parseRange(tokens[20], &info.encounter2MinCount, &info.encounter2MaxCount);
            info.encounter3MonsterInternalName = removeQuotes(tokens[21]); // "Mon3 Pic".
            info.Dif_M3 = fromString<int>(tokens[23]);
            parseRange(tokens[24], &info.encounter3MinCount, &info.encounter3MaxCount);
            info.musicId = static_cast<MusicId>(fromString<int>(tokens[25])); // "Redbook Track".
            // MM6 lacks these columns; default them (matches the MM7 data convention / fallback).
            info.perceptionDifficulty = 0;
            info.alertDays = 7;
            info.baseStealingFine = 0;
            info.uEAXEnv = 26;
        } else {
            info.perceptionDifficulty = fromString<int>(tokens[5]);
            info.respawnIntervalDays = fromString<int>(tokens[6]);
            info.alertDays = fromString<int>(tokens[7]);
            info.baseStealingFine = fromString<int>(tokens[8]);
            info.disarmDifficulty = fromString<int>(tokens[9]);
            info.trapDamageD20DiceCount = fromString<int>(tokens[10]);
            info.mapTreasureLevel = static_cast<MapTreasureLevel>(fromString<int>(tokens[11]));
            info.encounterChance = fromString<int>(tokens[12]);
            info.encounter1Chance = fromString<int>(tokens[13]);
            info.encounter2Chance = fromString<int>(tokens[14]);
            info.encounter3Chance = fromString<int>(tokens[15]);
            info.encounter1MonsterInternalName = removeQuotes(tokens[16]);
            info.Dif_M1 = fromString<int>(tokens[18]);
            parseRange(tokens[19], &info.encounter1MinCount, &info.encounter1MaxCount);
            info.encounter2MonsterInternalName = removeQuotes(tokens[20]);
            info.Dif_M2 = fromString<int>(tokens[22]);
            parseRange(tokens[23], &info.encounter2MinCount, &info.encounter2MaxCount);
            info.encounter3MonsterInternalName = removeQuotes(tokens[24]);
            info.Dif_M3 = fromString<int>(tokens[26]);
            parseRange(tokens[27], &info.encounter3MinCount, &info.encounter3MaxCount);
            info.musicId = static_cast<MusicId>(fromString<int>(tokens[28]));
            info.uEAXEnv = valueOr(eaxEnvMap, std::string(tokens[29]), 26);
        }
    }
}

MapId MapStats::GetMapInfo(std::string_view Str2) {
    std::string map_name = ascii::toLower(Str2);

    for (MapId i : pInfos.indices()) {
        if (pInfos[i].fileName == map_name) {
            return i;
        }
    }

    assert(false);
    return MAP_INVALID;
}

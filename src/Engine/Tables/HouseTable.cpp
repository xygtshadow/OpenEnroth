#include "HouseTable.h"

#include <array>
#include <map>
#include <string>
#include <string_view>

#include "Engine/Data/HouseEnums.h"

#include "Library/Logger/Logger.h"
#include "Library/Serialization/Serialization.h"

#include "Utility/MapAccess.h"
#include "Utility/Memory/Blob.h"
#include "Utility/String/Ascii.h"
#include "Utility/String/Split.h"
#include "Utility/String/Transformations.h"

IndexedArray<HouseData, HOUSE_FIRST, HOUSE_LAST> houseTable;

void initializeHouses(const Blob &houses, GameVersion version) {
    // 2devents.txt table structure (column names are the headers from the data file):
    //  0: "#"                  - house id
    //  1: "#"                  - per-type sequence number, resets at each new Type         (not used)
    //  2: "Type"               - house type                                                (not localized)
    //  3: "Map"                - map id this building lives on                             (not used)
    //  4: "Picture"            - index into `pAnimatedRooms`, for npc id, video & sound
    //  5: "Name"               - house name                                                (localized)
    //  6: "Proprietor Name"                                                                (localized)
    //  7: "Proprietor Title"                                                               (localized)
    //  8: "Picture"            - always 0
    //  9: "State"              - always 0
    // 10: "Rep"                - always 0, reputation?
    // 11: "Per"                - always 0
    // 12: "Val"                - shop price multiplier, float
    // 13: "A"                  - skill/spell price multiplier, float
    // 14: "B"                  - always empty
    // 15: "C"                  - item-generation interval, days
    // 16: "Notes:"             - mostly empty, an alternative index into `pAnimatedRooms`,
    //                            points at the base entry of a race-tier triplet           (not used)
    // 17: "Notes(2):"          - max trainable level for Training houses                   (not used)
    // 18: "Open"               - opening hour, 0-24
    // 19: "Closed"             - closing hour, 0-24
    // 20: "Pic"                - exit picture id                                           (not used in MM7)
    // 21: "Map"                - exit map id                                               (not used in MM7)
    // 22: "Restrictions"       - exit gating quest bit                                     (not used in MM7)
    // 23: "Text"               - exit text                                                 (not used in MM7)
    static const std::map<std::string, HouseType, ascii::NoCaseLess> houseTypeMap = {
        {"Weapon Shop", HOUSE_TYPE_WEAPON_SHOP},
        {"Armor Shop", HOUSE_TYPE_ARMOR_SHOP},
        {"Magic Shop", HOUSE_TYPE_MAGIC_SHOP},
        {"Alchemist", HOUSE_TYPE_ALCHEMY_SHOP},
        {"Stables", HOUSE_TYPE_STABLE},
        {"Boats", HOUSE_TYPE_BOAT},
        {"Temple", HOUSE_TYPE_TEMPLE},
        {"Training", HOUSE_TYPE_TRAINING_GROUND},
        {"Town Hall", HOUSE_TYPE_TOWN_HALL},
        {"Tavern", HOUSE_TYPE_TAVERN},
        {"Bank", HOUSE_TYPE_BANK},
        {"Fire Guild", HOUSE_TYPE_FIRE_GUILD},
        {"Air Guild", HOUSE_TYPE_AIR_GUILD},
        {"Water Guild", HOUSE_TYPE_WATER_GUILD},
        {"Earth Guild", HOUSE_TYPE_EARTH_GUILD},
        {"Spirit Guild", HOUSE_TYPE_SPIRIT_GUILD},
        {"Mind Guild", HOUSE_TYPE_MIND_GUILD},
        {"Body Guild", HOUSE_TYPE_BODY_GUILD},
        {"Light Guild", HOUSE_TYPE_LIGHT_GUILD},
        {"Dark Guild", HOUSE_TYPE_DARK_GUILD},
        {"Element Guild", HOUSE_TYPE_ELEMENTAL_GUILD}, // This is MM6 only.
        {"Self Guild", HOUSE_TYPE_SELF_GUILD},
        {"Mirrored Path Guild", HOUSE_TYPE_MIRRORED_PATH_GUILD},
        {"Mercenary Guild", HOUSE_TYPE_TOWN_HALL}, // This is MM6 only. TODO(captainurist): Is this right and not Merc Guild (18)?
    };

    // MM6's 2dEvents.txt keeps the same 24-column layout as MM7, but stores free-form text in several
    // columns the engine reads as numbers: the shop-stock columns A/C (13/15) describe stock as
    // level+category text (e.g. "L1 Weap", "L2 Misc"), and the auxiliary Picture/exit columns (8, 20-22)
    // carry editor annotations (e.g. "2 story poor house", "Throne", "Need Key"). For MM6 we tolerate a
    // non-numeric value and leave the field at its default; MM7 stays strict so genuine regressions still
    // throw. MM6's shop inventory ultimately couples to the deferred item model (docs/pending/mm6-item-model.md);
    // see docs/pending/mm6-2devents-houses.md.
    auto parseNum = [version](std::string_view token, auto fallback) {
        decltype(fallback) value = fallback;
        if (tryDeserialize(token, &value))
            return value;
        if (version == GAME_VERSION_MM6)
            return fallback; // Tolerate MM6's descriptive text in numeric columns.
        return fromString<decltype(fallback)>(token); // MM7: rethrow the original "not a number" error.
    };

    int skippedMm6Houses = 0;
    for (std::string_view line : split(houses.str()).by("\r\n").drop(2).skip("")) {
        // Some lines have only ~12 cols, and some cols are empty, so need both resize & replace.
        std::array<std::string_view, 24> tokens = split(line).by('\t').replace("", "0").resize(24, "0");

        // TODO(captainurist): We don't check if int is in range. A better way would be to deal away with enums
        //                     entirely, and just use typed ids. Do this once we iron out the details of how #mm6
        //                     enums will be handled by the engine. Also apply to other table parsers.
        int rawHouseId = fromString<int>(tokens[0]);

        // MM6 defines 557 houses (ids 1-557) where the engine's HouseId enum / houseTable is MM7-shaped
        // (HOUSE_FIRST..HOUSE_LAST = 1..525). MM6 ids 1-525 line up positionally with MM7's building slots
        // (both tables start with the weapon shops and follow the same shop/guild structure), but MM6's
        // trailing 32 entries (526-557: extra residences, tents, wagons) have no MM7 slot and would index
        // out of range. Skip them for MM6 so bring-up proceeds; the full MM6 house set is deferred.
        if (version == GAME_VERSION_MM6 &&
            (rawHouseId < static_cast<int>(HOUSE_FIRST) || rawHouseId > static_cast<int>(HOUSE_LAST))) {
            ++skippedMm6Houses;
            continue;
        }

        HouseId houseId = static_cast<HouseId>(rawHouseId);
        houseTable[houseId].uType = valueOr(houseTypeMap, tokens[2], HOUSE_TYPE_MERCENARY_GUILD);
        houseTable[houseId].uAnimationID = parseNum(tokens[4], 0);
        houseTable[houseId].name = removeQuotes(tokens[5]);
        houseTable[houseId].pProprieterName = removeQuotes(tokens[6]);
        houseTable[houseId].pProprieterTitle = removeQuotes(tokens[7]);
        houseTable[houseId].field_14 = parseNum(tokens[8], 0);
        houseTable[houseId]._state = parseNum(tokens[9], 0);
        houseTable[houseId]._rep = parseNum(tokens[10], 0);
        houseTable[houseId]._per = parseNum(tokens[11], 0);
        houseTable[houseId].fPriceMultiplier = parseNum(tokens[12], 0.0f);
        houseTable[houseId].flt_24 = parseNum(tokens[13], 0.0f);
        houseTable[houseId].generation_interval_days = parseNum(tokens[15], 0);
        houseTable[houseId].uOpenTime = parseNum(tokens[18], 0);
        houseTable[houseId].uCloseTime = parseNum(tokens[19], 0);
        houseTable[houseId].uExitPicID = parseNum(tokens[20], 0);
        houseTable[houseId].uExitMapID = static_cast<MapId>(parseNum(tokens[21], 0));
        houseTable[houseId]._quest_bit = static_cast<QuestBit>(parseNum(tokens[22], 0));
        houseTable[houseId].pEnterText = removeQuotes(tokens[23]);
    }

    if (skippedMm6Houses > 0)
        logger->warning("Skipped {} MM6 houses with ids outside the MM7-shaped HouseId range (1-{}); MM6's extra "
                        "residences/tents/wagons and its text-based shop stock need a version-aware house model.",
                        skippedMm6Houses, static_cast<int>(HOUSE_LAST));
}

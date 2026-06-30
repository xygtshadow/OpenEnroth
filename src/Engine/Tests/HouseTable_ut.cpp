#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Testing/Game/GameTest.h"

#include "Application/Paths/GameVersion.h"

#include "Engine/Tables/HouseTable.h"
#include "Engine/Data/HouseEnums.h"

#include "Utility/Memory/Blob.h"

// Joins cells with '\t' and rows with '\r\n' to mimic the on-disk 2dEvents.txt table format that
// initializeHouses parses (it drops the first 2 header rows, then splits the rest by "\r\n").
static Blob makeHousesBlob(const std::vector<std::vector<std::string>> &rows) {
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

// MM6's 2dEvents.txt keeps the same 24-column layout as MM7, but several "numeric" columns instead hold
// free-form text: the shop-stock columns A/C (13/15) describe stock as level+category text (e.g.
// "L1 Weap"), and the auxiliary Picture/exit columns (8, 20-22) carry editor annotations (e.g. "2 story
// poor house", "Throne", "Need Key"). Parsing those with MM7's numeric reads throws ("'L1 Weap' is not a
// number"). MM6 also defines 557 houses (ids 1-557) where the engine's HouseId enum / houseTable is
// MM7-shaped (1-525), so ids 526-557 (extra residences/tents/wagons) would index out of range. For MM6
// the parser tolerates non-numeric numeric columns (leaving those fields at their defaults) and skips the
// out-of-range houses, so engine bring-up proceeds; the faithful MM6 house/shop model is deferred (see
// docs/pending/mm6-2devents-houses.md, coupled to docs/pending/mm6-item-model.md).
GAME_TEST(HouseTableMm6, ToleratesTextColumnsAndSkipsOutOfRangeIds) {
    // id 1 (in range, the real crash): "L1 Weap" in the A column. id 530 (out of range >525): would crash
    // several times over if parsed - placed in the middle so the trailing id-204 row proves parsing
    // continues past the skip. id 204 (in range): descriptive text in the auxiliary numeric columns.
    Blob blob = makeHousesBlob({
        {"2D Events by Type"},                                                                       // Header 1.
        {"#", "#", "Type", "Map", "Picture", "Name", "Proprieter Name", "Title", "Picture", "State", // Header 2.
         "Rep", "Per", "Val", "A", "B", "C", "Notes:", "Notes(2):", "Open", "Closed", "Pic", "Map", "Restrictions", "Text"},
        {"1", "1", "Weapon Shop", "E3", "2", "The Knife Shoppe", "Caine", "Blacksmith", "0", "0",
         "0", "0", "1.5", "L1 Weap", "L2 Dagger", "2", "Weapon Shop gets:", "", "6", "18", "0", "0", "0", ""},
        {"530", "", "House", "P8", "0", "An out-of-range MM6 house", "", "", "2 story poor house", "0",
         "0", "0", "1", "L1 Misc", "", "-", "", "", "6", "18", "Throne", "2D 154", "Need Key", ""},
        {"204", "", "House", "C2", "5", "A Poor House", "", "", "2 story poor house", "0",
         "0", "0", "1", "L2 Misc", "", "-", "", "", "6", "18", "Throne", "2D 154", "Need Key", ""},
    });

    // initializeHouses writes the file-scope global houseTable; snapshot and restore the slots we touch so
    // the test stays hermetic for other tests sharing the process.
    HouseData saved1 = houseTable[HOUSE_WEAPON_SHOP_EMERALD_ISLAND];
    HouseData saved204 = houseTable[HOUSE_204];

    EXPECT_NO_THROW(initializeHouses(blob, GAME_VERSION_MM6));

    // id 1: the essential fields parse; the non-numeric A column (flt_24) is tolerated and stays at 0.
    const HouseData &shop = houseTable[HOUSE_WEAPON_SHOP_EMERALD_ISLAND];
    EXPECT_EQ(shop.uType, HOUSE_TYPE_WEAPON_SHOP);
    EXPECT_EQ(shop.uAnimationID, 2u);
    EXPECT_EQ(shop.name, "The Knife Shoppe");
    EXPECT_EQ(shop.pProprieterName, "Caine");
    EXPECT_FLOAT_EQ(shop.fPriceMultiplier, 1.5f);
    EXPECT_FLOAT_EQ(shop.flt_24, 0.0f); // "L1 Weap" tolerated -> default.
    EXPECT_EQ(shop.generation_interval_days, 2);
    EXPECT_EQ(shop.uOpenTime, 6u);
    EXPECT_EQ(shop.uCloseTime, 18u);

    // id 204 parsed (so the in-between out-of-range id 530 was skipped, not fatal), with the auxiliary
    // text columns tolerated and left at their numeric defaults.
    const HouseData &house = houseTable[HOUSE_204];
    EXPECT_EQ(house.name, "A Poor House");
    EXPECT_FLOAT_EQ(house.fPriceMultiplier, 1.0f);
    EXPECT_EQ(house.field_14, 0);                  // "2 story poor house" tolerated.
    EXPECT_FLOAT_EQ(house.flt_24, 0.0f);           // "L2 Misc" tolerated.
    EXPECT_EQ(house.generation_interval_days, 0);  // "-" tolerated.
    EXPECT_EQ(house.uExitPicID, 0);                // "Throne" tolerated.

    houseTable[HOUSE_WEAPON_SHOP_EMERALD_ISLAND] = saved1;
    houseTable[HOUSE_204] = saved204;
}

// Guards the MM7 parse path through the version-parameter refactor: MM7's columns are numeric, so the A
// column (flt_24) and the auxiliary columns must still read as numbers (no tolerance applied for MM7).
GAME_TEST(HouseTableMm7, ParsesNumericColumns) {
    Blob blob = makeHousesBlob({
        {"2D Events by Type"},
        {"#", "#", "Type", "Map", "Picture", "Name", "Proprieter Name", "Title", "Picture", "State",
         "Rep", "Per", "Val", "A", "B", "C", "Notes:", "Notes(2):", "Open", "Closed", "Pic", "Map", "Restrictions", "Text"},
        {"1", "1", "Weapon Shop", "1", "57", "The Knight's Blade", "Tor", "Blacksmith", "0", "0",
         "0", "0", "1.5", "1", "", "7", "", "", "6", "18", "0", "0", "0", "0"},
    });

    HouseData saved1 = houseTable[HOUSE_WEAPON_SHOP_EMERALD_ISLAND];

    EXPECT_NO_THROW(initializeHouses(blob, GAME_VERSION_MM7));

    const HouseData &shop = houseTable[HOUSE_WEAPON_SHOP_EMERALD_ISLAND];
    EXPECT_EQ(shop.uType, HOUSE_TYPE_WEAPON_SHOP);
    EXPECT_EQ(shop.uAnimationID, 57u);
    EXPECT_EQ(shop.name, "The Knight's Blade");
    EXPECT_FLOAT_EQ(shop.fPriceMultiplier, 1.5f);
    EXPECT_FLOAT_EQ(shop.flt_24, 1.0f); // Numeric A column still read for MM7.
    EXPECT_EQ(shop.generation_interval_days, 7);
    EXPECT_EQ(shop.uOpenTime, 6u);
    EXPECT_EQ(shop.uCloseTime, 18u);

    houseTable[HOUSE_WEAPON_SHOP_EMERALD_ISLAND] = saved1;
}

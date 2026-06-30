#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Testing/Game/GameTest.h"

#include "Application/Paths/GameVersion.h"

#include "Engine/Tables/ItemTable.h"
#include "Engine/Objects/CharacterEnums.h"
#include "Engine/Objects/ItemEnums.h"

#include "Utility/Memory/Blob.h"

// Joins cells with '\t' and rows with '\r\n' to mimic the on-disk stditems.txt / spcitems.txt table
// format that ItemTable parses (it drops the first 4 header rows, then splits the rest by "\r\n" and
// skips empty lines).
static Blob makeTableBlob(const std::vector<std::vector<std::string>> &rows) {
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

// One stditems "Standard Bonuses by Group" row: name, suffix, then the 9 per-item-type chances
// (Arm, Shld, Helm, Belt, Cape, Gaunt, Boot, Ring, Amul).
static std::vector<std::string> bonusRow(std::string name, std::string suffix, int arm, int amul) {
    return {std::move(name), std::move(suffix),
            std::to_string(arm), "0", "10", "10", "10", "10", "5", "10", std::to_string(amul)};
}

// MM6's stditems.txt lists only 14 standard bonuses (Might..Poison Resistance) where MM7 lists 24.
// MM6's elemental resistances remap onto the MM7 Attribute enum exactly as the monster parser does
// (Elec->Air, Cold->Water, Poison->Earth), so MM6's 14 bonuses are a clean prefix of the enchantable
// range. Parsing MM6 with the MM7-shaped 24-attribute layout throws ("'' is not a number") because
// the section-2 header rows land where the 24-row loop still expects numeric chance cells.
GAME_TEST(StdItemsMm6, ParsesMm6Layout) {
    Blob blob = makeTableBlob({
        {"Standard Bonuses by Group"},
        {""},
        {"Bonus Stat", "Of Name", "Arm", "Shld", "Helm", "Belt", "Cape", "Gaunt", "Boot", "Ring", "Amul"},
        {"", "", "", ""}, // Tab-only separator row, dropped by the leading drop(4).
        bonusRow("Might", "of Might", 5, 91),
        bonusRow("Intellect", "of Thought", 6, 92),
        bonusRow("Personality", "of Charm", 7, 93),
        bonusRow("Endurance", "of Vigor", 8, 94),
        bonusRow("Accuracy", "of Precision", 9, 95),
        bonusRow("Speed", "of Speed", 10, 96),
        bonusRow("Luck", "of Luck", 11, 97),
        bonusRow("Hit Points", "of Health", 12, 98),
        bonusRow("Spell Points", "of Magic", 13, 99),
        bonusRow("Armor Class", "of Defense", 14, 100),
        bonusRow("Fire Resistance", "of Fire Resistance", 15, 101),
        bonusRow("Elec Resistance", "of Elec Resistance", 16, 102), // MM6 Elec -> MM7 Air.
        bonusRow("Cold Resistance", "of Cold Resistance", 17, 103), // MM6 Cold -> MM7 Water.
        bonusRow("Poison Resistance", "of Poison Resistance", 18, 104), // MM6 Poison -> MM7 Earth.
        {""},
        {"", "", "115", "70", "140", "155", "110", "140", "140"}, // Sum row (between sections, not parsed).
        {""},
        {"", "Bonus range for Standard by Level"},
        {"", "lvl", "min", "max"},
        {"", "1", "0", "0"},
        {"", "2", "1", "5"},
        {"", "3", "3", "8"},
        {"", "4", "6", "12"},
        {"", "5", "10", "17"},
        {"", "6", "15", "25"},
    });

    ItemTable table;
    table.LoadStandardEnchantments(blob, GAME_VERSION_MM6);

    // First 11 attributes map 1:1 with MM7.
    EXPECT_EQ(table.standardEnchantments[ATTRIBUTE_MIGHT].attributeName, "Might");
    EXPECT_EQ(table.standardEnchantments[ATTRIBUTE_MIGHT].itemSuffix, "of Might");
    EXPECT_EQ(static_cast<int>(table.standardEnchantments[ATTRIBUTE_MIGHT].chanceByItemType[ITEM_TYPE_ARMOUR]), 5);
    EXPECT_EQ(static_cast<int>(table.standardEnchantments[ATTRIBUTE_MIGHT].chanceByItemType[ITEM_TYPE_AMULET]), 91);
    EXPECT_EQ(table.standardEnchantments[ATTRIBUTE_RESIST_FIRE].attributeName, "Fire Resistance");

    // The three MM6 elemental resistances land in the MM7 Air/Water/Earth slots.
    EXPECT_EQ(table.standardEnchantments[ATTRIBUTE_RESIST_AIR].attributeName, "Elec Resistance");
    EXPECT_EQ(table.standardEnchantments[ATTRIBUTE_RESIST_AIR].itemSuffix, "of Elec Resistance");
    EXPECT_EQ(table.standardEnchantments[ATTRIBUTE_RESIST_WATER].attributeName, "Cold Resistance");
    EXPECT_EQ(table.standardEnchantments[ATTRIBUTE_RESIST_EARTH].attributeName, "Poison Resistance");
    EXPECT_EQ(static_cast<int>(table.standardEnchantments[ATTRIBUTE_RESIST_EARTH].chanceByItemType[ITEM_TYPE_AMULET]), 104);

    // MM7-only attributes (Mind/Body resist + the 8 skills) are absent from MM6 and stay at defaults.
    EXPECT_TRUE(table.standardEnchantments[ATTRIBUTE_RESIST_MIND].attributeName.empty());
    EXPECT_TRUE(table.standardEnchantments[ATTRIBUTE_SKILL_UNARMED].attributeName.empty());

    // Section 2 (per-treasure-level bonus ranges) must still be found despite the shorter section 1.
    EXPECT_EQ(table.standardEnchantmentRangeByTreasureLevel[ITEM_TREASURE_LEVEL_1].front(), 0);
    EXPECT_EQ(table.standardEnchantmentRangeByTreasureLevel[ITEM_TREASURE_LEVEL_1].back(), 0);
    EXPECT_EQ(table.standardEnchantmentRangeByTreasureLevel[ITEM_TREASURE_LEVEL_6].front(), 15);
    EXPECT_EQ(table.standardEnchantmentRangeByTreasureLevel[ITEM_TREASURE_LEVEL_6].back(), 25);
}

// Guards the MM7 parse path through the version-parameter refactor: the full 24-attribute layout (which
// adds Mind/Body resistances and 8 skill bonuses) must still read correctly, with section 2 located via
// the unchanged "+3" offset.
GAME_TEST(StdItemsMm7, ParsesMm7Layout) {
    std::vector<std::vector<std::string>> rows = {
        {"Standard Bonuses by Group"},
        {""},
        {"Bonus Stat", "Of Name", "Arm", "Shld", "Helm", "Belt", "Cape", "Gaunt", "Boot", "Ring", "Amul"},
        {""},
        bonusRow("Might", "of Might", 5, 50),
        bonusRow("Intellect", "of Thought", 5, 50),
        bonusRow("Personality", "of Charm", 5, 50),
        bonusRow("Endurance", "of Vigor", 5, 50),
        bonusRow("Accuracy", "of Precision", 5, 50),
        bonusRow("Speed", "of Speed", 5, 50),
        bonusRow("Luck", "of Luck", 5, 50),
        bonusRow("Hit Points", "of Health", 10, 50),
        bonusRow("Spell Points", "of Magic", 10, 50),
        bonusRow("Armor Class", "of Defense", 20, 50),
        bonusRow("Fire Resistance", "of Fire Resistance", 10, 50),
        bonusRow("Air Resistance", "of Air Resistance", 10, 50),
        bonusRow("Water Resistance", "of Water Resistance", 10, 50),
        bonusRow("Earth Resistance", "of Earth Resistance", 10, 50),
        bonusRow("Mind Resistance", "of Mind Resistance", 10, 50),
        bonusRow("Body Resistance", "of Body Resistance", 10, 50),
        bonusRow("Alchemy skill", "of Alchemy", 0, 15),
        bonusRow("Stealing skill", "of Stealing", 0, 5),
        bonusRow("Disarm skill", "of Disarming", 0, 5),
        bonusRow("ID Item skill", "of Items", 0, 15),
        bonusRow("ID Monster skill", "of Monsters", 0, 15),
        bonusRow("Armsmaster skill", "of Arms", 10, 0),
        bonusRow("Dodge skill", "of Dodging", 0, 5),
        bonusRow("Unarmed skill", "of the Fist", 0, 5),
        {""},
        {"", "", "115", "70", "115", "70", "140", "155", "110", "140", "140"},
        {""},
        {"", "Bonus range for Standard by Level"},
        {"", "lvl", "min", "max"},
        {"", "1", "0", "0"},
        {"", "2", "1", "5"},
        {"", "3", "3", "8"},
        {"", "4", "6", "12"},
        {"", "5", "10", "17"},
        {"", "6", "15", "25"},
    };
    Blob blob = makeTableBlob(rows);

    ItemTable table;
    table.LoadStandardEnchantments(blob, GAME_VERSION_MM7);

    EXPECT_EQ(table.standardEnchantments[ATTRIBUTE_MIGHT].attributeName, "Might");
    // MM7 keeps its own elemental names in the Air/Water/Earth slots.
    EXPECT_EQ(table.standardEnchantments[ATTRIBUTE_RESIST_AIR].attributeName, "Air Resistance");
    EXPECT_EQ(table.standardEnchantments[ATTRIBUTE_RESIST_BODY].attributeName, "Body Resistance");
    // The skill bonuses at the tail of the 24-attribute set must be reached.
    EXPECT_EQ(table.standardEnchantments[ATTRIBUTE_SKILL_UNARMED].attributeName, "Unarmed skill");
    EXPECT_EQ(table.standardEnchantments[ATTRIBUTE_SKILL_UNARMED].itemSuffix, "of the Fist");

    EXPECT_EQ(table.standardEnchantmentRangeByTreasureLevel[ITEM_TREASURE_LEVEL_6].front(), 15);
    EXPECT_EQ(table.standardEnchantmentRangeByTreasureLevel[ITEM_TREASURE_LEVEL_6].back(), 25);
}

// One spcitems "Special Bonuses by Group" row: description, suffix, the 12 per-item-type chances
// (W1, W2, Miss, Arm, Shld, Helm, Belt, Cape, Gaunt, Boot, Ring, Amul), gold value, level letter, text.
static std::vector<std::string> spcRow(std::string description, std::string suffix, std::string value, std::string level) {
    return {std::move(description), std::move(suffix),
            "0", "0", "0", "10", "10", "10", "0", "10", "0", "0", "10", "10",
            std::move(value), std::move(level), "text"};
}

// MM6's spcitems.txt lists fewer special enchantments than MM7 (59 vs 72) and its set is a positional
// prefix of MM7's. Because the engine's ItemEnchantment enum is MM7-shaped (72 entries), the parser's
// zip would otherwise run past MM6's shorter data into the trailing sum/legend section and parse the
// section's empty "Value" cell ("'' is not a number"). The MM6 branch stops at the first row with an
// empty suffix ("Name Add"), which marks the section boundary.
GAME_TEST(SpcItemsMm6, StopsAtShorterSet) {
    Blob blob = makeTableBlob({
        {"Special Bonuses by Group"},
        {""},
        {"Bonus Stat", "Name Add", "W1", "W2", "Miss", "Arm", "Shld", "Helm", "Belt", "Cape", "Gaunt", "Boot", "Ring", "Amul", "Value", "Lvl", "Description"},
        {""},
        spcRow("Plus 10 to all Resistances.", "of Protection", "1000", "B"),
        spcRow("Plus 10 to all Seven Statistics.", "of The Gods", "3000", "D"),
        spcRow("Increased Value.", "Antique", "X 10", "B"),
        {""},
        {"", "", "437", "437", "447", "307", "272", "282", "232", "282", "217", "182", "347", "347"}, // Sum row: empty suffix marks the boundary.
        {""},
        {"Treasure Level", "If treasure Level = 3 add all A and B"}, // Legend rows that must never be parsed as data.
        {"A= 3 to 4", "If treasure Level = 4 add all A and B and C"},
    });

    ItemTable table;
    table.LoadSpecialEnchantments(blob, GAME_VERSION_MM6);

    EXPECT_EQ(table.specialEnchantments[ITEM_ENCHANTMENT_OF_PROTECTION].itemSuffixOrPrefix, "of Protection");
    EXPECT_EQ(table.specialEnchantments[ITEM_ENCHANTMENT_OF_PROTECTION].description, "Plus 10 to all Resistances.");
    EXPECT_EQ(table.specialEnchantments[ITEM_ENCHANTMENT_OF_PROTECTION].valueAdd, 1000);
    EXPECT_EQ(table.specialEnchantments[ITEM_ENCHANTMENT_OF_PROTECTION].valueMul, 1);
    EXPECT_EQ(table.specialEnchantments[ITEM_ENCHANTMENT_OF_PROTECTION].enchantmentLevel, 1); // "B".

    EXPECT_EQ(table.specialEnchantments[static_cast<ItemEnchantment>(2)].itemSuffixOrPrefix, "of The Gods");
    EXPECT_EQ(table.specialEnchantments[static_cast<ItemEnchantment>(2)].enchantmentLevel, 3); // "D".

    // "X 10" is a value multiplier, not an additive value.
    EXPECT_EQ(table.specialEnchantments[static_cast<ItemEnchantment>(3)].itemSuffixOrPrefix, "Antique");
    EXPECT_EQ(table.specialEnchantments[static_cast<ItemEnchantment>(3)].valueMul, 10);
    EXPECT_EQ(table.specialEnchantments[static_cast<ItemEnchantment>(3)].valueAdd, 0);

    // The shorter MM6 set stops before the trailing section: the 4th slot stays empty, and the sum
    // row's empty "Value" cell is never parsed.
    EXPECT_TRUE(table.specialEnchantments[static_cast<ItemEnchantment>(4)].itemSuffixOrPrefix.empty());
}

// Guards the MM7 parse path through the version-parameter refactor: MM7's data row count matches the
// enum size, so the zip stops at the data end with no section overrun and the same rows parse.
GAME_TEST(SpcItemsMm7, ParsesFullSet) {
    Blob blob = makeTableBlob({
        {"Special Bonuses by Group"},
        {""},
        {"Bonus Stat", "Name Add", "W1", "W2", "Miss", "Arm", "Shld", "Helm", "Belt", "Cape", "Gaunt", "Boot", "Ring", "Amul", "Value", "Lvl", "Description"},
        {""},
        spcRow("Plus 10 to all Resistances.", "of Protection", "1000", "B"),
        spcRow("Plus 10 to all Seven Statistics.", "of The Gods", "3000", "D"),
        spcRow("Increased Value.", "Antique", "X 10", "B"),
    });

    ItemTable table;
    table.LoadSpecialEnchantments(blob, GAME_VERSION_MM7);

    EXPECT_EQ(table.specialEnchantments[ITEM_ENCHANTMENT_OF_PROTECTION].itemSuffixOrPrefix, "of Protection");
    EXPECT_EQ(table.specialEnchantments[ITEM_ENCHANTMENT_OF_PROTECTION].valueAdd, 1000);
    EXPECT_EQ(table.specialEnchantments[static_cast<ItemEnchantment>(2)].itemSuffixOrPrefix, "of The Gods");
    EXPECT_EQ(table.specialEnchantments[static_cast<ItemEnchantment>(2)].enchantmentLevel, 3); // "D".
    EXPECT_EQ(table.specialEnchantments[static_cast<ItemEnchantment>(3)].valueMul, 10);
}

// MM6's items.txt / rnditems.txt are a different item-ID SET from MM7's (581 items, ids 0-580, vs MM7's
// 799 ids 1-799). The engine's ItemId enum is MM7-shaped, so MM6 ids (id 0, and the differing range)
// index outside the `items` array -> "array subscript out of range". Until a version-aware item model
// exists (docs/pending/mm6-item-model.md), the MM6 branch defers: it must not throw and must leave item
// data unpopulated. The id-0 row below would crash if parsed, so reaching the assertions proves the skip.
GAME_TEST(ItemsMm6, LoadDefers) {
    Blob items = makeTableBlob({
        {"header1"},
        {"header2"},
        {"0", "lsword1", "Longsword", "50", "Weapon", "Sword", "3d3", "0", "8", "1", "Longsword", "1", "4", "499", "8", "An MM6 item with out-of-range id."},
    });
    Blob rnditems = makeTableBlob({
        {"header1"}, {"header2"}, {"header3"}, {"header4"},
        {"0", "ring1", "5", "5", "5", "5", "5", "5"},
    });

    ItemTable table;
    table.LoadItems(items, GAME_VERSION_MM6);          // Must not throw despite the out-of-range id.
    table.LoadRandomItems(rnditems, GAME_VERSION_MM6); // Must not throw either.

    // Nothing populated - item data stays at defaults.
    EXPECT_TRUE(table.items[ITEM_CRUDE_LONGSWORD].name.empty());
}

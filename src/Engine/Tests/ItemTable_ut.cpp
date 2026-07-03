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

// MM6's items.txt is the same table as MM7's with four layout differences: a tabs-only ruler line
// precedes the header (three rows before the data instead of MM7's two), blank placeholder rows sit at
// ids 0/299/399, the VarA/VarB enchantment columns are absent (so the paperdoll/description tail is
// shifted left by two), and the id range is 1-580. MM6 ids fit inside the MM7-shaped `items` array and
// are parsed directly into it: in an MM6 session, ItemId(N) denotes MM6's item N (see
// docs/pending/mm6-item-model.md for the id-semantics leftovers this implies).
GAME_TEST(ItemsMm6, ParsesMm6Layout) {
    Blob items = makeTableBlob({
        {"", "", "", "", "", "", "", "", "", "", "", "", "", "", ""}, // Tabs-only ruler line.
        {"Item #", "Pic File", "Name", "Value", "Equip Stat", "Skill Group", "Mod1", "Mod2", "material", "ID/Rep/St", "Not identified name", "Sprite Index", "Shape", "Equip X", "Equip Y", "Notes"},
        {"0", "", "", "", "", "", "", "", "", "", "", "", "", "", ""}, // Blank id-0 placeholder row.
        {"1", "lsword1", "Longsword", "50", "Weapon", "Sword", "3d3", "0", "8", "1", "Longsword", "1", "4", "499", "8", "A longsword."},
        {"2", "", "", "", "", "", "", "", "", "", "", "", "", "", ""}, // Blank separator row (ids 299/399 in the real data).
        {"3", "scroll4", "Fly", "300", "Sscroll", "Misc", "S21", "0", "2", "1", "Spell Scroll", "0", "1", "0", "0", "The Fly spell."},
        {"4", "ldagger3", "Mordred", "20000", "Weapon", "Dagger", "2d3", "8", "Artifact", "15", "Artifact", "6", "0", "10", "20", "An artifact."},
        {"5", "memcryst", "Memory Crystal", "0", "N / A", "Misc", "0", "0", "", "0", "Memory Crystal", "0", "0", "0", "0", "A quest item."},
        {"6", "book5", "Dark Containment", "60000", "Book", "Misc", "S99", "0", "2", "10", "Spell Book", "0", "1", "0", "0", "A spell book."},
        {"7", "bottle22", "Rejuvenation", "1500", "Bottle", "Misc", "P25", "0", "2", "0", "Potion", "0", "1", "0", "0", "A potion."},
        // MM6 wands take their spell from the data column (the original MM6 engine had no wand table).
        // Id 136 deliberately matches an MM7 wand id: the MM7-only legacy wand map must NOT kick in here.
        {"136", "wand1", "Wand of Static", "750", "WeaponW", "Misc", "S13", "15", "2", "5", "Wand", "0", "1", "0", "0", "A wand."},
        {"500", "scroll5", "Name of Message", "10", "Mscroll", "Misc", "M1", "0", "2", "0", "Scroll", "0", "1", "0", "0", "A message scroll."},
    });

    ItemTable table;
    table.LoadItems(items, GAME_VERSION_MM6);

    EXPECT_EQ(table.items[ItemId(1)].name, "Longsword");
    EXPECT_EQ(table.items[ItemId(1)].iconName, "lsword1");
    EXPECT_EQ(table.items[ItemId(1)].baseValue, 50);
    EXPECT_EQ(table.items[ItemId(1)].type, ITEM_TYPE_SINGLE_HANDED);
    EXPECT_EQ(table.items[ItemId(1)].skill, SKILL_SWORD);
    EXPECT_EQ(table.items[ItemId(1)].damageDice, 3);
    EXPECT_EQ(table.items[ItemId(1)].damageRoll, 3);
    EXPECT_EQ(table.items[ItemId(1)].unidentifiedName, "Longsword");
    // The tail follows the MM6 column layout (no VarA/VarB): paperdoll anchor from columns 13-14,
    // description from column 15.
    EXPECT_EQ(table.items[ItemId(1)].paperdollAnchorOffset.x, 499);
    EXPECT_EQ(table.items[ItemId(1)].paperdollAnchorOffset.y, 8);
    EXPECT_EQ(table.items[ItemId(1)].description, "A longsword.");

    EXPECT_TRUE(table.items[ItemId(2)].name.empty()); // Blank separator rows are skipped.
    EXPECT_EQ(table.items[ItemId(3)].type, ITEM_TYPE_SPELL_SCROLL);
    EXPECT_EQ(table.items[ItemId(4)].rarity, RARITY_ARTIFACT);
    EXPECT_EQ(table.items[ItemId(5)].type, ITEM_TYPE_NONE); // "N / A" - quest items and other non-equipment.

    // Mod1 "S<n>" payloads carry the bound spell id (the standard 9-schools-by-11 spell numbering that
    // the SpellId enum follows) for scrolls, books and - in MM6 - wands; "P<n>" carries the MM6 potion
    // content id. Non-payload items get no bindings.
    EXPECT_EQ(table.items[ItemId(3)].spellId, SPELL_AIR_FLY); // S21.
    EXPECT_EQ(table.items[ItemId(6)].spellId, SPELL_DARK_SOULDRINKER); // S99 - MM6's Dark Containment slot.
    EXPECT_EQ(table.items[ItemId(7)].potionId, 25); // P25.
    EXPECT_EQ(table.items[ItemId(7)].spellId, SPELL_NONE);
    EXPECT_EQ(table.items[ItemId(136)].spellId, SpellId(13)); // S13 from data - MM6's Static Charge slot; no MM7 wand-map override.
    EXPECT_EQ(table.items[ItemId(1)].spellId, SPELL_NONE); // Dice Mod1.
    EXPECT_EQ(table.items[ItemId(1)].potionId, 0);
    EXPECT_EQ(table.items[ItemId(5)].spellId, SPELL_NONE); // "0" Mod1.
    EXPECT_EQ(table.items[ItemId(500)].spellId, SPELL_NONE); // "M<n>" is a message-scroll text ordinal, not a spell.
}

// Both games' items.txt binds spells to items through Mod1 "S<n>" payloads. For scrolls and spellbooks
// MM7's column agrees with the engine's legacy hardcoded maps on all 99+99 entries (verified against the
// original data), so the parse is authoritative. MM7's WAND rows however carry stale MM6 leftovers in
// that column (16 of 25 disagree with what MM7 wands actually cast), so for MM7 wands the parser applies
// the legacy wand map instead of the data.
GAME_TEST(ItemsMm7, ParsesSpellBindings) {
    Blob items = makeTableBlob({
        {"Item #", "Pic File", "Name", "Value", "Equip Stat", "Skill Group", "Mod1", "Mod2", "material", "ID/Rep/St", "Not identified name", "Sprite Index", "VarA", "VarB", "Equip X", "Equip Y", "Notes"},
        {"", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", ""},
        {"1", "lsword1", "Longsword", "50", "Weapon", "Sword", "3d3", "0", "0", "1", "Longsword", "1", "0", "0", "499", "8", "A longsword."},
        {"300", "item184", "Torch Light", "50", "Sscroll", "Misc", "S1", "0", "0", "1", "Scroll", "0", "0", "0", "0", "0", "A scroll."},
        {"498", "item239", "Souldrinker", "60000", "Book", "Misc", "S99", "0", "0", "10", "Book", "0", "0", "0", "0", "0", "A book."},
        // Wand of Sparks: the data column says S13 (a stale MM6 value), but MM7's wand really casts
        // Sparks (spell 15) - the legacy wand map must override the data for MM7 wands.
        {"136", "item135", "Wand of Sparks", "750", "WeaponW", "Misc", "S13", "15", "0", "5", "Wand", "0", "0", "0", "0", "0", "A wand."},
    });

    ItemTable table;
    table.LoadItems(items, GAME_VERSION_MM7);

    EXPECT_EQ(table.items[ItemId(300)].spellId, SPELL_FIRE_TORCH_LIGHT); // S1 from data.
    EXPECT_EQ(table.items[ItemId(498)].spellId, SPELL_DARK_SOULDRINKER); // S99 from data.
    EXPECT_EQ(table.items[ITEM_WAND_OF_SPARKS].spellId, SPELL_AIR_SPARKS); // Legacy map, NOT the stale S13.
    EXPECT_EQ(table.items[ItemId(1)].spellId, SPELL_NONE);
    EXPECT_EQ(table.items[ItemId(1)].potionId, 0);
}

// MM6's rnditems.txt lists per-item chances for ids 1-400 and, unlike MM7's fixed 618-row section, ends
// the per-item section with a sum row before the bonus-chance section, so the MM6 parse is data-driven:
// the per-item section ends at the first row without a numeric id, and the three bonus-chance rows are
// recognized by their labels.
GAME_TEST(RndItemsMm6, ParsesMm6Layout) {
    Blob rnditems = makeTableBlob({
        {"Random Item Generation By Treasure Level 1 - 6"},
        {"", "", "Chance By Level"},
        {"Item #", "Pic File", "1", "2", "3", "4", "5", "6"},
        {"0", "", "", "", "", "", "", ""}, // Blank id-0 placeholder row.
        {"1", "Longsword1", "5", "5", "2", "0", "0", "0"},
        {"2", "Longsword2", "0", "10", "5", "2", "0", "0"},
        {"", "", "5", "15", "7", "2", "0", "0"}, // Sum row - ends the per-item section (MM7 has no such row).
        {"Bonus chance by level %", "", "1", "2", "3", "4", "5", "6"},
        {"", "Standard", "0", "40", "40", "40", "40", "75"},
        {"", "Special", "0", "0", "10", "15", "20", "25"},
        {"Weapons", "Special %", "0", "0", "10", "20", "30", "50"},
        {"", "(note weapons have no chance for standard just chance for special)"},
    });

    ItemTable table;
    table.LoadRandomItems(rnditems, GAME_VERSION_MM6);

    EXPECT_EQ(table.items[ItemId(1)].uChanceByTreasureLvl[ITEM_TREASURE_LEVEL_1], 5);
    EXPECT_EQ(table.items[ItemId(2)].uChanceByTreasureLvl[ITEM_TREASURE_LEVEL_2], 10);
    EXPECT_EQ(table.itemChanceSumByTreasureLevel[ITEM_TREASURE_LEVEL_2], 15);
    EXPECT_EQ(table.standardEnchantmentChanceForEquipment[ITEM_TREASURE_LEVEL_6], 75);
    EXPECT_EQ(table.specialEnchantmentChanceForEquipment[ITEM_TREASURE_LEVEL_3], 10);
    EXPECT_EQ(table.specialEnchantmentChanceForWeapons[ITEM_TREASURE_LEVEL_6], 50);
}

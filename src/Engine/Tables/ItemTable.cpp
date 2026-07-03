#include "ItemTable.h"

#include <array>
#include <cctype>
#include <cstdlib>
#include <map>
#include <vector>
#include <string>
#include <string_view>
#include <utility>

#include "Library/Serialization/Serialization.h"

#include "Engine/Random/Random.h"
#include "Engine/Spells/Spells.h"
#include "Engine/Objects/CharacterEnumFunctions.h"
#include "Engine/Engine.h"
#include "Engine/Resources/EngineFileSystem.h"
#include "Engine/Party.h"
#include "Engine/Resources/ResourceManager.h"

#include "GUI/UI/UIHouses.h"

#include "Library/LodFormats/LodFormats.h"
#include "Library/Logger/Logger.h"

#include "Utility/String/Ascii.h"
#include "Utility/MapAccess.h"
#include "Utility/Memory/Blob.h"
#include "Utility/String/Transformations.h"
#include "Utility/String/Split.h"

void ItemTable::LoadStandardEnchantments(const Blob &stditems, GameVersion version) {
    // stditems.txt has two sections.
    std::vector<std::string_view> lines = split(stditems.str()).by("\r\n").drop(4).skip("");

    // MM6 defines only 14 standard bonuses (Might..Poison Resistance) where MM7 has 24 (which also add
    // Mind/Body resistances and the 8 skill bonuses). MM6's three extra elemental resistances map onto
    // the MM7 Attribute enum exactly the way the monster parser remaps them - Elec->Air, Cold->Water,
    // Poison->Earth (see Monsters.cpp) - so MM6's 14 bonuses are a clean prefix of the enchantable range:
    // [ATTRIBUTE_MIGHT, ATTRIBUTE_RESIST_EARTH]. The remaining attributes are left at their (zero-chance)
    // defaults, so they're never rolled during item generation. The section layout is otherwise identical
    // (sum row + 2 sub-headers between the two sections), so the "+3" offset below holds for both games.
    Segment<Attribute> enchantableAttributes = version == GAME_VERSION_MM6
        ? Segment(ATTRIBUTE_MIGHT, ATTRIBUTE_RESIST_EARTH)
        : allEnchantableAttributes();

    // #1 Standard Bonuses by Group: attribute name (localized) | suffix (localized) | chance by item type....
    standardEnchantmentChanceSumByItemType.fill(0);
    for (auto [line, i] : view(lines).zip(enchantableAttributes)) {
        std::array<std::string_view, 11> tokens = split(line).by('\t');
        standardEnchantments[i].attributeName = removeQuotes(tokens[0]);
        standardEnchantments[i].itemSuffix = removeQuotes(tokens[1]);

        int k = 2;
        for (ItemType equipType : standardEnchantments[i].chanceByItemType.indices()) {
            standardEnchantments[i].chanceByItemType[equipType] = fromString<int>(tokens[k++]);
            standardEnchantmentChanceSumByItemType[equipType] += standardEnchantments[i].chanceByItemType[equipType];
        }
    }

    // #2 Bonus range for Standard by Level: (empty) | treasure level | min | max.
    for (auto [line, i] : view(lines).drop(enchantableAttributes.size() + 3).zip(standardEnchantmentRangeByTreasureLevel.indices())) {
        std::array<std::string_view, 4> rangeTokens = split(line).by('\t');
        standardEnchantmentRangeByTreasureLevel[i] = Segment(fromString<int>(rangeTokens[2]), fromString<int>(rangeTokens[3]));
    }
}

void ItemTable::LoadSpecialEnchantments(const Blob &spcitems, GameVersion version) {
    // spcitems.txt table structure: description (localized) | suffix/prefix (localized) | chance by item type... | gold value | enchantment level.
    for (auto [line, i] : split(spcitems.str()).by("\r\n").drop(4).skip("").zip(specialEnchantments.indices())) {
        std::array<std::string_view, 17> tokens = split(line).by('\t');

        // MM6 lists fewer special enchantments than MM7 (59 vs 72), and MM6's set is a positional prefix
        // of MM7's, so the ids line up. But because MM6's list is shorter than the MM7-shaped enum, the
        // zip would otherwise overrun the data into the trailing sum/legend section and parse its empty
        // "Value" cell as a number ('' is not a number). Every real enchantment has a non-empty "Name Add"
        // (suffix) column, while the first section row leaves it empty, so it marks the end of MM6's data.
        if (version == GAME_VERSION_MM6 && tokens[1].empty())
            break;

        specialEnchantments[i].description = removeQuotes(tokens[0]);
        specialEnchantments[i].itemSuffixOrPrefix = removeQuotes(tokens[1]);

        int k = 2;
        for (ItemType j : specialEnchantments[i].chanceByItemType.indices())
            specialEnchantments[i].chanceByItemType[j] = fromString<int>(tokens[k++]);

        std::string_view token14 = tokens[14];
        int res;
        bool isMul = !token14.empty() && (token14[0] == 'x' || token14[0] == 'X');
        if (isMul) {
            // "X 2" or "x2" — strip prefix, parse number.
            token14.remove_prefix(1);
            while (!token14.empty() && token14[0] == ' ')
                token14.remove_prefix(1);
            res = fromString<int>(token14);
        } else {
            res = fromString<int>(token14);
        }
        if (isMul)
            specialEnchantments[i].valueMul = res;
        else
            specialEnchantments[i].valueAdd = res;
        specialEnchantments[i].enchantmentLevel = !tokens[15].empty() ? (tolower(tokens[15][0]) - 'a') : 0;
    }
}

void ItemTable::LoadItems(const Blob &itemsBlob, GameVersion version) {
    // items.txt table structure: index | icon | name (localized) | value | type | skill | damage | mod | material | ...
    //
    // MM6's items.txt is the same table with a different item SET (581 rows, ids 1-580 - MM6 id N does
    // not denote MM7's item N!) and four layout differences: a tabs-only ruler line precedes the header
    // row, blank placeholder rows sit at ids 0/299/399, and the VarA/VarB enchantment columns are absent,
    // shifting the paperdoll/description tail left by two. MM6's ids fit inside the MM7-shaped `items`
    // array, so they are parsed directly into it: in an MM6 session ItemId(N) means MM6's item N, and
    // engine code built around MM7's named ItemId constants (potion ranges, artifact ranges, quest item
    // patches) does not apply - see docs/pending/mm6-item-model.md for these id-semantics leftovers.

    static const std::map<std::string, ItemType, ascii::NoCaseLess> equipStatMap = { // TODO(captainurist): #enum use enum serialization
        {"weapon", ITEM_TYPE_SINGLE_HANDED},
        {"weapon2", ITEM_TYPE_TWO_HANDED},
        {"weapon1or2", ITEM_TYPE_SINGLE_HANDED},
        {"missile", ITEM_TYPE_BOW},
        {"bow", ITEM_TYPE_BOW},
        {"armor", ITEM_TYPE_ARMOUR},
        {"shield", ITEM_TYPE_SHIELD},
        {"helm", ITEM_TYPE_HELMET},
        {"belt", ITEM_TYPE_BELT},
        {"cloak", ITEM_TYPE_CLOAK},
        {"gauntlets", ITEM_TYPE_GAUNTLETS},
        {"boots", ITEM_TYPE_BOOTS},
        {"ring", ITEM_TYPE_RING},
        {"amulet", ITEM_TYPE_AMULET},
        {"weaponw", ITEM_TYPE_WAND},
        {"herb", ITEM_TYPE_REAGENT},
        {"reagent", ITEM_TYPE_REAGENT},
        {"bottle", ITEM_TYPE_POTION},
        {"sscroll", ITEM_TYPE_SPELL_SCROLL},
        {"book", ITEM_TYPE_BOOK},
        {"mscroll", ITEM_TYPE_MESSAGE_SCROLL},
        {"gold", ITEM_TYPE_GOLD},
        {"gem", ITEM_TYPE_GEM},
    };

    static const std::map<std::string, Skill, ascii::NoCaseLess> equipSkillMap = {
        {"staff", SKILL_STAFF},
        {"sword", SKILL_SWORD},
        {"dagger", SKILL_DAGGER},
        {"axe", SKILL_AXE},
        {"spear", SKILL_SPEAR},
        {"bow", SKILL_BOW},
        {"mace", SKILL_MACE},
        {"blaster", SKILL_BLASTER},
        {"shield", SKILL_SHIELD},
        {"leather", SKILL_LEATHER},
        {"chain", SKILL_CHAIN},
        {"plate", SKILL_PLATE},
        {"club", SKILL_CLUB},
    };

    static const std::map<std::string, ItemRarity, ascii::NoCaseLess> materialMap = {
        {"artifact", RARITY_ARTIFACT},
        {"relic", RARITY_RELIC},
        {"special", RARITY_SPECIAL},
    };

    // MM6's leading ruler line means three rows precede the data instead of MM7's two.
    for (std::string_view line : split(itemsBlob.str()).by("\r\n").drop(version == GAME_VERSION_MM6 ? 3 : 2).skip("")) {
        std::array<std::string_view, 17> tokens = split(line).by('\t');
        if (version == GAME_VERSION_MM6 && tokens[1].empty())
            continue; // Blank placeholder row (ids 0/299/399) - no data to parse.
        ItemId item_counter = ItemId(fromString<int>(tokens[0]));
        items[item_counter].iconName = removeQuotes(tokens[1]);
        items[item_counter].name = removeQuotes(tokens[2]);
        items[item_counter].baseValue = fromString<int>(tokens[3]);
        items[item_counter].type = valueOr(equipStatMap, tokens[4], ITEM_TYPE_NONE);
        items[item_counter].skill = valueOr(equipSkillMap, tokens[5], SKILL_MISC);
        // Non-dice Mod1 payloads carry a content id, not damage: "S<n>" binds spell n (the standard
        // 9-schools-by-11 numbering the SpellId enum follows) to scrolls, books and wands, "P<n>"
        // (MM6 only) is the potion content id, and "M<n>" is the message-scroll text ordinal (unused
        // here - scroll.txt is keyed by item id directly).
        std::array<std::string_view, 2> diceRollTokens = split(tokens[6]).by('d');
        char damagePrefix = tolower(diceRollTokens[0][0]);
        if (!diceRollTokens[1].empty()) {
            items[item_counter].damageDice = fromString<int>(diceRollTokens[0]);
            items[item_counter].damageRoll = fromString<int>(diceRollTokens[1]);
        } else if (damagePrefix != 's' && damagePrefix != 'm' && damagePrefix != 'p') {
            items[item_counter].damageDice = fromString<int>(diceRollTokens[0]);
            items[item_counter].damageRoll = 1;
        } else {
            items[item_counter].damageDice = 0;
            items[item_counter].damageRoll = 0;

            std::string_view payloadToken = trim(diceRollTokens[0].substr(1)); // Also handles "S 09"-style cells.
            ItemType itemType = items[item_counter].type;
            bool bindsSpell = itemType == ITEM_TYPE_SPELL_SCROLL || itemType == ITEM_TYPE_BOOK || itemType == ITEM_TYPE_WAND;
            if (damagePrefix == 's' && bindsSpell) {
                int spell = fromString<int>(payloadToken);
                if (spell >= std::to_underlying(SPELL_FIRST_REGULAR) && spell <= std::to_underlying(SPELL_LAST_REGULAR)) {
                    items[item_counter].spellId = static_cast<SpellId>(spell);
                } else {
                    logger->warning("items.txt: item {} has an out-of-range spell binding '{}'", std::to_underlying(item_counter), tokens[6]);
                }
            } else if (damagePrefix == 'p' && itemType == ITEM_TYPE_POTION) {
                items[item_counter].potionId = fromString<int>(payloadToken);
            }
        }

        // MM7's wand rows carry stale MM6 values in the "S<n>" column (16 of 25 disagree with what
        // MM7 wands actually cast), so for MM7 the hardcoded wand map below is authoritative. MM6 has
        // no wand map - its engine reads the spell from the data, so the parse above stands.
        static constexpr IndexedArray<SpellId, ITEM_FIRST_WAND, ITEM_LAST_WAND> mm7SpellByWand = {
            {ITEM_WAND_OF_FIRE,                SPELL_FIRE_FIRE_BOLT},
            {ITEM_WAND_OF_SPARKS,              SPELL_AIR_SPARKS},
            {ITEM_WAND_OF_POISON,              SPELL_WATER_POISON_SPRAY},
            {ITEM_WAND_OF_STUNNING,            SPELL_EARTH_STUN},
            {ITEM_WAND_OF_HARM,                SPELL_BODY_HARM},

            {ITEM_FAIRY_WAND_OF_LIGHT,         SPELL_LIGHT_LIGHT_BOLT},
            {ITEM_FAIRY_WAND_OF_ICE,           SPELL_WATER_ICE_BOLT},
            {ITEM_FAIRY_WAND_OF_LASHING,       SPELL_SPIRIT_SPIRIT_LASH},
            {ITEM_FAIRY_WAND_OF_MIND,          SPELL_MIND_MIND_BLAST},
            {ITEM_FAIRY_WAND_OF_SWARMS,        SPELL_EARTH_DEADLY_SWARM},

            {ITEM_ALACORN_WAND_OF_FIREBALLS,   SPELL_FIRE_FIREBALL},
            {ITEM_ALACORN_WAND_OF_ACID,        SPELL_WATER_ACID_BURST},
            {ITEM_ALACORN_WAND_OF_LIGHTNING,   SPELL_AIR_LIGHTNING_BOLT},
            {ITEM_ALACORN_WAND_OF_BLADES,      SPELL_EARTH_BLADES},
            {ITEM_ALACORN_WAND_OF_CHARMS,      SPELL_MIND_CHARM},

            {ITEM_ARCANE_WAND_OF_BLASTING,     SPELL_WATER_ICE_BLAST},
            {ITEM_ARCANE_WAND_OF_THE_FIST,     SPELL_BODY_FLYING_FIST},
            {ITEM_ARCANE_WAND_OF_ROCKS,        SPELL_EARTH_ROCK_BLAST},
            {ITEM_ARCANE_WAND_OF_PARALYZING,   SPELL_LIGHT_PARALYZE},
            {ITEM_ARCANE_WAND_OF_CLOUDS,       SPELL_DARK_TOXIC_CLOUD},

            {ITEM_MYSTIC_WAND_OF_IMPLOSION,    SPELL_AIR_IMPLOSION},
            {ITEM_MYSTIC_WAND_OF_DISTORTION,   SPELL_EARTH_MASS_DISTORTION},
            {ITEM_MYSTIC_WAND_OF_SHRAPMETAL,   SPELL_DARK_SHARPMETAL},
            {ITEM_MYSTIC_WAND_OF_SHRINKING,    SPELL_DARK_SHRINKING_RAY},
            {ITEM_MYSTIC_WAND_OF_INCINERATION, SPELL_FIRE_INCINERATE}
        };
        if (version != GAME_VERSION_MM6 && items[item_counter].type == ITEM_TYPE_WAND && isWand(item_counter))
            items[item_counter].spellId = mm7SpellByWand[item_counter];
        items[item_counter].damageMod = fromString<int>(tokens[7]);
        items[item_counter].rarity = valueOr(materialMap, tokens[8], RARITY_COMMON);
        items[item_counter].identifyAndRepairDifficulty = fromString<int>(tokens[9]);
        items[item_counter].unidentifiedName = removeQuotes(tokens[10]);
        items[item_counter].spriteId = static_cast<SpriteId>(fromString<int>(tokens[11]));

        if (items[item_counter].type == ITEM_TYPE_REAGENT) {
            items[item_counter].reagentPower = items[item_counter].damageDice;
            items[item_counter].damageDice = items[item_counter].damageRoll = items[item_counter].damageMod = 0;
        }

        items[item_counter].specialEnchantment = ITEM_ENCHANTMENT_NULL;
        items[item_counter].standardEnchantment = {};

        if (version == GAME_VERSION_MM6) {
            // MM6 has no VarA/VarB enchantment columns (and no "special"-material items either, only
            // artifacts and relics), so the paperdoll/description tail sits two columns to the left.
            // Column 12 is the inventory "Shape" id, unused - sizes come from icons in LoadItemSizes.
            items[item_counter].standardEnchantmentStrength = 0;
            items[item_counter].paperdollAnchorOffset.x = fromString<int>(tokens[13]);
            items[item_counter].paperdollAnchorOffset.y = fromString<int>(tokens[14]);
            items[item_counter].description = removeQuotes(tokens[15]);
            continue;
        }

        if (items[item_counter].rarity == RARITY_SPECIAL) {
            for (Attribute ii : allEnchantableAttributes()) {
                if (ascii::noCaseEquals(tokens[12], standardEnchantments[ii].itemSuffix)) { // TODO(captainurist): #unicode this is not ascii
                    items[item_counter].standardEnchantment = ii;
                    break;
                }
            }
            if (!items[item_counter].standardEnchantment) {
                for (ItemEnchantment ii : specialEnchantments.indices()) {
                    if (ascii::noCaseEquals(tokens[12], specialEnchantments[ii].itemSuffixOrPrefix)) { // TODO(captainurist): #unicode this is not ascii
                        items[item_counter].specialEnchantment = ii;
                    }
                }
            }
        }

        if ((items[item_counter].rarity == RARITY_SPECIAL) &&
            (items[item_counter].standardEnchantment)) {
            char b_s = fromString<int>(tokens[13]);
            if (b_s)
                items[item_counter].standardEnchantmentStrength = b_s;
            else
                items[item_counter].standardEnchantmentStrength = 1;
        } else {
            items[item_counter].standardEnchantmentStrength = 0;
        }
        items[item_counter].paperdollAnchorOffset.x = fromString<int>(tokens[14]);
        items[item_counter].paperdollAnchorOffset.y = fromString<int>(tokens[15]);
        items[item_counter].description = removeQuotes(tokens[16]);
    }
}

void ItemTable::LoadRandomItems(const Blob &rnditems, GameVersion version) {
    if (version == GAME_VERSION_MM6) {
        // MM6's rnditems.txt indexes the same MM6 item-id space as its items.txt (per-item chances for
        // ids 1-400) and, unlike MM7's fixed 618-row section, ends the per-item section with a sum row
        // before the bonus-chance section. So the MM6 parse is data-driven: per-item rows are the ones
        // with a numeric id, and the three bonus-chance rows are recognized by their labels.
        const auto parseChanceCells = [](const std::array<std::string_view, 8> &tokens, auto &chances) {
            for (ItemTreasureLevel level : chances.indices())
                chances[level] = fromString<int>(tokens[2 + std::to_underlying(level) - std::to_underlying(ITEM_TREASURE_LEVEL_FIRST_RANDOM)]);
        };

        for (std::string_view line : split(rnditems.str()).by("\r\n").drop(4).skip("")) {
            std::array<std::string_view, 8> tokens = split(line).by('\t');
            if (!tokens[0].empty() && isdigit(static_cast<unsigned char>(tokens[0][0]))) {
                parseChanceCells(tokens, items[ItemId(fromString<int>(tokens[0]))].uChanceByTreasureLvl);
            } else if (ascii::noCaseEquals(tokens[1], "Standard")) {
                parseChanceCells(tokens, standardEnchantmentChanceForEquipment);
            } else if (ascii::noCaseEquals(tokens[1], "Special")) {
                parseChanceCells(tokens, specialEnchantmentChanceForEquipment);
            } else if (ascii::noCaseEquals(tokens[1], "Special %")) {
                parseChanceCells(tokens, specialEnchantmentChanceForWeapons);
            } // Anything else is the sum row, a section header or a legend note - not data.
        }

        itemChanceSumByTreasureLevel.fill(0);
        for (ItemTreasureLevel i : itemChanceSumByTreasureLevel.indices())
            for (ItemId j : items.indices())
                itemChanceSumByTreasureLevel[i] += items[j].uChanceByTreasureLvl[i];
        return;
    }

    // rnditems.txt has two sections.
    std::vector<std::string_view> lines = split(rnditems.str()).by("\r\n").drop(4).skip("");
    constexpr size_t section1Size = 618;

    // #1 Per-item chances: item index | id (e.g. "ring1", not used) | chance by treasure level 1-6.
    for (std::string_view line : view(lines).drop(0).resize(section1Size, "")) {
        std::array<std::string_view, 8> tokens = split(line).by('\t');
        ItemId item_counter = ItemId(fromString<int>(tokens[0]));
        items[item_counter].uChanceByTreasureLvl[ITEM_TREASURE_LEVEL_1] = fromString<int>(tokens[2]);
        items[item_counter].uChanceByTreasureLvl[ITEM_TREASURE_LEVEL_2] = fromString<int>(tokens[3]);
        items[item_counter].uChanceByTreasureLvl[ITEM_TREASURE_LEVEL_3] = fromString<int>(tokens[4]);
        items[item_counter].uChanceByTreasureLvl[ITEM_TREASURE_LEVEL_4] = fromString<int>(tokens[5]);
        items[item_counter].uChanceByTreasureLvl[ITEM_TREASURE_LEVEL_5] = fromString<int>(tokens[6]);
        items[item_counter].uChanceByTreasureLvl[ITEM_TREASURE_LEVEL_6] = fromString<int>(tokens[7]);
    }
    itemChanceSumByTreasureLevel.fill(0);
    for (ItemTreasureLevel i : itemChanceSumByTreasureLevel.indices())
        for (ItemId j : items.indices())
            itemChanceSumByTreasureLevel[i] += items[j].uChanceByTreasureLvl[i];

    // #2: Enchantment chances by level: line type (not localized, not used) | enchantment type (not localized, not used) | chance by treasure level 1-6.
    auto chancesArray = std::array{&standardEnchantmentChanceForEquipment, &specialEnchantmentChanceForEquipment, &specialEnchantmentChanceForWeapons};
    for (auto [line, chances] : view(lines).drop(section1Size + 1).zip(chancesArray)) {
        std::array<std::string_view, 8> tokens = split(line).by('\t');
        for (ItemTreasureLevel i : chances->indices())
            (*chances)[i] = fromString<int>(tokens[2 + std::to_underlying(i) - std::to_underlying(ITEM_TREASURE_LEVEL_FIRST_RANDOM)]);
    }
}

//----- (00456D84) --------------------------------------------------------
void ItemTable::Initialize(ResourceManager *resourceManager, GameVersion version) {
    // potion.txt / potnotes.txt (potion-mixing matrices) are absent from MM6's icons.lod.
    // eventsDataIfPresent yields an empty blob when missing; LoadPotions/LoadPotionNotes no-op on it
    // (empty input splits to zero rows), leaving the matrices at their defaults.
    LoadPotions(resourceManager->eventsDataIfPresent("potion.txt"));
    LoadPotionNotes(resourceManager->eventsDataIfPresent("potnotes.txt"));
    LoadStandardEnchantments(resourceManager->eventsData("stditems.txt"), version);
    LoadSpecialEnchantments(resourceManager->eventsData("spcitems.txt"), version);
    LoadItems(resourceManager->eventsData("items.txt"), version);
    LoadRandomItems(resourceManager->eventsData("rnditems.txt"), version);

    Item::PopulateSpecialBonusMap();
    Item::PopulateArtifactBonusMap();
    LoadItemSizes();

    // Patch up the data - we want wetsuits to be armor.
    items[ITEM_QUEST_WETSUIT].type = ITEM_TYPE_ARMOUR;
}

//----- (00453B3C) --------------------------------------------------------
void ItemTable::LoadPotions(const Blob &potions) {
    // potion.txt table structure: item index | name (localized) | unidentified name (localized) | effect (not localized) | mixing matrix...
    // First rows are reagents, then real potions follow. Reagents don't have the mixing matrix values.
    // Matrix values: item ID of result, "no" for self-mixing, "E{n}" for damage level n on invalid mix.
    for (std::string_view line : split(potions.str()).by("\r\n").skip("").drop(1)) {
        std::array<std::string_view, 57> tokens = split(line).by('\t'); // 7 header cells + 50 mixing-matrix cells.
        if (tokens[0].empty())
            continue; // Skip tab-only lines.

        ItemId row = static_cast<ItemId>(fromString<int>(tokens[0]));
        if (row < ITEM_FIRST_REAL_POTION || row > ITEM_LAST_REAL_POTION)
            continue; // Skip reagents, catalyst, etc.

        for (ItemId column : Segment(ITEM_FIRST_REAL_POTION, ITEM_LAST_REAL_POTION)) {
            int flatPotionId = std::to_underlying(column) - std::to_underlying(ITEM_FIRST_REAL_POTION);
            std::string_view cell = tokens[flatPotionId + 7];
            if (cell == "no")
                potionCombination[row][column] = ITEM_NULL;
            else if (cell[0] == 'E')
                potionCombination[row][column] = static_cast<ItemId>(fromString<int>(cell.substr(1))); // Damage level.
            else
                potionCombination[row][column] = static_cast<ItemId>(fromString<int>(cell));
        }
    }
}

//----- (00453CE5) --------------------------------------------------------
void ItemTable::LoadPotionNotes(const Blob &notes) {
    // potnotes.txt has the same layout as potion.txt, but the mixing matrix contains autonote bit indices
    // (for recipe discovery) instead of resulting item IDs.
    for (std::string_view line : split(notes.str()).by("\r\n").skip("").drop(1)) {
        std::array<std::string_view, 57> tokens = split(line).by('\t'); // 7 header cells + 50 mixing-matrix cells.
        if (tokens[0].empty())
            continue; // Skip tab-only lines.

        ItemId row = static_cast<ItemId>(fromString<int>(tokens[0]));
        if (row < ITEM_FIRST_REAL_POTION || row > ITEM_LAST_REAL_POTION)
            continue; // Skip reagents, catalyst, etc.

        for (ItemId column : Segment(ITEM_FIRST_REAL_POTION, ITEM_LAST_REAL_POTION)) {
            int flatPotionId = std::to_underlying(column) - std::to_underlying(ITEM_FIRST_REAL_POTION);
            std::string_view cell = tokens[flatPotionId + 7];
            potionNotes[row][column] = cell == "no" ? 0 : fromString<int>(cell);
        }
    }
}

void ItemTable::LoadItemSizes() {
    // Item sizes are loaded at startup directly from LOD image headers. This would have been an overkill back in 1999
    // (think about all these random reads from your HDD) but is totally fine today. Another option would've been to
    // precalculate these and place in a json file, but why precalculate what's cheap to recalculate?
    LodReader reader(dfs->read("data/icons.lod"));

    for (ItemId itemId : items.indices()) {
        std::string iconName = items[itemId].iconName;

        Sizei iconSize(1, 1); // Actual icon name that will be used in this case is "pending", see LodTextureCache.
        if (reader.exists(iconName))
            iconSize = lod::decodeImageSize(reader.read(iconName));

        itemSizes[itemId] = Sizei(GetSizeInInventorySlots(iconSize.w), GetSizeInInventorySlots(iconSize.h));
    }
}

void ItemTable::generateItem(ItemTreasureLevel treasureLevel, RandomItemType uTreasureType, Item *outItem) {
    assert(isRandomTreasureLevel(treasureLevel));

    std::vector<ItemId> possibleItems;
    std::vector<ItemEnchantment> possibleEnchantments;
    std::vector<int> cumulativeWeights;
    int weightSum = 0;

    assert(outItem != NULL);
    *outItem = Item();

    if (uTreasureType != RANDOM_ITEM_ANY) {  // generate known treasure type
        auto [requestedType, requestedSkill] = itemTypeOrSkillForRandomItemType(uTreasureType);
        for (ItemId itemId : allSpawnableItems()) {
            if ((requestedType == ITEM_TYPE_INVALID || items[itemId].type == requestedType) && (requestedSkill == SKILL_INVALID || items[itemId].skill == requestedSkill)) {
                if (items[itemId].uChanceByTreasureLvl[treasureLevel]) {
                    weightSum += items[itemId].uChanceByTreasureLvl[treasureLevel];
                    possibleItems.push_back(itemId);
                    cumulativeWeights.push_back(weightSum);
                }
            }
        }

        if (weightSum) {
            int pickedWeight = grng->random(weightSum) + 1;
            auto foundWeight = std::lower_bound(cumulativeWeights.begin(), cumulativeWeights.end(), pickedWeight);

            assert(foundWeight != cumulativeWeights.end());

            outItem->itemId = possibleItems[std::distance(cumulativeWeights.begin(), foundWeight)];
        } else {
            outItem->itemId = ITEM_CRUDE_LONGSWORD;
        }
    } else {
        // Try to generate an artifact.
        if (treasureLevel == ITEM_TREASURE_LEVEL_6) {
            int artifactsFound = 0;
            ItemId artifactRandomId = grng->randomSample(allSpawnableArtifacts());
            for (ItemId i : allSpawnableArtifacts())
                artifactsFound += pParty->pIsArtifactFound[i];
            bool artifactLimitReached = (engine->config->gameplay.ArtifactLimit.value() != 0 && artifactsFound >= engine->config->gameplay.ArtifactLimit.value());
            if ((grng->random(100) < 5) && !pParty->pIsArtifactFound[artifactRandomId] && !artifactLimitReached) {
                pParty->pIsArtifactFound[artifactRandomId] = true;
                outItem->flags = 0;
                outItem->itemId = artifactRandomId;
                return;
            }
        }

        // Otherwise try to spawn any random item.
        if (itemChanceSumByTreasureLevel[treasureLevel] == 0) {
            // No spawn chance data at all for this treasure level - rnditems.txt didn't load or lists
            // no items at this level. Leave the item empty.
            logger->warning("Item data is not loaded - cannot generate a random treasure level {} item.",
                            std::to_underlying(treasureLevel));
            return;
        }

        int randomWeight = grng->random(this->itemChanceSumByTreasureLevel[treasureLevel]) + 1;
        for (ItemId itemId : allSpawnableItems()) {
            weightSum += items[itemId].uChanceByTreasureLvl[treasureLevel];
            if (weightSum >= randomWeight) {
                outItem->itemId = itemId;
                break;
            }
        }
    }
    if (outItem->isPotion() && outItem->itemId != ITEM_POTION_BOTTLE) {  // if it potion set potion spec
        outItem->potionPower = grng->randomDice(2, 4) * std::to_underlying(treasureLevel);
    }

    if (outItem->itemId == ITEM_SPELLBOOK_DIVINE_INTERVENTION && !pParty->_questBits[QBIT_DIVINE_INTERVENTION_RETRIEVED])
        outItem->itemId = ITEM_SPELLBOOK_SUNRAY;
    if (pItemTable->items[outItem->itemId].identifyAndRepairDifficulty)
        outItem->flags = 0;
    else
        outItem->flags = ITEM_IDENTIFIED;

    if (!outItem->isPotion()) {
        outItem->specialEnchantment = ITEM_ENCHANTMENT_NULL;
        outItem->standardEnchantment = {};
    }
    // try get special enchantment
    switch (outItem->type()) {
        case ITEM_TYPE_SINGLE_HANDED:
        case ITEM_TYPE_TWO_HANDED:
        case ITEM_TYPE_BOW:
            if (!specialEnchantmentChanceForWeapons[treasureLevel] || grng->random(100) >= specialEnchantmentChanceForWeapons[treasureLevel])
                return;
            break;
        case ITEM_TYPE_ARMOUR:
        case ITEM_TYPE_SHIELD:
        case ITEM_TYPE_HELMET:
        case ITEM_TYPE_BELT:
        case ITEM_TYPE_CLOAK:
        case ITEM_TYPE_GAUNTLETS:
        case ITEM_TYPE_BOOTS:
        case ITEM_TYPE_RING:
        case ITEM_TYPE_AMULET: {
            if (!standardEnchantmentChanceForEquipment[treasureLevel])
                return;
            int bonusChanceRoll = grng->random(100);
            if (bonusChanceRoll < standardEnchantmentChanceForEquipment[treasureLevel]) {
                int enchantmentChanceSumRoll = grng->random(standardEnchantmentChanceSumByItemType[outItem->type()]) + 1;
                int currentEnchantmentChancesSum = 0;
                for (Attribute attr : allEnchantableAttributes()) {
                    if (currentEnchantmentChancesSum >= enchantmentChanceSumRoll)
                        break;

                    currentEnchantmentChancesSum += standardEnchantments[attr].chanceByItemType[outItem->type()];
                    outItem->standardEnchantment = attr;
                }
                assert(outItem->standardEnchantment);

                outItem->standardEnchantmentStrength = grng->randomSample(standardEnchantmentRangeByTreasureLevel[treasureLevel]);
                Attribute standardEnchantmentAttributeSkill = *outItem->standardEnchantment;
                if (standardEnchantmentAttributeSkill == ATTRIBUTE_SKILL_ARMSMASTER ||
                    standardEnchantmentAttributeSkill == ATTRIBUTE_SKILL_DODGE ||
                    standardEnchantmentAttributeSkill == ATTRIBUTE_SKILL_UNARMED) {
                    outItem->standardEnchantmentStrength /= 2;
                }
                // if enchantment generated, it needs to actually have an effect
                if (outItem->standardEnchantmentStrength <= 0) {
                    outItem->standardEnchantmentStrength = 1;
                }
                return;
            } else if (bonusChanceRoll >= standardEnchantmentChanceForEquipment[treasureLevel] + specialEnchantmentChanceForEquipment[treasureLevel]) {
                return;
            }
        }
            break;
        case ITEM_TYPE_WAND:
            outItem->numCharges = grng->random(6) + outItem->GetDamageMod() + 1;
            outItem->maxCharges = outItem->numCharges;
            return;
        default:
            return;
    }

    cumulativeWeights.clear();
    weightSum = 0;
    for (ItemEnchantment ench : specialEnchantments.indices()) {
        int tr_lv = specialEnchantments[ench].enchantmentLevel;

        // tr_lv  0 = treasure level 3/4
        // tr_lv  1 = treasure level 3/4/5
        // tr_lv  2 = treasure level 4/5
        // tr_lv  3 = treasure level 5/6

        if ((treasureLevel == ITEM_TREASURE_LEVEL_3) && (tr_lv == 1 || tr_lv == 0) ||
            (treasureLevel == ITEM_TREASURE_LEVEL_4) && (tr_lv == 2 || tr_lv == 1 || tr_lv == 0) ||
            (treasureLevel == ITEM_TREASURE_LEVEL_5) && (tr_lv == 3 || tr_lv == 2 || tr_lv == 1) ||
            (treasureLevel == ITEM_TREASURE_LEVEL_6) && (tr_lv == 3)) {
            int spc = specialEnchantments[ench].chanceByItemType[outItem->type()];
            if (spc) {
                weightSum += spc;
                possibleEnchantments.push_back(ench);
                cumulativeWeights.push_back(weightSum);
            }
        }
    }

    int pickedWeight = grng->random(weightSum) + 1;
    auto foundWeight = std::lower_bound(cumulativeWeights.begin(), cumulativeWeights.end(), pickedWeight);
    assert(foundWeight != cumulativeWeights.end());
    outItem->specialEnchantment = possibleEnchantments[std::distance(cumulativeWeights.begin(), foundWeight)];
}

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "Testing/Game/GameTest.h"

#include "Engine/Engine.h"
#include "Engine/Evt/EvtProgram.h"
#include "Engine/Evt/Processor.h"
#include "Engine/MapEnumFunctions.h"
#include "Engine/MapInfo.h"
#include "Engine/Party.h"
#include "Engine/Resources/EngineFileSystem.h"
#include "Engine/Resources/ResourceManager.h"
#include "Engine/mm7_data.h"

#include "Utility/Math/TrigLut.h"
#include "Engine/Graphics/BSPModel.h"
#include "Engine/Graphics/Image.h"
#include "Engine/Graphics/Indoor.h"
#include "Engine/Graphics/Outdoor.h"
#include "Engine/Graphics/LocationFunctions.h"
#include "Engine/Graphics/Overlays.h"
#include "Engine/Graphics/Renderer/Renderer.h"
#include "Engine/Graphics/Sprites.h"
#include "Engine/Graphics/TurnBasedOverlay.h"
#include "Engine/Graphics/Weather.h"
#include "Engine/TurnEngine/TurnEngineEnums.h"
#include "Engine/Objects/Actor.h"
#include "Engine/Objects/CharacterEnumFunctions.h"
#include "Engine/Objects/Chest.h"
#include "Engine/Objects/ItemEnumFunctions.h"
#include "Engine/Objects/CombinedSkillValue.h"
#include "Engine/Objects/Decoration.h"
#include "Engine/Objects/MonsterEnumFunctions.h"
#include "Engine/Objects/Monsters.h"
#include "Engine/Objects/NPC.h"
#include "Engine/Objects/ObjectList.h"
#include "Engine/Objects/SpriteObject.h"
#include "Engine/Spells/CastSpellInfo.h"
#include "Engine/Spells/SpellEnums.h"
#include "Engine/Spells/SpellEnumFunctions.h"
#include "Engine/Spells/Spells.h"
#include "Engine/Tables/HouseTable.h"
#include "Engine/Tables/ItemTable.h"
#include "Engine/Tables/MessageScrollTable.h"
#include "Engine/Tables/NPCTable.h"
#include "Engine/Tables/TileTable.h"

#include "GUI/GUIButton.h"
#include "GUI/GUIWindow.h"
#include "GUI/UI/UIDialogue.h"
#include "GUI/UI/UIHouses.h"
#include "GUI/UI/UIMessageScroll.h"
#include "GUI/UI/UISpell.h"
#include "GUI/UI/Houses/Shops.h"
#include "GUI/UI/Houses/Transport.h"

#include "Io/Mouse.h"

// MM6 bring-up tests. These require MM6 game data and only run when the test binary is
// invoked with '--game-version mm6'; under the default MM7 test suite they are skipped.

GAME_TEST(Mm6, NewGame) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // MM6 new game starts outdoors in New Sorpigal.
    EXPECT_EQ(uCurrentlyLoadedLevelType, LEVEL_OUTDOOR);
    EXPECT_EQ(pMapStats->pInfos[engine->_currentLoadedMapId].fileName, "oute3.odm");

    // Entities placed in oute3.ddm should have been loaded: 38 peasants (MM6 monster ids 121-135,
    // random encounter spawns can add more on top), 42 sprite objects and 20 chests.
    int placedPeasants = 0;
    for (const Actor &actor : pActors) {
        int monsterId = std::to_underlying(actor.monsterId);
        placedPeasants += monsterId >= 121 && monsterId <= 135;
    }
    EXPECT_GE(placedPeasants, 38);
    EXPECT_TRUE(std::ranges::any_of(pActors, [](const Actor &actor) {
        // First actor record of oute3.ddm - a level 3 peasant (PeasantF1C).
        return std::to_underlying(actor.monsterId) == 123 && actor.initialPosition == Vec3f(-10296, -7528, 160);
    }));
    EXPECT_GE(pSpriteObjects.size(), 42u);
    EXPECT_TRUE(std::ranges::any_of(pSpriteObjects, [](const SpriteObject &object) {
        return object.containing_item.itemId == static_cast<ItemId>(160); // Most common oute3.ddm sprite object.
    }));
    EXPECT_EQ(vChests.size(), 20u);

    game.tick(10); // And the game loop should be able to run for a bit without crashing.
}

GAME_TEST(Mm6, NewGameDefaults) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    game.tick(1);

    // The authoritative MM6 new-game state lives in new.lod's party.bin, a savegame template:
    // 200 gold, 7 food, quest bits 81 (The Letter delivery quest active) and 181 set, and the
    // default party Roderick/Alexis/Serena/Zoltan with fixed stats, skills, spells and gear.
    EXPECT_EQ(pParty->GetGold(), 200);
    EXPECT_EQ(pParty->GetFood(), 7);
    EXPECT_TRUE(pParty->_questBits[static_cast<QuestBit>(81)]);
    EXPECT_TRUE(pParty->_questBits[static_cast<QuestBit>(181)]);
    EXPECT_FALSE(pParty->_questBits[QBIT_EMERALD_ISLAND_RED_POTION_ACTIVE]); // No MM7 leakage.

    // The party.bin start pose: New Sorpigal, facing north.
    EXPECT_EQ(pMapStats->pInfos[engine->_currentLoadedMapId].fileName, "oute3.odm");
    EXPECT_EQ(pParty->pos.x, -9728);
    EXPECT_EQ(pParty->pos.y, -11319);
    EXPECT_NEAR(pParty->pos.z, 160, 1);
    EXPECT_EQ(pParty->_viewYaw, 512);

    // The autosave is named after the game version, so MM6 and MM7 saves don't mix.
    EXPECT_TRUE(ufs->exists("saves/autosave.mm6"));
    EXPECT_FALSE(ufs->exists("saves/autosave.mm7"));

    struct DefaultCharacter {
        const char *name;
        Class classType;
        Sex sex;
        int face;
        std::array<int, 7> stats; // Might, Intellect, Personality, Endurance, Accuracy, Speed, Luck.
        std::array<Skill, 4> skills;
        std::vector<int> spells;
        int experience;
        int age;
        int hp;
        int sp;
        std::vector<int> backpack;
        int mainHand;
    };
    // Every caster knows the first spell of their school and carries the book of the second.
    std::array<DefaultCharacter, 4> expected = {{
        {"Roderick", CLASS_PALADIN, SEX_MALE, 0, {17, 5, 15, 15, 15, 13, 6},
         {SKILL_SWORD, SKILL_SHIELD, SKILL_CHAIN, SKILL_SPIRIT}, {45}, 343, 21, 31, 7,
         {124, 345, 505}, 1}, // Blessed Ring, Bless book, The Letter; Longsword.
        {"Alexis", CLASS_ARCHER, SEX_FEMALE, 11, {14, 15, 5, 15, 17, 13, 6},
         {SKILL_AXE, SKILL_BOW, SKILL_AIR, SKILL_PERCEPTION}, {12}, 291, 21, 31, 7,
         {122, 312, 163, 160}, 23}, // Lunar Ring, Static Charge book, bottle, Poppysnaps; Hand Axe.
        {"Serena", CLASS_CLERIC, SEX_FEMALE, 9, {11, 7, 17, 15, 13, 11, 12},
         {SKILL_MACE, SKILL_MIND, SKILL_BODY, SKILL_MEDITATION}, {56, 67}, 266, 22, 24, 22,
         {121, 356, 367, 163, 162}, 50}, // Sparkling Ring, 2 books, bottle, Widoweeps Berries; Mace.
        {"Zoltan", CLASS_SORCERER, SEX_MALE, 7, {11, 17, 7, 15, 13, 13, 9},
         {SKILL_DAGGER, SKILL_FIRE, SKILL_WATER, SKILL_MEDITATION}, {1, 23}, 336, 25, 24, 22,
         {122, 301, 323, 163, 160}, 15}, // Lunar Ring, 2 books, bottle, Poppysnaps; Dagger.
    }};

    for (int i = 0; i < 4; i++) {
        const DefaultCharacter &want = expected[i];
        const Character &have = pParty->pCharacters[i];
        EXPECT_EQ(have.name, want.name);
        EXPECT_EQ(have.classType, want.classType) << want.name;
        EXPECT_EQ(have.uSex, want.sex) << want.name;
        EXPECT_EQ(have.uCurrentFace, want.face) << want.name;
        EXPECT_EQ(have.uLevel, 1) << want.name;
        EXPECT_EQ(have.experience, want.experience) << want.name;
        EXPECT_EQ(have.GetBaseAge(), want.age) << want.name;

        for (int s = 0; s < 7; s++)
            EXPECT_EQ(have._stats[static_cast<Attribute>(s)], want.stats[s]) << want.name << " stat " << s;

        int activeSkills = 0;
        for (Skill skill : allSkills())
            activeSkills += static_cast<bool>(have.pActiveSkills[skill]);
        EXPECT_EQ(activeSkills, 4) << want.name;
        for (Skill skill : want.skills)
            EXPECT_EQ(have.getSkillValue(skill), CombinedSkillValue::novice()) << want.name;

        int knownSpells = 0;
        for (SpellId spell : have.bHaveSpell.indices())
            knownSpells += have.bHaveSpell[spell];
        EXPECT_EQ(knownSpells, static_cast<int>(want.spells.size())) << want.name;
        for (int spell : want.spells)
            EXPECT_TRUE(have.bHaveSpell[static_cast<SpellId>(spell)]) << want.name << " spell " << spell;

        InventoryConstEntry mainHand = have.inventory.entry(ITEM_SLOT_MAIN_HAND);
        ASSERT_TRUE(mainHand) << want.name;
        EXPECT_EQ(mainHand->itemId, static_cast<ItemId>(want.mainHand)) << want.name;
        for (int itemId : want.backpack)
            EXPECT_TRUE(have.inventory.find(static_cast<ItemId>(itemId))) << want.name << " item " << itemId;
        for (InventoryConstEntry entry : have.inventory.entries())
            EXPECT_TRUE(entry->IsIdentified()) << want.name;

        // MM6 has no races, and max HP/SP come from the MM6 class tables - the template's values.
        EXPECT_EQ(have.GetRace(), RACE_HUMAN) << want.name;
        EXPECT_EQ(have.GetMaxHealth(), want.hp) << want.name;
        EXPECT_EQ(have.GetMaxMana(), want.sp) << want.name;
        EXPECT_EQ(have.health, have.GetMaxHealth()) << want.name;
        EXPECT_EQ(have.mana, have.GetMaxMana()) << want.name;
    }

    // MM6 starts in year 1165 (MM7 in 1168).
    EXPECT_EQ(pParty->GetPlayingTime().toCivilTime().year, 1165);

    // HP/SP growth: a paladin gains 3 hp & 1 sp per level, and each promotion tier
    // adds another +1 hp & +1 sp per level.
    Character &roderick = pParty->pCharacters[0];
    roderick.uLevel = 2;
    EXPECT_EQ(roderick.GetMaxHealth(), 34); // 25 + 3 * (2 + endBonus 1).
    EXPECT_EQ(roderick.GetMaxMana(), 8);    // 5 + 1 * (2 + perBonus 1).
    roderick.classType = CLASS_CRUSADER;
    EXPECT_EQ(roderick.GetMaxHealth(), 37);
    EXPECT_EQ(roderick.GetMaxMana(), 11);
    roderick.classType = CLASS_HERO;
    EXPECT_EQ(roderick.GetMaxHealth(), 40);
    EXPECT_EQ(roderick.GetMaxMana(), 14);
    roderick.classType = CLASS_PALADIN;
    roderick.uLevel = 1;

    // Roderick's shield hand, armor, and his ring's rolled enchantment ("of Magic" +3).
    EXPECT_EQ(pParty->pCharacters[0].inventory.entry(ITEM_SLOT_OFF_HAND)->itemId, static_cast<ItemId>(84));
    EXPECT_EQ(pParty->pCharacters[0].inventory.entry(ITEM_SLOT_ARMOUR)->itemId, static_cast<ItemId>(71));
    InventoryConstEntry blessedRing = pParty->pCharacters[0].inventory.find(static_cast<ItemId>(124));
    ASSERT_TRUE(blessedRing);
    EXPECT_EQ(blessedRing->standardEnchantment, ATTRIBUTE_MANA);
    EXPECT_EQ(blessedRing->standardEnchantmentStrength, 3);

    // Alexis' bow slot and her ring's enchantment ("of Fire Resistance" +1).
    EXPECT_EQ(pParty->pCharacters[1].inventory.entry(ITEM_SLOT_BOW)->itemId, static_cast<ItemId>(47));
    InventoryConstEntry lunarRing = pParty->pCharacters[1].inventory.find(static_cast<ItemId>(122));
    ASSERT_TRUE(lunarRing);
    EXPECT_EQ(lunarRing->standardEnchantment, ATTRIBUTE_RESIST_FIRE);
    EXPECT_EQ(lunarRing->standardEnchantmentStrength, 1);

    // Zoltan's ring rolled no enchantment.
    InventoryConstEntry plainRing = pParty->pCharacters[3].inventory.find(static_cast<ItemId>(122));
    ASSERT_TRUE(plainRing);
    EXPECT_EQ(plainRing->standardEnchantment, std::nullopt);
}

GAME_TEST(Mm6, WalkAndInteract) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // Walking forward moves the party.
    Vec3f posBefore = pParty->pos;
    game.pressKey(PlatformKey::KEY_UP);
    game.tick(20);
    game.releaseKey(PlatformKey::KEY_UP);
    game.tick(1);
    EXPECT_NE(pParty->pos, posBefore);

    // Turning changes view yaw but not position.
    int yawBefore = pParty->_viewYaw;
    posBefore = pParty->pos;
    game.pressKey(PlatformKey::KEY_LEFT);
    game.tick(5);
    game.releaseKey(PlatformKey::KEY_LEFT);
    game.tick(1);
    EXPECT_NE(pParty->_viewYaw, yawBefore);
    EXPECT_EQ(pParty->pos, posBefore);

    // Pressing the interact key with nothing targeted shouldn't crash.
    game.pressAndReleaseKey(PlatformKey::KEY_SPACE);
    game.tick(5);

    // And the world should keep simulating - actor AI, animations, ambient sounds - without crashing.
    game.tick(200);
}

GAME_TEST(Mm6, KillAndLootPeasant) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // Find the known placed level 3 peasant (first actor record of oute3.ddm).
    auto peasant = std::ranges::find_if(pActors, [](const Actor &actor) {
        return std::to_underlying(actor.monsterId) == 123 && actor.initialPosition == Vec3f(-10296, -7528, 160);
    });
    ASSERT_NE(peasant, pActors.end());
    int peasantId = peasant->id;

    // The ddm-embedded stat block is what the engine uses (matching the original), and for this actor it
    // slightly diverges from monsters.txt row 123 (PeasantF1C): 3D6 gold instead of the txt's 4D6.
    EXPECT_EQ(peasant->monsterInfo.exp, 39);
    EXPECT_EQ(peasant->monsterInfo.goldDiceRolls, 3);
    EXPECT_EQ(peasant->monsterInfo.goldDiceSides, 6);

    auto teleportNextTo = [&](Vec3f targetPos, float distance) {
        Vec3f pos = targetPos + Vec3f(-distance, 0, 0);
        int yawDegrees = TrigLUT.atan2(targetPos.x - pos.x, targetPos.y - pos.y) * 90 / 512;
        game.teleportTo(engine->_currentLoadedMapId, pos, yawDegrees);
        game.tick(1);
    };

    uint64_t expBefore = 0;
    for (const Character &character : pParty->pCharacters)
        expBefore += character.experience;
    int goldBefore = pParty->GetGold();

    // Melee the peasant until it dies, chasing it if it flees.
    for (int i = 0; i < 100 && pActors[peasantId].aiState != Dead; i++) {
        teleportNextTo(pActors[peasantId].pos, 160);
        game.pressAndReleaseKey(PlatformKey::KEY_A);
        game.tick(2);
    }
    EXPECT_EQ(pActors[peasantId].aiState, Dead);

    // The kill awards the monster's exp, split between the four party members.
    uint64_t expAfter = 0;
    for (const Character &character : pParty->pCharacters)
        expAfter += character.experience;
    EXPECT_GE(expAfter - expBefore, 36u); // 39 / 4 = 9 per character, at least.

    // Looting the corpse with the interact key rolls the peasant's 3D6 gold dice. Loot from a bit further
    // out: a corpse right at the party's feet projects below the game viewport and isn't pickable, just
    // like in the original. The peasant flees mid-fight and can die with scenery or bystanders in the
    // way, so try several approach sides and distances until the pick lands on the corpse.
    int goldFound = 0;
    Vec3f corpsePos = pActors[peasantId].pos;
    for (Vec3f offset : {Vec3f(-350, 0, 0), Vec3f(350, 0, 0), Vec3f(0, -350, 0), Vec3f(0, 350, 0),
                         Vec3f(-250, 0, 0), Vec3f(250, 0, 0), Vec3f(0, -250, 0), Vec3f(0, 250, 0)}) {
        Vec3f pos = corpsePos + offset;
        int yawDegrees = TrigLUT.atan2(corpsePos.x - pos.x, corpsePos.y - pos.y) * 90 / 512;
        game.teleportTo(engine->_currentLoadedMapId, pos, yawDegrees);
        game.tick(1);
        game.pressAndReleaseKey(PlatformKey::KEY_SPACE);
        game.tick(2);
        if (current_screen_type != SCREEN_GAME) { // Picked a live bystander instead - dismiss its dialogue and retry.
            game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
            game.tick(2);
            continue;
        }
        goldFound = pParty->GetGold() - goldBefore;
        if (goldFound)
            break;
    }
    EXPECT_GE(goldFound, 3);
    EXPECT_LE(goldFound, 18);
}

GAME_TEST(Mm6, EnterGoblinwatch) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // Goblinwatch is New Sorpigal's dungeon, an indoor map.
    MapId goblinwatch = pMapStats->GetMapInfo("d01.blv");
    ASSERT_NE(goblinwatch, MAP_INVALID);
    game.teleportTo(goblinwatch, Vec3f(-1850, 4304, -512), 0); // First spawn point of d01.blv.

    // The blv geometry should have been loaded: d01.blv carries 2287 vertices, 2291 faces, 59 sectors
    // and 200 door records.
    EXPECT_EQ(uCurrentlyLoadedLevelType, LEVEL_INDOOR);
    EXPECT_EQ(pIndoor->vertices.size(), 2287u);
    EXPECT_EQ(pIndoor->faces.size(), 2291u);
    EXPECT_EQ(pIndoor->sectors.size(), 59u);
    EXPECT_EQ(pIndoor->doors.size(), 200u);

    // And the party should be inside the dungeon, in a valid sector.
    EXPECT_NE(pIndoor->GetSector(pParty->pos.x, pParty->pos.y, pParty->pos.z), 0);

    // First entry respawns the location: monsters are generated from the blv's 66 spawn points
    // (rats / goblins / bloodsuckers per mapstats.txt), and d01.dlv's 20 chest records are loaded.
    EXPECT_GE(pActors.size(), 66u);
    EXPECT_TRUE(std::ranges::all_of(pActors, [](const Actor &actor) {
        int monsterId = std::to_underlying(actor.monsterId);
        return (monsterId >= 13 && monsterId <= 15)      // Bloodsucker A-C.
            || (monsterId >= 76 && monsterId <= 78)      // Goblin A-C.
            || (monsterId >= 145 && monsterId <= 147);   // Rat A-C.
    }));
    EXPECT_EQ(vChests.size(), 20u);

    // The game loop should keep running: actor AI, animations, doors.
    game.tick(20);
}

GAME_TEST(Mm6, EnterTempleOfBaaThroughDoor) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // New Sorpigal's Abandoned Temple of Baa entrance is the door face wired to local event 102,
    // an ungated MoveToMap into d02.blv.
    const BLVFace *door = nullptr;
    for (const BSPModel &model : pOutdoor->pBModels) {
        for (const BLVFace &face : model.faces) {
            if (face.eventId == 102 && face.Clickable()) {
                door = &face;
                break;
            }
        }
    }
    ASSERT_NE(door, nullptr);

    // Stand right in front of the door, facing it.
    Vec3f doorCenter = door->boundingBox.center();
    Vec3f pos = doorCenter + door->facePlane.normal * 130;
    pos.z = door->boundingBox.z1; // Feet on the ground, not at the door's mid-height.
    int yawDegrees = TrigLUT.atan2(doorCenter.x - pos.x, doorCenter.y - pos.y) * 90 / 512;
    game.teleportTo(engine->_currentLoadedMapId, pos, yawDegrees);
    game.tick(1);

    // Interacting with the door fires event 102, which opens the enter-the-dungeon prompt.
    game.pressAndReleaseKey(PlatformKey::KEY_SPACE);
    game.tick(1);

    // Confirming it loads the Abandoned Temple.
    game.pressAndReleaseKey(PlatformKey::KEY_Y);
    game.tick(5);
    EXPECT_EQ(uCurrentlyLoadedLevelType, LEVEL_INDOOR);
    EXPECT_EQ(pMapStats->pInfos[engine->_currentLoadedMapId].fileName, "d02.blv");

    // The event's MoveToMap teleports the party to the temple's entrance and the map is live.
    EXPECT_EQ(pParty->pos, Vec3f(16406, -19669, 865));
    EXPECT_NE(pIndoor->GetSector(pParty->pos.x, pParty->pos.y, pParty->pos.z), 0);
    game.tick(20);
}

GAME_TEST(Mm6, OpenChestInGoblinwatch) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    MapId goblinwatch = pMapStats->GetMapInfo("d01.blv");
    ASSERT_NE(goblinwatch, MAP_INVALID);
    game.teleportTo(goblinwatch, Vec3f(-1850, 4304, -512), 0); // First spawn point of d01.blv.

    // Find a clickable vertical (side-facing) face wired to an OpenChest event, so that the party
    // can stand in front of it. Prefer an untrapped chest so that interacting opens the chest UI
    // instead of setting off the trap.
    const BLVFace *chestFace = nullptr;
    int chestId = -1;
    for (const BLVFace &face : pIndoor->faces) {
        if (!face.eventId || !face.Clickable() || !engine->_localEventMap.hasEvent(face.eventId))
            continue;
        if (std::abs(face.facePlane.normal.z) >= 0.5f)
            continue; // A chest lid/floor plate - the party can't stand in front of it.
        for (const EvtInstruction &instruction : engine->_localEventMap.function(face.eventId)) {
            if (instruction.opcode == EVENT_OpenChest && !vChests[instruction.data.chest_id].Trapped()) {
                chestFace = &face;
                chestId = instruction.data.chest_id;
                break;
            }
        }
        if (chestFace)
            break;
    }
    ASSERT_NE(chestFace, nullptr);

    // Stand in front of the chest, facing it.
    Vec3f chestCenter = chestFace->boundingBox.center();
    Vec3f pos = chestCenter + chestFace->facePlane.normal * 130;
    pos.z = chestFace->boundingBox.z1;
    int yawDegrees = TrigLUT.atan2(chestCenter.x - pos.x, chestCenter.y - pos.y) * 90 / 512;
    game.teleportTo(goblinwatch, pos, yawDegrees);
    game.tick(1);

    // Interacting with the chest fires its OpenChest event and brings up the chest screen.
    game.pressAndReleaseKey(PlatformKey::KEY_SPACE);
    game.tick(1);
    EXPECT_EQ(current_screen_type, SCREEN_CHEST);

    // The opened chest was set up: its stash was laid out on the chest grid.
    EXPECT_TRUE(vChests[chestId].Initialized());

    // With MM6 item data loaded, chest contents are real items: every item in every chest of the
    // dungeon resolves to a named items.txt entry, and the chests aren't all empty.
    int chestItemCount = 0;
    for (const Chest &chest : vChests) {
        for (auto entry : chest.inventory.entries()) {
            EXPECT_FALSE(pItemTable->items[(*entry).itemId].name.empty())
                << "item id " << std::to_underlying((*entry).itemId);
            chestItemCount++;
        }
    }
    EXPECT_GT(chestItemCount, 0);

    // Escape closes the chest and the game is live again.
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_GAME);
    game.tick(20);
}

GAME_TEST(Mm6, SwitchChangesDoorStateInGoblinwatch) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    MapId goblinwatch = pMapStats->GetMapInfo("d01.blv");
    ASSERT_NE(goblinwatch, MAP_INVALID);
    game.teleportTo(goblinwatch, Vec3f(-1850, 4304, -512), 0); // First spawn point of d01.blv.

    // Find a clickable vertical face whose event script starts with a ChangeDoorState - a switch
    // or a door handle - and locate the door it drives.
    const BLVFace *switchFace = nullptr;
    int doorId = -1;
    for (const BLVFace &face : pIndoor->faces) {
        if (!face.eventId || !face.Clickable() || !engine->_localEventMap.hasEvent(face.eventId))
            continue;
        if (std::abs(face.facePlane.normal.z) >= 0.5f)
            continue;
        const std::vector<EvtInstruction> &script = engine->_localEventMap.function(face.eventId);
        if (script.size() >= 2 && script[1].opcode == EVENT_ChangeDoorState) {
            switchFace = &face;
            doorId = script[1].data.door_descr.door_id;
            break;
        }
    }
    ASSERT_NE(switchFace, nullptr);

    BLVDoor *door = nullptr;
    for (BLVDoor &candidate : pIndoor->doors) {
        if (candidate.doorId == static_cast<uint32_t>(doorId)) {
            door = &candidate;
            break;
        }
    }
    ASSERT_NE(door, nullptr);
    DoorState stateBefore = door->state;

    // Stand in front of the switch, facing it.
    Vec3f switchCenter = switchFace->boundingBox.center();
    Vec3f pos = switchCenter + switchFace->facePlane.normal * 130;
    pos.z = switchFace->boundingBox.z1;
    int yawDegrees = TrigLUT.atan2(switchCenter.x - pos.x, switchCenter.y - pos.y) * 90 / 512;
    game.teleportTo(goblinwatch, pos, yawDegrees);
    game.tick(1);

    // Interacting fires the event, and the door it drives starts moving.
    game.pressAndReleaseKey(PlatformKey::KEY_SPACE);
    game.tick(2);
    EXPECT_NE(door->state, stateBefore);
    game.tick(20); // And the door animation keeps the game loop happy.
}

GAME_TEST(Mm6, AllMapEventsParse) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // The global event map is parsed during engine initialization; its first event is a Compare record.
    EXPECT_TRUE(engine->_globalEventMap.hasEvent(1));

    // Local events of every MM6 map parse with the MM6 opcode/operand layouts.
    int parsed = 0;
    for (MapId mapId : allMaps()) {
        if (!isMapIndoor(mapId) && !isMapOutdoor(mapId))
            continue; // Not an MM6 map slot.
        std::string fileName = pMapStats->pInfos[mapId].fileName;
        std::string baseName = fileName.substr(0, fileName.rfind('.'));
        EXPECT_NO_THROW(EvtProgram::load(engine->resources()->eventsData(baseName + ".evt"), engine->gameVersion())) << fileName;
        parsed++;
    }
    EXPECT_EQ(parsed, 67); // All of MM6's maps.
}

GAME_TEST(Mm6, ClassPromotion) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // MM6 stores a character's class as a byte, base*3 + tier, base order Knight/Cleric/Sorcerer/
    // Paladin/Archer/Druid (see mm6-character-model.md). Promotion events drive it via
    // If(Class == byte) / Set(Class, byte); the byte must map onto the engine's MM7-shaped Class enum,
    // else a promotion gate misfires (a Paladin, CLASS_PALADIN=12, fails If(Class == 9)) or lands on
    // a garbage class (Set(Class, 10) would be CLASS_MASTER, not Crusader).
    struct PromotionCase { int mm6Byte; Class expected; };
    static const std::array<PromotionCase, 18> cases = {{
        {0, CLASS_KNIGHT}, {1, CLASS_CAVALIER}, {2, CLASS_CHAMPION},
        {3, CLASS_CLERIC}, {4, CLASS_PRIEST}, {5, CLASS_PRIEST_OF_SUN},
        {6, CLASS_SORCERER}, {7, CLASS_WIZARD}, {8, CLASS_ARCHAMGE},
        {9, CLASS_PALADIN}, {10, CLASS_CRUSADER}, {11, CLASS_HERO},
        {12, CLASS_ARCHER}, {13, CLASS_WARRIOR_MAGE}, {14, CLASS_MASTER_ARCHER},
        {15, CLASS_DRUID}, {16, CLASS_GREAT_DRUID}, {17, CLASS_ARCH_DRUID},
    }};

    Character &character = pParty->pCharacters[0];
    for (const PromotionCase &c : cases) {
        // Set(Class, byte) - the promotion "grant" - lands on the right enum class...
        character.SetVariable(VAR_Class, c.mm6Byte);
        EXPECT_EQ(character.classType, c.expected) << "Set(Class, " << c.mm6Byte << ")";
        // ...and If(Class == byte) - the promotion "gate" - matches the class just set, and nothing else.
        EXPECT_TRUE(character.CompareVariable(VAR_Class, c.mm6Byte)) << "If(Class == " << c.mm6Byte << ")";
        if (c.mm6Byte != 9) // Paladin.
            EXPECT_FALSE(character.CompareVariable(VAR_Class, 9)) << "byte 9 must not match byte " << c.mm6Byte;
    }

    // Promotion grows HP/SP by a tier: Paladin(3/1) -> Crusader(4/2) -> Hero(5/3) per level.
    character.SetVariable(VAR_Class, 9); // Paladin.
    int paladinHp = character.GetMaxHealth(), paladinSp = character.GetMaxMana();
    character.SetVariable(VAR_Class, 10); // Crusader.
    int crusaderHp = character.GetMaxHealth(), crusaderSp = character.GetMaxMana();
    character.SetVariable(VAR_Class, 11); // Hero.
    EXPECT_GT(crusaderHp, paladinHp);
    EXPECT_GT(crusaderSp, paladinSp);
    EXPECT_GT(character.GetMaxHealth(), crusaderHp);
    EXPECT_GT(character.GetMaxMana(), crusaderSp);

    // End-to-end: run the real Cleric->Priest promotion (global event 36), the effect an NPC promotion
    // topic executes. Its class byte differs from its enum (Cleric=3 vs CLASS_CLERIC=24), so it only
    // promotes once the translation is in place. Start at step 4 to skip the QBits[106] quest gate and
    // its promotion-speech ShowMessage; the mechanic (per-member ForPartyMember -> If(Class==3) ->
    // Set(Class,4)) runs from there. Global events resolve against the global map only while a level
    // decoration is active, so point at a scratch one for the call. The default MM6 party's Serena is
    // the Cleric (member 2); the non-Clerics must be left untouched.
    Character &serena = pParty->pCharacters[2];
    ASSERT_EQ(serena.classType, CLASS_CLERIC);
    Class roderickClass = pParty->pCharacters[0].classType; // Roderick, a Paladin - not a Cleric.
    LevelDecoration scratchDecoration;
    LevelDecoration *savedDecoration = activeLevelDecoration;
    activeLevelDecoration = &scratchDecoration;
    eventProcessor(36, Pid(), false, 4);
    activeLevelDecoration = savedDecoration;
    EXPECT_EQ(serena.classType, CLASS_PRIEST);
    EXPECT_EQ(pParty->pCharacters[0].classType, roderickClass);
}

GAME_TEST(Mm6, RiddlePasswordPrompt) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // The Dragoons' Caverns (cd1.blv) sword-in-the-rock is MM6's password prompt: local event 69
    // asks "What's the password?" (answers JBARD / jbard). A correct answer jumps to the
    // MapVars[6] = 1 branch, a wrong answer falls through into a teleport trap.
    MapId caverns = pMapStats->GetMapInfo("cd1.blv");
    ASSERT_NE(caverns, MAP_INVALID);
    game.teleportTo(caverns, Vec3f(-3136, 2240, 224), 0); // A known-valid cd1 position (the event's own teleport target).

    // Event 69 hangs on a pressure-plate floor face: it fires when the party steps onto it.
    const BLVFace *plate = nullptr;
    for (const BLVFace &face : pIndoor->faces) {
        if (face.eventId == 69 && (face.attributes & FACE_PRESSURE_PLATE)) {
            plate = &face;
            break;
        }
    }
    ASSERT_NE(plate, nullptr);
    Vec3f platePos = plate->boundingBox.center();
    platePos.z = plate->boundingBox.z1;
    Vec3f awayPos = platePos + Vec3f(300, 0, 0);

    // Stepping onto the plate opens the password prompt.
    game.teleportTo(caverns, awayPos, 0);
    game.tick(1);
    game.teleportTo(caverns, platePos, 0);
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_BRANCHLESS_NPC_DIALOG);

    // Escape cancels the prompt without taking either event branch: no trap teleport, no unlock.
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_GAME);
    EXPECT_LE((pParty->pos - platePos).length(), 256.0f);
    EXPECT_EQ(engine->_persistentVariables.mapVars[6], 0);

    // Step off and back on: the prompt reopens (pressure plates are edge-triggered).
    game.teleportTo(caverns, awayPos, 0);
    game.tick(2);
    game.teleportTo(caverns, platePos, 0);
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_BRANCHLESS_NPC_DIALOG);

    // Typing the password unlocks: the event jumps to its correct-answer branch.
    for (PlatformKey key : {PlatformKey::KEY_J, PlatformKey::KEY_B, PlatformKey::KEY_A, PlatformKey::KEY_R, PlatformKey::KEY_D}) {
        game.pressAndReleaseKey(key);
        game.tick(1);
    }
    game.pressAndReleaseKey(PlatformKey::KEY_RETURN);
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_GAME);
    EXPECT_LE((pParty->pos - platePos).length(), 256.0f); // Not teleported into the wrong-answer trap.
    EXPECT_EQ(engine->_persistentVariables.mapVars[6], 1);
    game.tick(5);
}

GAME_TEST(Mm6, QuestItemPickup) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // MM6 item data is loaded: the Fly spell scroll is items.txt row 220, and the rnditems.txt
    // random-generation chances are populated.
    EXPECT_EQ(pItemTable->items[ItemId(220)].name, "Fly");
    EXPECT_EQ(pItemTable->items[ItemId(220)].iconName, "scroll4");
    EXPECT_EQ(pItemTable->items[ItemId(220)].type, ITEM_TYPE_SPELL_SCROLL);
    EXPECT_EQ(pItemTable->items[ItemId(220)].baseValue, 300);
    EXPECT_GT(pItemTable->itemChanceSumByTreasureLevel[ITEM_TREASURE_LEVEL_1], 0);

    // New Sorpigal's free Fly scroll: local event 225 is an ungated Add(ItemInHands, 220) pickup
    // hanging on a clickable outdoor model face.
    const BLVFace *scrollFace = nullptr;
    for (const BSPModel &model : pOutdoor->pBModels) {
        for (const BLVFace &face : model.faces) {
            if (face.eventId == 225 && face.Clickable()) {
                scrollFace = &face;
                break;
            }
        }
    }
    ASSERT_NE(scrollFace, nullptr);

    // Stand in front of the scroll, facing it.
    Vec3f scrollCenter = scrollFace->boundingBox.center();
    Vec3f pos = scrollCenter + scrollFace->facePlane.normal * 130;
    pos.z = scrollFace->boundingBox.z1;
    int yawDegrees = TrigLUT.atan2(scrollCenter.x - pos.x, scrollCenter.y - pos.y) * 90 / 512;
    game.teleportTo(engine->_currentLoadedMapId, pos, yawDegrees);
    game.tick(1);

    // Interacting fires the event and the scroll lands in the party's hands (on the cursor).
    EXPECT_EQ(pParty->pPickedItem.itemId, ITEM_NULL);
    game.pressAndReleaseKey(PlatformKey::KEY_SPACE);
    game.tick(2);
    EXPECT_EQ(pParty->pPickedItem.itemId, ItemId(220));
    game.tick(5);
}

GAME_TEST(Mm6, EnterWeaponShop) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // The Knife Shoppe is New Sorpigal's weapon shop: 2dEvents.txt row 1, loaded into houseTable.
    EXPECT_EQ(houseTable[HouseId(1)].name, "The Knife Shoppe");
    EXPECT_EQ(houseTable[HouseId(1)].uType, HOUSE_TYPE_WEAPON_SHOP);

    // Its door face is wired to local event 17, an ungated SpeakInHouse(1).
    const BLVFace *door = nullptr;
    for (const BSPModel &model : pOutdoor->pBModels) {
        for (const BLVFace &face : model.faces) {
            if (face.eventId == 17 && face.Clickable()) {
                door = &face;
                break;
            }
        }
    }
    ASSERT_NE(door, nullptr);

    // Stand in front of the door, facing it.
    Vec3f doorCenter = door->boundingBox.center();
    Vec3f pos = doorCenter + door->facePlane.normal * 130;
    pos.z = door->boundingBox.z1;
    int yawDegrees = TrigLUT.atan2(doorCenter.x - pos.x, doorCenter.y - pos.y) * 90 / 512;
    game.teleportTo(engine->_currentLoadedMapId, pos, yawDegrees);
    game.tick(1);

    // Interacting with the door fires the SpeakInHouse event and brings up the house screen.
    game.pressAndReleaseKey(PlatformKey::KEY_SPACE);
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_HOUSE);
    ASSERT_NE(window_SpeakInHouse, nullptr);
    EXPECT_EQ(window_SpeakInHouse->houseId(), HouseId(1));

    // Escape leaves the shop and the game is live again.
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_GAME);
    game.tick(5);
}

GAME_TEST(Mm6, BuyFromWeaponShop) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // Enter The Knife Shoppe (see Mm6.EnterWeaponShop for the door mechanics).
    const BLVFace *door = nullptr;
    for (const BSPModel &model : pOutdoor->pBModels) {
        for (const BLVFace &face : model.faces) {
            if (face.eventId == 17 && face.Clickable()) {
                door = &face;
                break;
            }
        }
    }
    ASSERT_NE(door, nullptr);
    Vec3f doorCenter = door->boundingBox.center();
    Vec3f pos = doorCenter + door->facePlane.normal * 130;
    pos.z = door->boundingBox.z1;
    int yawDegrees = TrigLUT.atan2(doorCenter.x - pos.x, doorCenter.y - pos.y) * 90 / 512;
    game.teleportTo(engine->_currentLoadedMapId, pos, yawDegrees);
    game.tick(1);
    game.pressAndReleaseKey(PlatformKey::KEY_SPACE);
    game.tick(2);
    ASSERT_EQ(current_screen_type, SCREEN_HOUSE);

    pParty->SetGold(2000); // Enough for any treasure-level-1 weapon.

    // Pick "Buy Standard Goods" - the first right-hand dialogue option.
    game.pressAndReleaseButton(BUTTON_LEFT, 550, 160);
    game.tick(2);

    // Entering the buy screen generated the shop's stock from MM6 item data.
    const std::array<Item, 12> &stock = pParty->standartItemsInShops[HouseId(1)];
    int slot = -1;
    for (int i = 0; i < 6; i++) {
        if (stock[i].itemId != ITEM_NULL) {
            slot = i;
            break;
        }
    }
    ASSERT_NE(slot, -1);
    ItemId stockItem = stock[slot].itemId;
    EXPECT_FALSE(pItemTable->items[stockItem].name.empty());

    // Click the item to buy it: gold is paid and it moves into the active character's inventory.
    // Click coordinates mirror GUIWindow_Shop::houseScreenClick's hit test: the slot's icon is
    // horizontally centered at 60 + slot * 70, vertically at weaponYPos[slot] + 30 plus half the icon.
    int goldBefore = pParty->GetGold();
    int x = 60 + slot * 70;
    int y = weaponYPos[slot] + 30 + shop_ui_items_in_store[slot]->height() / 2;
    game.pressAndReleaseButton(BUTTON_LEFT, x, y);
    game.tick(2);
    EXPECT_LT(pParty->GetGold(), goldBefore);
    EXPECT_EQ(stock[slot].itemId, ITEM_NULL); // The shelf slot sold out.
    EXPECT_TRUE(pParty->activeCharacter().inventory.find(stockItem));

    // Leave the buy screen, then the shop.
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_GAME);
    game.tick(5);
}

// MM6 street townsfolk are generated citizens: the first talk to a peasant actor generates a random
// citizen lazily (towns place far more peasants than the citizen buffer holds) - sex from the peasant's
// monster row (PeasantF* rows 121-132 / PeasantM* rows 133-144; the ddm npcId carries no reliable sex),
// name by sex from npcnames.txt, profession rolled on npcprof.txt's "Random Chance" column, portrait
// from the npc501..npc554 commoner block. Talking opens the standard hireable-NPC dialogue: it greets
// with a regional news line (npcnews.txt - the old bare-news flow folded into the dialogue) and offers
// the profession-details and hire topics; hiring pays the profession's hire price and puts the citizen
// in the party.
GAME_TEST(Mm6, StreetCitizenDialogueAndHire) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // No eager generation at level load - peasants keep their raw ddm npcId (0/1/2, all below the
    // 5000+ generated-NPC handles).
    int peasants = 0;
    for (const Actor &actor : pActors) {
        if (isPeasant(actor.monsterInfo.id, GAME_VERSION_MM6)) {
            EXPECT_LT(actor.npcId, 5000);
            peasants++;
        }
    }
    EXPECT_GT(peasants, 0);
    EXPECT_EQ(pNPCStats->uNewlNPCBufPos, 0);

    // Teleport right next to a placed peasant, facing it, and talk to it.
    auto peasant = std::ranges::find_if(pActors, [](const Actor &actor) {
        return isPeasant(actor.monsterInfo.id, GAME_VERSION_MM6) && actor.CanAct();
    });
    ASSERT_NE(peasant, pActors.end());
    Vec3f peasantPos = peasant->pos;
    Vec3f pos = peasantPos + Vec3f(-160, 0, 0);
    int yawDegrees = TrigLUT.atan2(peasantPos.x - pos.x, peasantPos.y - pos.y) * 90 / 512;
    game.teleportTo(engine->_currentLoadedMapId, pos, yawDegrees);
    game.tick(1);
    game.pressAndReleaseKey(PlatformKey::KEY_SPACE);
    game.tick(2);

    // The standard NPC dialogue opened on the generated citizen.
    ASSERT_EQ(current_screen_type, SCREEN_NPC_DIALOGUE);
    ASSERT_GE(speakingNpcId, 5000);
    NPCData *citizen = getNPCData(speakingNpcId);
    EXPECT_FALSE(citizen->name.empty());
    EXPECT_TRUE(std::ranges::contains(pNPCStats->pNPCNames[citizen->sex], citizen->name));
    EXPECT_GE(citizen->portraitId, 501);
    EXPECT_LE(citizen->portraitId, 554);
    EXPECT_NE(citizen->profession, NoProfession);
    EXPECT_GT(pNPCStats->pProfessions[citizen->profession].uHirePrice, 0u);
    EXPECT_TRUE(citizen->canJoin);

    // The citizen greets with a regional news line: New Sorpigal local news or a kingdom-wide rumor.
    auto *dialogue = static_cast<GUIWindow_Dialogue *>(pDialogueWindow.get());
    const std::string &greeting = dialogue->mm6NewsGreeting();
    EXPECT_FALSE(greeting.empty());
    const std::vector<RegionalNewsEntry> &localNews = pNPCStats->pRegionalNews[engine->_currentLoadedMapId];
    EXPECT_EQ(localNews.size(), 30u); // New Sorpigal's share of npcnews.txt.
    auto saysIt = [&](const RegionalNewsEntry &entry) { return entry.text == greeting; };
    EXPECT_TRUE(std::ranges::any_of(localNews, saysIt) || std::ranges::any_of(pNPCStats->pGeneralNews, saysIt));

    // Hire the citizen: click the hire topic (buttons are re-laid-out on draw - locate by msg_param).
    pParty->SetGold(5000); // Some professions cost up to 2000 to hire.
    const GUIButton *hireOption = nullptr;
    for (const GUIButton *button : pDialogueWindow->vButtons)
        if (button->msg == UIMSG_SelectNPCDialogueOption && button->msg_param == std::to_underlying(DIALOGUE_HIRE_FIRE))
            hireOption = button;
    ASSERT_NE(hireOption, nullptr);
    game.pressAndReleaseButton(BUTTON_LEFT, hireOption->rect.x + hireOption->rect.w / 2,
                               hireOption->rect.y + hireOption->rect.h / 2);
    game.tick(2);

    EXPECT_TRUE(citizen->Hired());
    // Burglars are the one profession hired for free (the hire handler skips the gold check for them).
    int expectedPrice = citizen->profession == Burglar ? 0 : pNPCStats->pProfessions[citizen->profession].uHirePrice;
    EXPECT_EQ(pParty->GetGold(), 5000 - expectedPrice);
    EXPECT_TRUE(pParty->pHirelings[0].name == citizen->name || pParty->pHirelings[1].name == citizen->name);
}

GAME_TEST(Mm6, QuestNpcDialogueInTavern) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // A Lonely Knight is New Sorpigal's tavern (2dEvents row 92). Andover Potbello (npcdata row 1)
    // lives there with two scripted dialogue topics wired to global.evt: event 1 "The Letter" and
    // event 296 "Quest" - the candelabra quest that MM6 opens with.
    EXPECT_EQ(houseTable[HouseId(92)].name, "A Lonely Knight");
    NPCData *andover = &pNPCStats->pNPCData[1];
    EXPECT_EQ(andover->name, "Andover Potbello");
    EXPECT_EQ(andover->house, HouseId(92));
    EXPECT_EQ(andover->dialogue_1_evt_id, 1u);
    EXPECT_EQ(andover->dialogue_2_evt_id, 296u);
    EXPECT_EQ(pNPCTopics[1].pTopic, "The Letter");
    EXPECT_EQ(pNPCTopics[296].pTopic, "Quest");

    // The tavern door face is wired to local event 11, an ungated SpeakInHouse(92).
    const BLVFace *door = nullptr;
    for (const BSPModel &model : pOutdoor->pBModels) {
        for (const BLVFace &face : model.faces) {
            if (face.eventId == 11 && face.Clickable()) {
                door = &face;
                break;
            }
        }
    }
    ASSERT_NE(door, nullptr);
    Vec3f doorCenter = door->boundingBox.center();
    Vec3f pos = doorCenter + door->facePlane.normal * 130;
    pos.z = door->boundingBox.z1;
    int yawDegrees = TrigLUT.atan2(doorCenter.x - pos.x, doorCenter.y - pos.y) * 90 / 512;
    game.teleportTo(engine->_currentLoadedMapId, pos, yawDegrees);
    game.tick(1);
    game.pressAndReleaseKey(PlatformKey::KEY_SPACE);
    game.tick(2);
    ASSERT_EQ(current_screen_type, SCREEN_HOUSE);
    ASSERT_NE(window_SpeakInHouse, nullptr);
    EXPECT_EQ(window_SpeakInHouse->houseId(), HouseId(92));

    // The tavern hosts the proprietor plus the house NPCs Andover and Maria; click Andover's portrait.
    ASSERT_GE(houseNpcs.size(), 3u);
    int andoverIndex = -1;
    for (int i = 0; i < houseNpcs.size(); i++)
        if (houseNpcs[i].type == HOUSE_NPC && houseNpcs[i].npc == andover)
            andoverIndex = i;
    ASSERT_NE(andoverIndex, -1);
    ASSERT_NE(houseNpcs[andoverIndex].button, nullptr);
    Recti portrait = houseNpcs[andoverIndex].button->rect;
    game.pressAndReleaseButton(BUTTON_LEFT, portrait.x + portrait.w / 2, portrait.y + portrait.h / 2);
    game.tick(2);

    // Both scripted topics became dialogue options. Their buttons are re-laid-out to the rendered
    // text metrics on draw, so locate the "Quest" option (DIALOGUE_SCRIPTED_LINE_2) by its message
    // param instead of assuming creation-time coordinates.
    ASSERT_NE(pDialogueWindow, nullptr);
    const GUIButton *questOption = nullptr;
    for (const GUIButton *button : pDialogueWindow->vButtons) {
        if (button->msg == UIMSG_SelectHouseNPCDialogueOption) {
            EXPECT_TRUE(button->msg_param == std::to_underlying(DIALOGUE_SCRIPTED_LINE_1) ||
                        button->msg_param == std::to_underlying(DIALOGUE_SCRIPTED_LINE_2));
            if (button->msg_param == std::to_underlying(DIALOGUE_SCRIPTED_LINE_2))
                questOption = button;
        }
    }
    ASSERT_NE(questOption, nullptr);

    // Clicking the "Quest" topic runs global event 296: quest bit 126 is granted, the topic rewires
    // itself to event 297 via SetNPCTopic, and the reply text (npctext.txt row 305) is displayed.
    EXPECT_FALSE(pParty->_questBits[static_cast<QuestBit>(126)]);
    game.pressAndReleaseButton(BUTTON_LEFT, questOption->rect.x + questOption->rect.w / 2,
                               questOption->rect.y + questOption->rect.h / 2);
    game.tick(2);
    EXPECT_TRUE(pParty->_questBits[static_cast<QuestBit>(126)]);
    EXPECT_EQ(andover->dialogue_2_evt_id, 297u);
    EXPECT_TRUE(current_npc_text.contains("Temple of Baa"));

    // Escape backs out to the portrait selection, a second escape leaves the tavern.
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_GAME);
    game.tick(5);
}

// Walks the party up to A Lonely Knight, New Sorpigal's tavern, and enters through its door
// (local event 11, an ungated SpeakInHouse(92)). The party must already be in New Sorpigal.
static void enterLonelyKnightTavern(EngineController &game) {
    const BLVFace *door = nullptr;
    for (const BSPModel &model : pOutdoor->pBModels) {
        for (const BLVFace &face : model.faces) {
            if (face.eventId == 11 && face.Clickable()) {
                door = &face;
                break;
            }
        }
    }
    ASSERT_NE(door, nullptr);
    Vec3f doorCenter = door->boundingBox.center();
    Vec3f pos = doorCenter + door->facePlane.normal * 130;
    pos.z = door->boundingBox.z1;
    int yawDegrees = TrigLUT.atan2(doorCenter.x - pos.x, doorCenter.y - pos.y) * 90 / 512;
    game.teleportTo(engine->_currentLoadedMapId, pos, yawDegrees);
    game.tick(1);
    game.pressAndReleaseKey(PlatformKey::KEY_SPACE);
    game.tick(2);
    ASSERT_EQ(current_screen_type, SCREEN_HOUSE);
}

// Opens the dialogue with a house NPC by clicking their portrait.
static void clickHouseNpcPortrait(EngineController &game, const NPCData *npc) {
    int npcIndex = -1;
    for (int i = 0; i < houseNpcs.size(); i++)
        if (houseNpcs[i].type == HOUSE_NPC && houseNpcs[i].npc == npc)
            npcIndex = i;
    ASSERT_NE(npcIndex, -1);
    ASSERT_NE(houseNpcs[npcIndex].button, nullptr);
    Recti portrait = houseNpcs[npcIndex].button->rect;
    game.pressAndReleaseButton(BUTTON_LEFT, portrait.x + portrait.w / 2, portrait.y + portrait.h / 2);
    game.tick(2);
}

// Finds the dialogue-option button for a scripted topic line. Option buttons are re-laid-out to
// rendered-text metrics on draw, so they are located by their message params, never by fixed
// coordinates. Returns nullptr when the NPC doesn't offer the topic.
static const GUIButton *findScriptedTopicButton(DialogueId topicLine) {
    if (!pDialogueWindow)
        return nullptr;
    for (const GUIButton *button : pDialogueWindow->vButtons)
        if (button->msg == UIMSG_SelectHouseNPCDialogueOption && button->msg_param == std::to_underlying(topicLine))
            return button;
    return nullptr;
}

// Clicks a scripted topic in an open NPC dialogue, running its global.evt script.
static void selectScriptedTopic(EngineController &game, DialogueId topicLine) {
    const GUIButton *option = findScriptedTopicButton(topicLine);
    ASSERT_NE(option, nullptr);
    game.pressAndReleaseButton(BUTTON_LEFT, option->rect.x + option->rect.w / 2,
                               option->rect.y + option->rect.h / 2);
    game.tick(2);
}

GAME_TEST(Mm6, CompleteLetterQuestDelivery) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // MM6's opening quest: deliver The Letter (item 505) to Andover Potbello in A Lonely Knight.
    // His "The Letter" topic runs global event 1: with the letter in the party's possession it pays
    // 1000 gold, clears quest bit 81, sets quest bit 82 and retires the topic to event 2; without
    // it, it shows a refusal and changes nothing.
    NPCData *andover = &pNPCStats->pNPCData[1];
    EXPECT_EQ(andover->name, "Andover Potbello");
    EXPECT_EQ(andover->dialogue_1_evt_id, 1u);
    EXPECT_EQ(pNPCTopics[1].pTopic, "The Letter");
    EXPECT_EQ(pItemTable->items[ItemId(505)].name, "The Letter");

    // A new MM6 party starts with quest bit 81 set and The Letter in Roderick's backpack
    // (Mm6.NewGameDefaults); take the letter away to exercise the refusal branch first.
    EXPECT_TRUE(pParty->_questBits[static_cast<QuestBit>(81)]);
    InventoryEntry letter = pParty->pCharacters[0].inventory.find(ItemId(505));
    ASSERT_TRUE(letter);
    pParty->pCharacters[0].inventory.take(letter);

    enterLonelyKnightTavern(game);

    // Without the letter the Compare(PlayerItemInHands, 505) branch falls through to the refusal
    // reply (npctext.txt row 3) and the quest state doesn't budge.
    int goldBefore = pParty->GetGold();
    clickHouseNpcPortrait(game, andover);
    selectScriptedTopic(game, DIALOGUE_SCRIPTED_LINE_1);
    EXPECT_TRUE(current_npc_text.contains("you don't have a letter"));
    EXPECT_EQ(pParty->GetGold(), goldBefore);
    EXPECT_EQ(andover->dialogue_1_evt_id, 1u);
    EXPECT_TRUE(pParty->_questBits[static_cast<QuestBit>(81)]);
    EXPECT_FALSE(pParty->_questBits[static_cast<QuestBit>(82)]);

    // Hand over the letter: back out to the portraits, put it in a backpack, ask again.
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    ASSERT_TRUE(pParty->pCharacters[0].inventory.add(Item(ItemId(505))));
    clickHouseNpcPortrait(game, andover);
    selectScriptedTopic(game, DIALOGUE_SCRIPTED_LINE_1);
    EXPECT_TRUE(current_npc_text.contains("The Seal"));
    EXPECT_EQ(pParty->GetGold(), goldBefore + 1000);
    EXPECT_FALSE(pParty->_questBits[static_cast<QuestBit>(81)]);
    EXPECT_TRUE(pParty->_questBits[static_cast<QuestBit>(82)]);
    EXPECT_EQ(andover->dialogue_1_evt_id, 2u); // Topic retired to event 2 via SetNPCTopic.
    EXPECT_TRUE(pParty->pCharacters[0].inventory.find(ItemId(505))); // The script doesn't take it.

    // Asking again hits the retired topic: a "you got your gold" brush-off, no second payout.
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    clickHouseNpcPortrait(game, andover);
    selectScriptedTopic(game, DIALOGUE_SCRIPTED_LINE_1);
    EXPECT_TRUE(current_npc_text.contains("You got your gold"));
    EXPECT_EQ(pParty->GetGold(), goldBefore + 1000);

    // Escape back to the portraits, then out of the tavern.
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_GAME);
    game.tick(5);
}

GAME_TEST(Mm6, CompleteCandelabraFetchQuest) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    MapId newSorpigal = engine->_currentLoadedMapId;
    Vec3f newSorpigalPos = pParty->pos;

    // The full fetch-quest loop: Andover Potbello's "Quest" topic (global event 296) asks the party
    // to recover the Candelabra (item 449) left behind in the old Temple of Baa - the Abandoned
    // Temple, d02.blv. Returning it to him (event 297) pays out and retires the whole topic.
    NPCData *andover = &pNPCStats->pNPCData[1];
    EXPECT_EQ(andover->dialogue_2_evt_id, 296u);
    EXPECT_EQ(pNPCTopics[296].pTopic, "Quest");
    EXPECT_EQ(pItemTable->items[ItemId(449)].name, "Candelabra");

    // Take the quest: quest bit 126 is granted and the topic chains to event 297.
    enterLonelyKnightTavern(game);
    clickHouseNpcPortrait(game, andover);
    selectScriptedTopic(game, DIALOGUE_SCRIPTED_LINE_2);
    EXPECT_TRUE(pParty->_questBits[static_cast<QuestBit>(126)]);
    EXPECT_EQ(andover->dialogue_2_evt_id, 297u);
    EXPECT_TRUE(current_npc_text.contains("candelabra"));

    // Asking again empty-handed hits event 297's Compare fall-through (npctext.txt row 306).
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    clickHouseNpcPortrait(game, andover);
    selectScriptedTopic(game, DIALOGUE_SCRIPTED_LINE_2);
    EXPECT_TRUE(current_npc_text.contains("Baa is patient"));
    EXPECT_TRUE(pParty->_questBits[static_cast<QuestBit>(126)]);

    // Leave the tavern.
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    ASSERT_EQ(current_screen_type, SCREEN_GAME);

    // Fetch: the candelabra sits in a chest in the Abandoned Temple.
    MapId temple = pMapStats->GetMapInfo("d02.blv");
    ASSERT_NE(temple, MAP_INVALID);
    game.teleportTo(temple, Vec3f(16406, -19669, 865), 0); // The temple's entrance.
    game.tick(1);
    int chestId = -1;
    for (int i = 0; i < vChests.size(); i++)
        if (vChests[i].inventory.find(ItemId(449)))
            chestId = i;
    ASSERT_NE(chestId, -1);

    // Find a side face of that chest so the party can stand in front of it (see
    // Mm6.OpenChestInGoblinwatch), and disarm it - the trap flow isn't what's under test here.
    const BLVFace *chestFace = nullptr;
    for (const BLVFace &face : pIndoor->faces) {
        if (!face.eventId || !face.Clickable() || !engine->_localEventMap.hasEvent(face.eventId))
            continue;
        if (std::abs(face.facePlane.normal.z) >= 0.5f)
            continue;
        for (const EvtInstruction &instruction : engine->_localEventMap.function(face.eventId)) {
            if (instruction.opcode == EVENT_OpenChest && instruction.data.chest_id == chestId) {
                chestFace = &face;
                break;
            }
        }
        if (chestFace)
            break;
    }
    ASSERT_NE(chestFace, nullptr);
    vChests[chestId].flags &= ~CHEST_TRAPPED;

    Vec3f chestCenter = chestFace->boundingBox.center();
    Vec3f standPos = chestCenter + chestFace->facePlane.normal * 130;
    standPos.z = chestFace->boundingBox.z1;
    int chestYawDegrees = TrigLUT.atan2(chestCenter.x - standPos.x, chestCenter.y - standPos.y) * 90 / 512;
    game.teleportTo(temple, standPos, chestYawDegrees);
    game.tick(1);
    game.pressAndReleaseKey(PlatformKey::KEY_SPACE);
    game.tick(1);
    ASSERT_EQ(current_screen_type, SCREEN_CHEST);

    // Spacebar grabs one chest item at a time into the active character's backpack; keep grabbing
    // until the candelabra comes out.
    auto partyHasCandelabra = [] {
        return std::ranges::any_of(pParty->pCharacters, [](const Character &character) {
            return static_cast<bool>(character.inventory.find(ItemId(449)));
        });
    };
    for (int i = 0; i < 30 && !partyHasCandelabra(); i++) {
        game.pressAndReleaseKey(PlatformKey::KEY_SPACE);
        game.tick(1);
    }
    EXPECT_TRUE(partyHasCandelabra());
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    ASSERT_EQ(current_screen_type, SCREEN_GAME);

    // Return to Andover and hand it over: event 297's success branch thanks the party
    // (npctext.txt row 307), takes the candelabra, and pays out - award 39 and 2000 experience
    // to everyone, 1000 gold, a 200-point reputation boost (MM6 reputation improves downwards)
    // and quest bit 126 back off. SetNPCTopic(npc 1, index 1, 0) then removes the topic for good.
    game.teleportTo(newSorpigal, newSorpigalPos, 0);
    game.tick(1);
    enterLonelyKnightTavern(game);
    int goldBefore = pParty->GetGold();
    std::vector<uint64_t> xpBefore;
    for (const Character &character : pParty->pCharacters)
        xpBefore.push_back(character.experience);
    int reputationBefore = currentLocationInfo().reputation;
    clickHouseNpcPortrait(game, andover);
    selectScriptedTopic(game, DIALOGUE_SCRIPTED_LINE_2);
    EXPECT_TRUE(current_npc_text.contains("Baa be praised"));
    EXPECT_EQ(pParty->GetGold(), goldBefore + 1000);
    for (int i = 0; i < pParty->pCharacters.size(); i++) {
        EXPECT_EQ(pParty->pCharacters[i].experience, xpBefore[i] + 2000);
        EXPECT_TRUE(pParty->pCharacters[i]._achievedAwardsBits[static_cast<AwardId>(39)]);
    }
    EXPECT_EQ(currentLocationInfo().reputation, reputationBefore - 200);
    EXPECT_FALSE(partyHasCandelabra());
    EXPECT_FALSE(pParty->_questBits[static_cast<QuestBit>(126)]);
    EXPECT_EQ(andover->dialogue_2_evt_id, 0u);

    // With the topic gone, reopening the dialogue offers no second quest line.
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    clickHouseNpcPortrait(game, andover);
    EXPECT_EQ(findScriptedTopicButton(DIALOGUE_SCRIPTED_LINE_2), nullptr);
    EXPECT_NE(findScriptedTopicButton(DIALOGUE_SCRIPTED_LINE_1), nullptr); // The Letter is still on offer.

    // Escape back to the portraits, then out of the tavern.
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_GAME);
    game.tick(5);
}

GAME_TEST(Mm6, UseSpellScroll) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    game.tick(1);

    // Item 200 is MM6's Torch Light scroll (items.txt binds it to spell 1 via Mod1 "S1").
    // Right-clicking a character portrait with the scroll held uses it on that character:
    // the scroll is consumed and the bound spell is cast, lighting the party torch.
    ASSERT_EQ(pItemTable->items[ItemId(200)].type, ITEM_TYPE_SPELL_SCROLL);
    EXPECT_FALSE(pParty->TorchlightActive());
    for (int i = 0; i < 100 && pParty->pCharacters[0].timeToRecovery != 0_ticks; i++)
        game.tick(1); // Using a scroll requires the target character to be recovered.
    ASSERT_EQ(pParty->pCharacters[0].timeToRecovery, 0_ticks);
    pParty->setHoldingItem(Item(ItemId(200)));
    game.pressAndReleaseButton(BUTTON_RIGHT, 50, 420); // Character 1's portrait.
    game.tick(5);
    EXPECT_EQ(pParty->pPickedItem.itemId, ITEM_NULL); // Scroll consumed.
    EXPECT_TRUE(pParty->TorchlightActive());
}

GAME_TEST(Mm6, LearnSpellFromBook) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    game.tick(1);

    // Item 300 is MM6's Torch Light spell book (Mod1 "S1"). Learning it requires the Fire skill;
    // the book is used on the character whose portrait is right-clicked while it's held.
    ASSERT_EQ(pItemTable->items[ItemId(300)].type, ITEM_TYPE_BOOK);
    Character &learner = pParty->pCharacters[0];
    learner.setSkillValue(SKILL_FIRE, CombinedSkillValue::novice());
    EXPECT_FALSE(learner.bHaveSpell[SPELL_FIRE_TORCH_LIGHT]);
    pParty->setHoldingItem(Item(ItemId(300)));
    game.pressAndReleaseButton(BUTTON_RIGHT, 50, 420);
    game.tick(5);
    EXPECT_TRUE(learner.bHaveSpell[SPELL_FIRE_TORCH_LIGHT]);
}

GAME_TEST(Mm6, ReadMessageScroll) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    game.tick(1);

    // Item 505 is "The Letter" from the Andover Potbello quest, an Mscroll whose text lives in
    // scroll.txt under the same item id. Right-clicking a portrait with it held opens the scroll
    // reading window with that text.
    ASSERT_EQ(pItemTable->items[ItemId(505)].type, ITEM_TYPE_MESSAGE_SCROLL);
    pParty->setHoldingItem(Item(ItemId(505)));
    // Like all right-click popups, the scroll shows only while the right button is held.
    game.pressButton(BUTTON_RIGHT, 50, 420);
    game.tick(2);
    ASSERT_NE(pGUIWindow_ScrollWindow, nullptr);
    EXPECT_EQ(pGUIWindow_ScrollWindow->scroll_type, ItemId(505));
    EXPECT_TRUE(pMessageScrolls[ItemId(505)].starts_with("My Dear Sulman"));

    game.releaseButton(BUTTON_RIGHT, 50, 420);
    game.tick(2);
    EXPECT_EQ(pGUIWindow_ScrollWindow, nullptr);
}

GAME_TEST(Mm6, ArtifactIdsAndTreasureRoll) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    game.tick(1);

    // MM6 artifacts & relics are items 400-429 - below the MM7-shaped [500, 528] window that
    // pIsArtifactFound used to span. A quest script granting one via SetVariable(ItemInHands)
    // must mark it as found without going out of bounds.
    ASSERT_EQ(pItemTable->items[ItemId(401)].rarity, RARITY_ARTIFACT); // Thor.
    pParty->pCharacters[0].SetVariable(VAR_PlayerItemInHands, 401);
    EXPECT_EQ(pParty->pPickedItem.itemId, ItemId(401));
    EXPECT_TRUE(pParty->pIsArtifactFound[ItemId(401)]);
    pParty->takeHoldingItem();

    // MM6 quest items sit inside MM7's artifact id range and must NOT be counted as artifacts.
    pParty->pCharacters[0].SetVariable(VAR_PlayerItemInHands, 505); // The Letter.
    EXPECT_FALSE(pParty->pIsArtifactFound[ItemId(505)]);
    pParty->takeHoldingItem();

    // The treasure-level-6 artifact roll must produce MM6 artifacts/relics - rolling MM7's id
    // range instead would spawn MM6 message scrolls and quest items as random loot.
    int artifactsRolled = 0;
    for (int i = 0; i < 400; i++) {
        Item item;
        pItemTable->generateItem(ITEM_TREASURE_LEVEL_6, RANDOM_ITEM_ANY, &item);
        ItemRarity rarity = pItemTable->items[item.itemId].rarity;
        if (rarity == RARITY_ARTIFACT || rarity == RARITY_RELIC) {
            EXPECT_GE(std::to_underlying(item.itemId), 400);
            EXPECT_LE(std::to_underlying(item.itemId), 429);
            artifactsRolled++;
        }
        EXPECT_NE(pItemTable->items[item.itemId].type, ITEM_TYPE_MESSAGE_SCROLL);
        EXPECT_NE(item.itemId, ItemId(505));
    }
    EXPECT_GT(artifactsRolled, 0); // ~5% of 400 rolls, capped by the artifact limit.
}

GAME_TEST(Mm6, DrinkPotions) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    game.tick(1);

    // MM6 potions are items 164-188, bound to content ids P1-P25 in items.txt. Unlike MM7 they
    // have no potion power: effects are fixed, temporary boosts go into the until-rest bonus
    // fields (statBonuses/sACModifier/sRes*Bonus/sLevelModifier), buff potions last 6 hours, and
    // black potions are permanent (essences once per stat per character).
    ASSERT_EQ(pItemTable->items[ItemId(164)].type, ITEM_TYPE_POTION);
    ASSERT_EQ(pItemTable->items[ItemId(164)].potionId, 1); // Cure Wounds.
    ASSERT_EQ(pItemTable->items[ItemId(188)].potionId, 25); // Rejuvenation.

    // Generated MM6 potions must not get MM7-style potion power.
    Item generated(ItemId(165));
    generated.postGenerate(ITEM_SOURCE_CHEST);
    EXPECT_EQ(generated.potionPower, 0);
    for (int i = 0; i < 20; i++) {
        Item rolled;
        pItemTable->generateItem(ITEM_TREASURE_LEVEL_3, RANDOM_ITEM_POTION, &rolled);
        ASSERT_TRUE(rolled.isPotion());
        EXPECT_EQ(rolled.potionPower, 0);
    }

    Character &knight = pParty->pCharacters[0]; // Character 1, portrait at (50,420).
    int casterIndex = -1; // Someone with spell points for the mana potions.
    for (int i = 0; i < pParty->pCharacters.size(); i++) {
        if (pParty->pCharacters[i].GetMaxMana() >= 10) {
            casterIndex = i;
            break;
        }
    }
    ASSERT_NE(casterIndex, -1);
    Character &caster = pParty->pCharacters[casterIndex];

    auto drink = [&](int itemId, int targetCharacter) {
        pParty->setHoldingItem(Item(ItemId(itemId)));
        pParty->pCharacters[0].useItem(targetCharacter, true);
        game.tick(1);
        // Drinking changes the held potion into an empty Potion Bottle (useitems.txt:
        // "Change Item to 163").
        EXPECT_EQ(pParty->pPickedItem.itemId, ItemId(163));
        pParty->takeHoldingItem();
    };

    // P1 Cure Wounds heals 10, via the real right-click-portrait path.
    for (int i = 0; i < 100 && knight.timeToRecovery != 0_ticks; i++)
        game.tick(1);
    ASSERT_GT(knight.health, 15);
    knight.health -= 15;
    int hpBefore = knight.health;
    pParty->setHoldingItem(Item(ItemId(164)));
    game.pressAndReleaseButton(BUTTON_RIGHT, 50, 420);
    game.tick(3);
    EXPECT_EQ(pParty->pPickedItem.itemId, ItemId(163)); // Left an empty bottle in hand.
    pParty->takeHoldingItem();
    EXPECT_EQ(knight.health, hpBefore + 10);

    // P2 Magic restores 10 spell points.
    caster.mana = 0;
    drink(165, casterIndex);
    EXPECT_EQ(caster.mana, std::min(10, caster.GetMaxMana()));

    // P3 Energy adds 10 to all seven stats until rest; P9 Extreme Energy stacks another 20.
    for (Attribute stat : knight._statBonuses.indices())
        ASSERT_EQ(knight._statBonuses[stat], 0);
    drink(166, 0);
    for (Attribute stat : knight._statBonuses.indices())
        EXPECT_EQ(knight._statBonuses[stat], 10);
    drink(172, 0);
    for (Attribute stat : knight._statBonuses.indices())
        EXPECT_EQ(knight._statBonuses[stat], 30);

    // P4 Protection adds 10 AC until rest; P7 Supreme Protection another 20.
    ASSERT_EQ(knight.sACModifier, 0);
    drink(167, 0);
    EXPECT_EQ(knight.sACModifier, 10);
    drink(170, 0);
    EXPECT_EQ(knight.sACModifier, 30);

    // P5 Resistance adds 10 to the five MM6 resistances until rest; P10 Super Resistance
    // another 20. MM6 Elec/Cold/Poison map to Air/Water/Earth, and the single MM6 "Magic"
    // resistance fans out to Mind/Spirit/Body, mirroring the monsters.txt column mapping.
    ASSERT_EQ(knight.sResFireBonus, 0);
    drink(168, 0);
    EXPECT_EQ(knight.sResFireBonus, 10);
    EXPECT_EQ(knight.sResAirBonus, 10);
    EXPECT_EQ(knight.sResWaterBonus, 10);
    EXPECT_EQ(knight.sResEarthBonus, 10);
    EXPECT_EQ(knight.sResMindBonus, 10);
    EXPECT_EQ(knight.sResSpiritBonus, 10);
    EXPECT_EQ(knight.sResBodyBonus, 10);
    EXPECT_EQ(knight.sResPhysicalBonus, 0); // Not a character resistance in MM6.
    EXPECT_EQ(knight.sResLightBonus, 0);
    EXPECT_EQ(knight.sResDarkBonus, 0);
    drink(173, 0);
    EXPECT_EQ(knight.sResFireBonus, 30);
    EXPECT_EQ(knight.sResBodyBonus, 30);

    // P6 Cure Poison clears all poison stages.
    knight.conditions.set(CONDITION_POISON_MEDIUM, pParty->GetPlayingTime());
    drink(169, 0);
    EXPECT_FALSE(knight.conditions.has(CONDITION_POISON_WEAK));
    EXPECT_FALSE(knight.conditions.has(CONDITION_POISON_MEDIUM));
    EXPECT_FALSE(knight.conditions.has(CONDITION_POISON_SEVERE));

    // P8 Restoration cures everything except dead, stone and eradicated.
    knight.conditions.set(CONDITION_WEAK, pParty->GetPlayingTime());
    knight.conditions.set(CONDITION_DISEASE_SEVERE, pParty->GetPlayingTime());
    knight.conditions.set(CONDITION_PARALYZED, pParty->GetPlayingTime());
    knight.conditions.set(CONDITION_DEAD, pParty->GetPlayingTime());
    drink(171, 0);
    EXPECT_FALSE(knight.conditions.has(CONDITION_WEAK));
    EXPECT_FALSE(knight.conditions.has(CONDITION_DISEASE_SEVERE));
    EXPECT_FALSE(knight.conditions.has(CONDITION_PARALYZED));
    EXPECT_TRUE(knight.conditions.has(CONDITION_DEAD));
    knight.conditions.reset(CONDITION_DEAD);

    // P11 Heroism, P12 Haste, P13 Stone Skin, P14 Bless: the spell effect for 6 hours.
    Time drinkStart = pParty->GetPlayingTime();
    drink(174, 0);
    drink(175, 0);
    drink(176, 0);
    drink(177, 0);
    Time drinkEnd = pParty->GetPlayingTime();
    for (CharacterBuff buff : {CHARACTER_BUFF_HEROISM, CHARACTER_BUFF_HASTE,
                               CHARACTER_BUFF_STONESKIN, CHARACTER_BUFF_BLESS}) {
        EXPECT_TRUE(knight.pCharacterBuffs[buff].Active());
        EXPECT_GE(knight.pCharacterBuffs[buff].expireTime, drinkStart + Duration::fromHours(6));
        EXPECT_LE(knight.pCharacterBuffs[buff].expireTime, drinkEnd + Duration::fromHours(6));
    }

    // P15 Divine Power adds 20 levels until rest and a year of magical age.
    ASSERT_EQ(knight.sLevelModifier, 0);
    int ageModifier = knight.sAgeModifier;
    drink(178, 0);
    EXPECT_EQ(knight.sLevelModifier, 20);
    EXPECT_EQ(knight.sAgeModifier, ageModifier + 1);

    // P16 Divine Cure restores 100 hit points and ages a year.
    knight.health = 1;
    drink(179, 0);
    EXPECT_EQ(knight.health, std::min(101, knight.GetMaxHealth()));
    EXPECT_EQ(knight.sAgeModifier, ageModifier + 2);

    // P17 Divine Magic restores 100 spell points and ages a year.
    caster.mana = 0;
    int casterAgeModifier = caster.sAgeModifier;
    drink(180, casterIndex);
    EXPECT_EQ(caster.mana, std::min(100, caster.GetMaxMana()));
    EXPECT_EQ(caster.sAgeModifier, casterAgeModifier + 1);

    // P18 Essence of Might: +15 might / -5 intellect, permanent, once per character.
    int mightBefore = knight._stats[ATTRIBUTE_MIGHT];
    int intellectBefore = knight._stats[ATTRIBUTE_INTELLIGENCE];
    drink(181, 0);
    EXPECT_EQ(knight._stats[ATTRIBUTE_MIGHT], mightBefore + 15);
    EXPECT_EQ(knight._stats[ATTRIBUTE_INTELLIGENCE], intellectBefore - 5);
    EXPECT_TRUE(knight._pureStatPotionUsed[ATTRIBUTE_MIGHT]);
    drink(181, 0); // A second one is drunk but has no further effect.
    EXPECT_EQ(knight._stats[ATTRIBUTE_MIGHT], mightBefore + 15);
    EXPECT_EQ(knight._stats[ATTRIBUTE_INTELLIGENCE], intellectBefore - 5);

    // P21 Essence of Endurance: +15 endurance / -1 everything else.
    IndexedArray<int, ATTRIBUTE_FIRST_STAT, ATTRIBUTE_LAST_STAT> statsBefore = knight._stats;
    drink(184, 0);
    for (Attribute stat : knight._stats.indices())
        EXPECT_EQ(knight._stats[stat], statsBefore[stat] + (stat == ATTRIBUTE_ENDURANCE ? 15 : -1));

    // P24 Essence of Luck: +15 luck / -5 accuracy.
    int luckBefore = knight._stats[ATTRIBUTE_LUCK];
    int accuracyBefore = knight._stats[ATTRIBUTE_ACCURACY];
    drink(187, 0);
    EXPECT_EQ(knight._stats[ATTRIBUTE_LUCK], luckBefore + 15);
    EXPECT_EQ(knight._stats[ATTRIBUTE_ACCURACY], accuracyBefore - 5);

    // P25 Rejuvenation wipes magical aging at the price of 1 point of every stat, permanently.
    ASSERT_GT(knight.sAgeModifier, 0);
    statsBefore = knight._stats;
    drink(188, 0);
    EXPECT_EQ(knight.sAgeModifier, 0);
    for (Attribute stat : knight._stats.indices())
        EXPECT_EQ(knight._stats[stat], statsBefore[stat] - 1);
}

GAME_TEST(Mm6, EatHerbs) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    game.tick(1);

    // MM6 herbs are edible reagents (useitems.txt): Poppysnaps (160) set weak poison, Phirna
    // Root (161) restores 2 spell points, Widoweeps Berries (162) heal 2 hit points, and the
    // herb is consumed. MM7's useItem carries a broken remnant of exactly this mechanic.
    ASSERT_EQ(pItemTable->items[ItemId(160)].type, ITEM_TYPE_REAGENT);
    ASSERT_EQ(pItemTable->items[ItemId(161)].type, ITEM_TYPE_REAGENT);
    ASSERT_EQ(pItemTable->items[ItemId(162)].type, ITEM_TYPE_REAGENT);

    Character &knight = pParty->pCharacters[0]; // Character 1, portrait at (50,420).
    for (int i = 0; i < 100 && knight.timeToRecovery != 0_ticks; i++)
        game.tick(1);

    // Widoweeps Berries heal 2, via the real right-click-portrait path.
    ASSERT_GT(knight.health, 5);
    knight.health -= 5;
    int hpBefore = knight.health;
    pParty->setHoldingItem(Item(ItemId(162)));
    game.pressAndReleaseButton(BUTTON_RIGHT, 50, 420);
    game.tick(3);
    EXPECT_EQ(pParty->pPickedItem.itemId, ITEM_NULL); // Eaten.
    EXPECT_EQ(knight.health, hpBefore + 2);

    // Phirna Root restores 2 spell points - eaten by a character who has any.
    int casterIndex = -1;
    for (int i = 0; i < pParty->pCharacters.size(); i++) {
        if (pParty->pCharacters[i].GetMaxMana() >= 2) {
            casterIndex = i;
            break;
        }
    }
    ASSERT_NE(casterIndex, -1);
    Character &caster = pParty->pCharacters[casterIndex];
    caster.mana = 0;
    pParty->setHoldingItem(Item(ItemId(161)));
    pParty->pCharacters[0].useItem(casterIndex, true);
    game.tick(1);
    EXPECT_EQ(pParty->pPickedItem.itemId, ITEM_NULL);
    EXPECT_EQ(caster.mana, 2);

    // Poppysnaps poison the eater.
    EXPECT_FALSE(knight.conditions.has(CONDITION_POISON_WEAK));
    pParty->setHoldingItem(Item(ItemId(160)));
    pParty->pCharacters[0].useItem(0, true);
    game.tick(1);
    EXPECT_EQ(pParty->pPickedItem.itemId, ITEM_NULL);
    EXPECT_TRUE(knight.conditions.has(CONDITION_POISON_WEAK));
}

GAME_TEST(Mm6, MixPotions) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    game.tick(1);

    // MM6 potion mixing is recipe-less: the full combination matrix ships in useitems.txt.
    // mm6PotionCombination returns ITEM_NULL when nothing happens, ItemId(1..4) for the E1..E4
    // explosion tiers (the same encoding MM7's potion.txt parse uses - MM7 inherited MM6's
    // explosion code verbatim), or the resulting item id.
    auto mix = [](int held, int target) {
        return std::to_underlying(mm6PotionCombination(ItemId(held), ItemId(target)));
    };

    // The matrix is symmetric.
    for (int a = 160; a <= 188; a++)
        for (int b = 160; b <= 188; b++)
            EXPECT_EQ(mix(a, b), mix(b, a)) << "a=" << a << " b=" << b;

    // Herbs fill an empty Potion Bottle (163) and combine with nothing else.
    EXPECT_EQ(mix(160, 163), 166); // Poppysnaps -> Energy.
    EXPECT_EQ(mix(161, 163), 165); // Phirna Root -> Magic.
    EXPECT_EQ(mix(162, 163), 164); // Widoweeps Berries -> Cure Wounds.
    EXPECT_EQ(mix(160, 161), 0);
    EXPECT_EQ(mix(160, 164), 0);
    EXPECT_EQ(mix(160, 178), 0);

    // The 22 potion recipes of the useitems.txt matrix.
    EXPECT_EQ(mix(164, 165), 169); // Cure Wounds + Magic = Cure Poison.
    EXPECT_EQ(mix(164, 166), 167); // Cure Wounds + Energy = Protection.
    EXPECT_EQ(mix(164, 167), 174); // Cure Wounds + Protection = Heroism.
    EXPECT_EQ(mix(164, 174), 181); // Cure Wounds + Heroism = Pure Might.
    EXPECT_EQ(mix(164, 175), 186); // Cure Wounds + Haste = Pure Speed.
    EXPECT_EQ(mix(165, 166), 168); // Magic + Energy = Resistance.
    EXPECT_EQ(mix(165, 167), 176); // Magic + Protection = Stone Skin.
    EXPECT_EQ(mix(165, 168), 173); // Magic + Resistance = Super Resistance.
    EXPECT_EQ(mix(165, 169), 177); // Magic + Cure Poison = Bless.
    EXPECT_EQ(mix(165, 171), 183); // Magic + Restoration = Pure Personality.
    EXPECT_EQ(mix(165, 176), 182); // Magic + Stone Skin = Pure Intellect.
    EXPECT_EQ(mix(166, 167), 172); // Energy + Protection = Extreme Energy.
    EXPECT_EQ(mix(166, 168), 175); // Energy + Resistance = Haste.
    EXPECT_EQ(mix(166, 170), 184); // Energy + Supreme Protection = Pure Endurance.
    EXPECT_EQ(mix(166, 177), 185); // Energy + Bless = Pure Accuracy.
    EXPECT_EQ(mix(167, 168), 170); // Protection + Resistance = Supreme Protection.
    EXPECT_EQ(mix(167, 171), 179); // Protection + Restoration = Divine Cure.
    EXPECT_EQ(mix(168, 169), 171); // Resistance + Cure Poison = Restoration.
    EXPECT_EQ(mix(168, 172), 188); // Resistance + Extreme Energy = Rejuvenation.
    EXPECT_EQ(mix(168, 173), 180); // Resistance + Super Resistance = Divine Magic.
    EXPECT_EQ(mix(169, 172), 178); // Cure Poison + Extreme Energy = Divine Power.
    EXPECT_EQ(mix(169, 173), 187); // Cure Poison + Super Resistance = Pure Luck.

    // Same potion, white+white, black+black and bottle+potion: nothing happens.
    EXPECT_EQ(mix(164, 164), 0);
    EXPECT_EQ(mix(170, 171), 0);
    EXPECT_EQ(mix(174, 175), 0);
    EXPECT_EQ(mix(178, 179), 0);
    EXPECT_EQ(mix(163, 163), 0);
    EXPECT_EQ(mix(163, 164), 0);
    EXPECT_EQ(mix(163, 188), 0);

    // Every other pair explodes, tier by category: colored+colored=E1, colored+white=E2,
    // colored+black=E3, white+black=E4.
    EXPECT_EQ(mix(164, 168), 1);
    EXPECT_EQ(mix(164, 169), 1);
    EXPECT_EQ(mix(166, 169), 1);
    EXPECT_EQ(mix(164, 170), 2);
    EXPECT_EQ(mix(169, 176), 2);
    EXPECT_EQ(mix(164, 178), 3);
    EXPECT_EQ(mix(169, 188), 3);
    EXPECT_EQ(mix(170, 178), 4);
    EXPECT_EQ(mix(177, 188), 4);

    // End-to-end through the inventory right-click path: Magic onto Cure Wounds makes Cure
    // Poison and returns a spare empty bottle to the inventory.
    game.pressAndReleaseKey(PlatformKey::KEY_I);
    game.tick(2);
    ASSERT_EQ(current_screen_type, SCREEN_CHARACTERS);
    Character &active = pParty->activeCharacter();
    InventoryEntry red = active.inventory.add(Item(ItemId(164)));
    ASSERT_TRUE(red);
    Pointi gridPos = red.geometry().topLeft();
    auto countBottles = [&] {
        int count = 0;
        for (InventoryEntry e : active.inventory.entries(ItemId(163)))
            count++;
        return count;
    };
    int bottlesBefore = countBottles();
    pParty->setHoldingItem(Item(ItemId(165)));
    // Right-click actions fire while the button is held (popup mode), so press, tick, release.
    game.pressButton(BUTTON_RIGHT, 14 + 32 * gridPos.x + 16, 17 + 32 * gridPos.y + 16);
    game.tick(2);
    game.releaseButton(BUTTON_RIGHT, 14 + 32 * gridPos.x + 16, 17 + 32 * gridPos.y + 16);
    game.tick(1);
    EXPECT_EQ(pParty->pPickedItem.itemId, ITEM_NULL);
    InventoryEntry mixed = active.inventory.entry(gridPos);
    ASSERT_TRUE(mixed);
    EXPECT_EQ(mixed->itemId, ItemId(169)); // Cure Poison.
    EXPECT_EQ(countBottles(), bottlesBefore + 1);

    // And the explosion path: Cure Wounds onto Resistance is E1 - 10-20 fire damage, both
    // potions destroyed, no bottle back.
    InventoryEntry green = active.inventory.add(Item(ItemId(168)));
    ASSERT_TRUE(green);
    Pointi greenPos = green.geometry().topLeft();
    int bottlesBeforeExplosion = countBottles();
    active.health = active.GetMaxHealth();
    int hpBefore = active.health;
    pParty->setHoldingItem(Item(ItemId(164)));
    game.pressButton(BUTTON_RIGHT, 14 + 32 * greenPos.x + 16, 17 + 32 * greenPos.y + 16);
    game.tick(2);
    game.releaseButton(BUTTON_RIGHT, 14 + 32 * greenPos.x + 16, 17 + 32 * greenPos.y + 16);
    game.tick(1);
    EXPECT_EQ(pParty->pPickedItem.itemId, ITEM_NULL);
    EXPECT_FALSE(active.inventory.entry(greenPos));
    EXPECT_LT(active.health, hpBefore);
    EXPECT_GE(active.health, hpBefore - 20);
    EXPECT_EQ(countBottles(), bottlesBeforeExplosion);
}

GAME_TEST(Mm6, ArtifactPowers) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    game.tick(1);

    // The MM6 default party starts with gear equipped (Mm6.NewGameDefaults); strip it so the
    // item bonus checks below see only the artifact under test.
    for (Character &character : pParty->pCharacters)
        for (ItemSlot slot : allItemSlots())
            if (InventoryEntry equipped = character.inventory.entry(slot))
                character.inventory.take(equipped);

    Character &knight = pParty->pCharacters[0];
    int casterIndex = -1; // Someone whose class has spell points, for the +N spell point powers.
    for (int i = 0; i < pParty->pCharacters.size(); i++) {
        if (pParty->pCharacters[i].GetMaxMana() >= 10) {
            casterIndex = i;
            break;
        }
    }
    ASSERT_NE(casterIndex, -1);
    Character &caster = pParty->pCharacters[casterIndex];

    // Equips an MM6 artifact into its natural slot (freeing the slot first), runs the checks,
    // and removes it again. The artifact bonus lookup iterates all equipped items, so freeing
    // the slot only matters for the equip() free-slot precondition, not for the bonuses.
    auto withEquipped = [&](Character &character, ItemSlot slot, int itemId, auto &&checks) {
        if (InventoryEntry existing = character.inventory.entry(slot))
            character.inventory.take(existing);
        InventoryEntry entry = character.inventory.equip(slot, Item(ItemId(itemId)));
        checks();
        character.inventory.take(entry);
    };

    // Excalibur: +30 Might.
    ASSERT_EQ(pItemTable->items[ItemId(403)].name, "Excalibur");
    ASSERT_EQ(pItemTable->items[ItemId(403)].rarity, RARITY_ARTIFACT);
    int mightBefore = knight.GetActualMight();
    withEquipped(knight, ITEM_SLOT_MAIN_HAND, 403, [&] {
        EXPECT_EQ(knight.GetActualMight(), mightBefore + 30);
    });
    EXPECT_EQ(knight.GetActualMight(), mightBefore); // Gone once unequipped.

    // Mordred is Vampiric only - no stat entries must have crept in.
    withEquipped(knight, ITEM_SLOT_MAIN_HAND, 400, [&] {
        EXPECT_EQ(knight.GetActualMight(), mightBefore);
    });

    // Arthur: 'of the Gods' (+10 to all seven stats) and +25 spell points.
    IndexedArray<int, ATTRIBUTE_FIRST_STAT, ATTRIBUTE_LAST_STAT> statsBefore;
    for (Attribute stat : statsBefore.indices())
        statsBefore[stat] = caster.GetActualStat(stat);
    int manaBefore = caster.GetMaxMana();
    withEquipped(caster, ITEM_SLOT_HELMET, 409, [&] {
        for (Attribute stat : statsBefore.indices())
            EXPECT_EQ(caster.GetActualStat(stat), statsBefore[stat] + 10);
        // +10 to the mana stats bumps their step-function parameter bonus too, so the max
        // spell points delta is at least the flat +25 the crown grants.
        EXPECT_GE(caster.GetMaxMana(), manaBefore + 25);
        EXPECT_EQ(caster.GetItemsBonus(ATTRIBUTE_MANA), 25);
    });

    // Galahad: 'of Protection' (+10 to the five MM6 resistances) and +25 hit points. MM6
    // Elec/Cold/Poison map onto Air/Water/Earth and "Magic" fans out to Mind/Spirit/Body,
    // like the potion and monsters.txt resistance mappings.
    const std::array<Attribute, 7> allResistances = {
        ATTRIBUTE_RESIST_FIRE, ATTRIBUTE_RESIST_AIR, ATTRIBUTE_RESIST_WATER,
        ATTRIBUTE_RESIST_EARTH, ATTRIBUTE_RESIST_MIND, ATTRIBUTE_RESIST_SPIRIT,
        ATTRIBUTE_RESIST_BODY};
    std::array<int, 7> resistancesBefore;
    for (size_t i = 0; i < allResistances.size(); i++)
        resistancesBefore[i] = knight.GetActualResistance(allResistances[i]);
    int healthBefore = knight.GetMaxHealth();
    withEquipped(knight, ITEM_SLOT_ARMOUR, 406, [&] {
        for (size_t i = 0; i < allResistances.size(); i++)
            EXPECT_EQ(knight.GetActualResistance(allResistances[i]), resistancesBefore[i] + 10);
        EXPECT_EQ(knight.GetMaxHealth(), healthBefore + 25);
    });

    // Odin: +50 to resistances at -40 Speed - relics carry downsides.
    ASSERT_EQ(pItemTable->items[ItemId(424)].rarity, RARITY_RELIC);
    int speedBefore = knight.GetActualSpeed();
    withEquipped(knight, ITEM_SLOT_HELMET, 424, [&] {
        for (size_t i = 0; i < allResistances.size(); i++)
            EXPECT_EQ(knight.GetActualResistance(allResistances[i]), resistancesBefore[i] + 50);
        EXPECT_EQ(knight.GetActualSpeed(), speedBefore - 40);
    });

    // Poseidon: +20 Might/Endurance/Accuracy, -10 AC and Speed. Armor class is checked via
    // the item bonus - actual AC also moves with the speed parameter bonus.
    int enduranceBefore = knight.GetActualEndurance();
    int accuracyBefore = knight.GetActualAccuracy();
    withEquipped(knight, ITEM_SLOT_MAIN_HAND, 417, [&] {
        EXPECT_EQ(knight.GetActualMight(), mightBefore + 20);
        EXPECT_EQ(knight.GetActualEndurance(), enduranceBefore + 20);
        EXPECT_EQ(knight.GetActualAccuracy(), accuracyBefore + 20);
        EXPECT_EQ(knight.GetItemsBonus(ATTRIBUTE_AC_BONUS), -10);
        EXPECT_EQ(knight.GetActualSpeed(), speedBefore - 10);
    });

    // Hera: +50 hit points, spell points and Luck for -50 Personality.
    int casterHealthBefore = caster.GetMaxHealth();
    int luckBefore = caster.GetActualLuck();
    int personalityBefore = caster.GetActualPersonality();
    withEquipped(caster, ITEM_SLOT_AMULET, 429, [&] {
        EXPECT_EQ(caster.GetMaxHealth(), casterHealthBefore + 50);
        EXPECT_EQ(caster.GetItemsBonus(ATTRIBUTE_MANA), 50);
        EXPECT_EQ(caster.GetActualLuck(), luckBefore + 50);
        EXPECT_EQ(caster.GetActualPersonality(), personalityBefore - 50);
    });

    // Guinevere: +30 spell points, 'of Light Magic' and 'of Dark Magic' - the school boosts
    // add half the character's skill level to the effective skill, like MM7's Ruler's Ring.
    caster.pActiveSkills[SKILL_LIGHT] = CombinedSkillValue(10, MASTERY_EXPERT);
    caster.pActiveSkills[SKILL_DARK] = CombinedSkillValue(7, MASTERY_EXPERT);
    withEquipped(caster, ITEM_SLOT_RING1, 412, [&] {
        EXPECT_EQ(caster.GetItemsBonus(ATTRIBUTE_MANA), 30);
        EXPECT_EQ(caster.GetItemsBonus(ATTRIBUTE_SKILL_LIGHT), 5);
        EXPECT_EQ(caster.GetItemsBonus(ATTRIBUTE_SKILL_DARK), 3);
        EXPECT_EQ(caster.GetItemsBonus(ATTRIBUTE_SKILL_FIRE), 0); // Only the two bound schools.
    });
    caster.pActiveSkills[SKILL_LIGHT] = CombinedSkillValue();
    caster.pActiveSkills[SKILL_DARK] = CombinedSkillValue();
}

GAME_TEST(Mm6, ArtifactBehavioralPowers) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    game.tick(1);

    Character &knight = pParty->pCharacters[0];

    auto withEquipped = [&](Character &character, ItemSlot slot, int itemId, auto &&checks) {
        if (InventoryEntry existing = character.inventory.entry(slot))
            character.inventory.take(existing);
        InventoryEntry entry = character.inventory.equip(slot, Item(ItemId(itemId)));
        checks();
        character.inventory.take(entry);
    };

    // Mordred (400) is Vampiric: hits drain the target's life instead of adding elemental
    // damage. Hades (415) drips acid: +20 poison damage (MM6 Poison maps onto Earth, like
    // the monsters.txt resistance columns). Ares (416) burns: +30 fire damage. Artemis (420)
    // fires charged bolts: +20 electricity damage (MM6 Elec maps onto Air).
    {
        DamageType damageType = DAMAGE_PHYSICAL;
        bool drainsHp = false;

        Item mordred(ItemId(400));
        EXPECT_EQ(mordred._439DF3_get_additional_damage(&damageType, &drainsHp), 0);
        EXPECT_TRUE(drainsHp);
        EXPECT_EQ(damageType, DAMAGE_DARK);

        Item hades(ItemId(415));
        EXPECT_EQ(hades._439DF3_get_additional_damage(&damageType, &drainsHp), 20);
        EXPECT_FALSE(drainsHp);
        EXPECT_EQ(damageType, DAMAGE_EARTH);

        Item ares(ItemId(416));
        EXPECT_EQ(ares._439DF3_get_additional_damage(&damageType, &drainsHp), 30);
        EXPECT_FALSE(drainsHp);
        EXPECT_EQ(damageType, DAMAGE_FIRE);

        Item artemis(ItemId(420));
        EXPECT_EQ(artemis._439DF3_get_additional_damage(&damageType, &drainsHp), 20);
        EXPECT_FALSE(drainsHp);
        EXPECT_EQ(damageType, DAMAGE_AIR);
    }

    // The monster supertypes behind the slaying powers follow MM6's monsters.txt rows, not
    // MM7's id ranges: demons ARE the devils (DemonFly 25-27, Demon 28-30, zDemonqueen 172),
    // dragons are DragonCave 31-33 / DragonLand 37-39 / DragonCover 40-42 - while DragonFly
    // 34-36 is an insect - and Ghost/Lich/Skeleton are the undead.
    EXPECT_EQ(supertypeForMonsterId(MonsterId(25), GAME_VERSION_MM6), MONSTER_SUPERTYPE_KREEGAN);
    EXPECT_EQ(supertypeForMonsterId(MonsterId(28), GAME_VERSION_MM6), MONSTER_SUPERTYPE_KREEGAN);
    EXPECT_EQ(supertypeForMonsterId(MonsterId(172), GAME_VERSION_MM6), MONSTER_SUPERTYPE_KREEGAN);
    EXPECT_EQ(supertypeForMonsterId(MonsterId(31), GAME_VERSION_MM6), MONSTER_SUPERTYPE_DRAGON);
    EXPECT_EQ(supertypeForMonsterId(MonsterId(37), GAME_VERSION_MM6), MONSTER_SUPERTYPE_DRAGON);
    EXPECT_EQ(supertypeForMonsterId(MonsterId(42), GAME_VERSION_MM6), MONSTER_SUPERTYPE_DRAGON);
    EXPECT_EQ(supertypeForMonsterId(MonsterId(34), GAME_VERSION_MM6), MONSTER_SUPERTYPE_NONE);
    EXPECT_EQ(supertypeForMonsterId(MonsterId(73), GAME_VERSION_MM6), MONSTER_SUPERTYPE_UNDEAD);
    EXPECT_EQ(supertypeForMonsterId(MonsterId(94), GAME_VERSION_MM6), MONSTER_SUPERTYPE_UNDEAD);
    EXPECT_EQ(supertypeForMonsterId(MonsterId(154), GAME_VERSION_MM6), MONSTER_SUPERTYPE_UNDEAD);
    EXPECT_EQ(supertypeForMonsterId(MonsterId(166), GAME_VERSION_MM6), MONSTER_SUPERTYPE_TITAN);
    EXPECT_EQ(supertypeForMonsterId(MonsterId(58), GAME_VERSION_MM6), MONSTER_SUPERTYPE_WATER_ELEMENTAL);
    EXPECT_EQ(supertypeForMonsterId(MonsterId(76), GAME_VERSION_MM6), MONSTER_SUPERTYPE_NONE);

    // Conan (402) slays devils and dragons: double damage. Conan is 3d7+10, so a single
    // roll is 13-31 and a doubled one 26-62; with 16 rolls at least one lands above the
    // single-roll maximum unless the power is missing.
    {
        Item conan(ItemId(402));
        ASSERT_EQ(pItemTable->items[ItemId(402)].name, "Conan");
        const ItemData &conanData = pItemTable->items[ItemId(402)];
        int minSingle = conanData.damageMod + conanData.damageDice;
        int maxSingle = conanData.damageMod + conanData.damageDice * conanData.damageRoll;
        bool exceededSingleMax = false;
        for (int i = 0; i < 16; i++) {
            int vsDragon = knight.CalculateMeleeDmgToEnemyWithWeapon(&conan, MonsterId(37), false);
            EXPECT_GE(vsDragon, 2 * minSingle);
            EXPECT_LE(vsDragon, 2 * maxSingle);
            exceededSingleMax = exceededSingleMax || vsDragon > maxSingle;
            int vsDevil = knight.CalculateMeleeDmgToEnemyWithWeapon(&conan, MonsterId(28), false);
            EXPECT_GE(vsDevil, 2 * minSingle);
            int vsGoblin = knight.CalculateMeleeDmgToEnemyWithWeapon(&conan, MonsterId(76), false);
            EXPECT_GE(vsGoblin, minSingle);
            EXPECT_LE(vsGoblin, maxSingle);
        }
        EXPECT_TRUE(exceededSingleMax);
    }

    // Swiftness: Merlin (404) and Percival (405) attack 20 ticks faster than a plain staff
    // (61) and bow (42).
    {
        Duration plainStaffRecovery, merlinRecovery;
        withEquipped(knight, ITEM_SLOT_MAIN_HAND, 61, [&] {
            plainStaffRecovery = knight.GetAttackRecoveryTime(false);
        });
        withEquipped(knight, ITEM_SLOT_MAIN_HAND, 404, [&] {
            merlinRecovery = knight.GetAttackRecoveryTime(false);
        });
        EXPECT_EQ(plainStaffRecovery - merlinRecovery, 20_ticks);

        Duration plainBowRecovery, percivalRecovery;
        withEquipped(knight, ITEM_SLOT_BOW, 42, [&] {
            plainBowRecovery = knight.GetAttackRecoveryTime(true);
        });
        withEquipped(knight, ITEM_SLOT_BOW, 405, [&] {
            percivalRecovery = knight.GetAttackRecoveryTime(true);
        });
        EXPECT_EQ(plainBowRecovery - percivalRecovery, 20_ticks);
    }

    // Force: a blow from Thor (401) knocks enemies back, via the same special-item bonus
    // MM7's 'of Force' enchantment feeds into the knockback code.
    EXPECT_EQ(knight.GetSpecialItemBonus(ITEM_ENCHANTMENT_OF_FORCE), 0);
    withEquipped(knight, ITEM_SLOT_MAIN_HAND, 401, [&] {
        EXPECT_EQ(knight.GetSpecialItemBonus(ITEM_ENCHANTMENT_OF_FORCE), 5);
    });

    // Hit Recovery: Pellinore (407) speeds up recovery like MM7's 'of Recovery'.
    EXPECT_EQ(knight.GetSpecialItemBonus(ITEM_ENCHANTMENT_OF_RECOVERY), 0);
    withEquipped(knight, ITEM_SLOT_ARMOUR, 407, [&] {
        EXPECT_EQ(knight.GetSpecialItemBonus(ITEM_ENCHANTMENT_OF_RECOVERY), 50);
    });

    // Shielding: Valeria (408) and Aegis (423) halve incoming missile damage.
    EXPECT_FALSE(knight.wearsShieldingItem());
    withEquipped(knight, ITEM_SLOT_OFF_HAND, 408, [&] {
        EXPECT_TRUE(knight.wearsShieldingItem());
    });
    withEquipped(knight, ITEM_SLOT_OFF_HAND, 423, [&] {
        EXPECT_TRUE(knight.wearsShieldingItem());
    });

    // Carnage: Percival's arrows explode in a fireball on impact; Artemis (420), the other
    // bow, doesn't carry the power.
    EXPECT_TRUE(Item(ItemId(405)).grantsCarnage());
    EXPECT_FALSE(Item(ItemId(420)).grantsCarnage());

    // Thievery: Pendragon (410) and Hades (415) boost trap disarming, like MM7's
    // 'of Thievery' (the exact MM6 number is unreversed - the MM7 multiplier bump is used).
    CombinedSkillValue disarmSkillBefore = knight.pActiveSkills[SKILL_TRAP_DISARM];
    knight.pActiveSkills[SKILL_TRAP_DISARM] = CombinedSkillValue(4, MASTERY_NOVICE);
    int plainDisarm = knight.GetDisarmTrap();
    EXPECT_EQ(plainDisarm, 4);
    withEquipped(knight, ITEM_SLOT_CLOAK, 410, [&] {
        EXPECT_EQ(knight.GetDisarmTrap(), plainDisarm + 4);
    });
    withEquipped(knight, ITEM_SLOT_MAIN_HAND, 415, [&] {
        EXPECT_EQ(knight.GetDisarmTrap(), plainDisarm + 4);
    });
    knight.pActiveSkills[SKILL_TRAP_DISARM] = disarmSkillBefore;

    // Immunity: Pendragon blocks all poison severities, Aegis blocks Flesh to Stone.
    knight.SetCondition(CONDITION_POISON_WEAK, 1);
    EXPECT_TRUE(knight.conditions.has(CONDITION_POISON_WEAK)); // Unprotected, the poison sticks.
    knight.conditions.reset(CONDITION_POISON_WEAK);
    withEquipped(knight, ITEM_SLOT_CLOAK, 410, [&] {
        for (Condition poison : {CONDITION_POISON_WEAK, CONDITION_POISON_MEDIUM, CONDITION_POISON_SEVERE}) {
            knight.SetCondition(poison, 1);
            EXPECT_FALSE(knight.conditions.has(poison));
        }
    });
    withEquipped(knight, ITEM_SLOT_OFF_HAND, 423, [&] {
        knight.SetCondition(CONDITION_PETRIFIED, 1);
        EXPECT_FALSE(knight.conditions.has(CONDITION_PETRIFIED));
    });

    // Hit Point Regeneration: Pellinore heals on the 5-minute regen tick; Hades draws its
    // power from its wielder - Negative Regeneration drains on the same tick.
    withEquipped(knight, ITEM_SLOT_ARMOUR, 407, [&] {
        knight.health = 1;
        pParty->last_regenerated = pParty->GetPlayingTime();
        pParty->playing_time += Duration::fromMinutes(11); // Two 5-minute regen ticks.
        RegeneratePartyHealthMana();
        EXPECT_EQ(knight.health, 3);
    });
    withEquipped(knight, ITEM_SLOT_MAIN_HAND, 415, [&] {
        int healthBefore = knight.health = knight.GetMaxHealth();
        pParty->last_regenerated = pParty->GetPlayingTime();
        pParty->playing_time += Duration::fromMinutes(11);
        RegeneratePartyHealthMana();
        EXPECT_EQ(knight.health, healthBefore - 2);
    });
    knight.health = knight.GetMaxHealth();
}

GAME_TEST(Mm6, MonsterModel) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    game.tick(1);

    // MM6 monsters.txt attack types: Elec/Cold/Pois map onto MM7's Air/Water/Earth like the
    // resistance columns do, Magic is the non-elemental DAMAGE_MAGIC, and Ener is a real energy
    // attack (the MM7 parser's Ener->Earth first-letter collision is an MM7-only preserved bug).
    EXPECT_EQ(pMonsterStats->infos[MonsterId(12)].attack1Type, DAMAGE_AIR);    // BeholderC, "Elec".
    EXPECT_EQ(pMonsterStats->infos[MonsterId(10)].attack1Type, DAMAGE_WATER);  // BeholderA, "Cold".
    EXPECT_EQ(pMonsterStats->infos[MonsterId(37)].attack1Type, DAMAGE_EARTH);  // DragonLandA, "Pois".
    EXPECT_EQ(pMonsterStats->infos[MonsterId(73)].attack1Type, DAMAGE_MAGIC);  // GhostA, "Magic".
    EXPECT_EQ(pMonsterStats->infos[MonsterId(36)].attack1Type, DAMAGE_ENERGY); // DragonFlyC, "Ener".

    // Ranged attackers keep their elemental bolt projectiles; Ghosts strike in melee. MM6-only
    // projectiles with no MM7 sprite (Magic/Rock/Dagger/FireAr) drop to NONE, mirroring the
    // ddm-embedded stats path.
    EXPECT_EQ(pMonsterStats->infos[MonsterId(12)].attack1MissileType, MONSTER_PROJECTILE_AIR_BOLT);
    EXPECT_EQ(pMonsterStats->infos[MonsterId(10)].attack1MissileType, MONSTER_PROJECTILE_WATER_BOLT);
    EXPECT_EQ(pMonsterStats->infos[MonsterId(37)].attack1MissileType, MONSTER_PROJECTILE_EARTH_BOLT);
    EXPECT_EQ(pMonsterStats->infos[MonsterId(36)].attack1MissileType, MONSTER_PROJECTILE_ENERGY_BOLT);
    EXPECT_EQ(pMonsterStats->infos[MonsterId(73)].attack1MissileType, MONSTER_PROJECTILE_NONE);

    // MM6 "Magic" damage is checked against Magic resistance, which this engine represents as
    // the Mind/Spirit/Body fan-out. A highly resistant target sees its damage halved at least
    // once in a handful of rolls; without the mapping the damage always lands in full (which is
    // MM7's correct Souldrinker behavior).
    Character &knight = pParty->pCharacters[0];
    int16_t mindResBefore = knight.sResMindBase;
    knight.sResMindBase = 500;
    bool characterResisted = false;
    for (int i = 0; i < 64 && !characterResisted; i++)
        characterResisted = knight.CalculateIncommingDamage(DAMAGE_MAGIC, 1000) < 1000;
    EXPECT_TRUE(characterResisted);
    knight.sResMindBase = mindResBefore;

    Actor resistantActor;
    resistantActor.monsterInfo.resMind = 100;
    bool actorResisted = false;
    for (int i = 0; i < 64 && !actorResisted; i++)
        actorResisted = resistantActor.CalcMagicalDamageToActor(DAMAGE_MAGIC, 1000) < 1000;
    EXPECT_TRUE(actorResisted);

    // MM6 peasants are the PeasantF*/PeasantM* rows 121-144. The MM7 id ranges would also
    // swallow everything from Oozes up to zReactor - killing a Titan must not read as a
    // peasant murder.
    EXPECT_TRUE(isPeasant(MonsterId(121), GAME_VERSION_MM6));  // PeasantF1A.
    EXPECT_TRUE(isPeasant(MonsterId(135), GAME_VERSION_MM6));  // PeasantM1C.
    EXPECT_TRUE(isPeasant(MonsterId(144), GAME_VERSION_MM6));  // PeasantM4C.
    EXPECT_FALSE(isPeasant(MonsterId(118), GAME_VERSION_MM6)); // Ogre.
    EXPECT_FALSE(isPeasant(MonsterId(147), GAME_VERSION_MM6)); // Giant Rat.
    EXPECT_FALSE(isPeasant(MonsterId(154), GAME_VERSION_MM6)); // Skeleton.
    EXPECT_FALSE(isPeasant(MonsterId(166), GAME_VERSION_MM6)); // Titan.
    EXPECT_FALSE(isPeasant(MonsterId(173), GAME_VERSION_MM6)); // zReactor.

    // Actor::IsPeasant goes through the hostility group and must agree.
    Actor titan;
    titan.monsterInfo.id = MonsterId(166);
    titan.hostilityGroup = monsterTypeForMonsterId(titan.monsterInfo.id);
    EXPECT_FALSE(titan.IsPeasant());

    Actor peasant;
    peasant.monsterInfo.id = MonsterId(123);
    peasant.hostilityGroup = monsterTypeForMonsterId(peasant.monsterInfo.id);
    EXPECT_TRUE(peasant.IsPeasant());
}

// Advances party time forward to the given 0-based month index (Might & Magic months are
// exactly 28 days), then runs a frame so the per-frame time update recomputes uCurrentMonth.
static void advanceToMonth(EngineController &game, int monthIndex) {
    int currentMonthIndex = pParty->GetPlayingTime().toCivilTime().month - 1;
    pParty->GetPlayingTime() += Duration::fromDays(28 * ((monthIndex - currentMonthIndex + 12) % 12));
    game.tick(1);
    EXPECT_EQ(pParty->uCurrentMonth, monthIndex);
}

GAME_TEST(Mm6, SnowFromMapEvents) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // MM6 snow is event-driven: outc1 (Frozen Highlands) has OnMapReload event 211 doing
    // SetSnow(0, 1), so it snows there permanently - even in summer. The MM7 every-third-
    // winter-day hack must not overwrite the event-set value in an MM6 session.
    advanceToMonth(game, 5); // June.
    MapId frozenHighlands = pMapStats->GetMapInfo("outc1.odm");
    ASSERT_NE(frozenHighlands, MAP_INVALID);
    game.teleportTo(frozenHighlands, Vec3f(0, 0, 0), 0);
    game.tick(5);
    EXPECT_TRUE(pWeather->bRenderSnow);

    // New Sorpigal has no SetSnow event - no snow, not even in deep winter.
    MapId newSorpigal = pMapStats->GetMapInfo("oute3.odm");
    ASSERT_NE(newSorpigal, MAP_INVALID);
    game.teleportTo(newSorpigal, Vec3f(-9728, -11319, 160), 0);
    game.tick(5);
    EXPECT_FALSE(pWeather->bRenderSnow);
    advanceToMonth(game, 0); // January.
    game.tick(5);
    EXPECT_FALSE(pWeather->bRenderSnow);
}

GAME_TEST(Mm6, SeasonsChangeTerrain) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    MapId newSorpigal = pMapStats->GetMapInfo("oute3.odm");
    MapId goblinwatch = pMapStats->GetMapInfo("d01.blv");
    ASSERT_NE(newSorpigal, MAP_INVALID);
    ASSERT_NE(goblinwatch, MAP_INVALID);

    // Reloads New Sorpigal (bouncing through Goblinwatch) so OutdoorLocation::Initialize
    // re-runs the seasonal tileset swap for the current month, then counts terrain tilesets.
    auto tilesetCountsAfterReload = [&](int monthIndex) {
        advanceToMonth(game, monthIndex);
        game.teleportTo(goblinwatch, Vec3f(-1850, 4304, -512), 0);
        game.teleportTo(newSorpigal, Vec3f(-9728, -11319, 160), 0);
        game.tick(1);
        std::map<Tileset, int> counts;
        for (int y = 0; y < 128; y++)
            for (int x = 0; x < 128; x++)
                counts[pTileTable->tile(pOutdoor->pTerrain.tileIdByGrid(Pointi(x, y))).tileset]++;
        return counts;
    };

    // Seasons originate in MM6: grass in summer, dirt in autumn, snow in winter.
    std::map<Tileset, int> summer = tilesetCountsAfterReload(5); // June.
    EXPECT_GT(summer[TILESET_GRASS], 0);
    EXPECT_EQ(summer[TILESET_SNOW], 0);

    std::map<Tileset, int> autumn = tilesetCountsAfterReload(9); // October.
    EXPECT_EQ(autumn[TILESET_GRASS], 0);
    EXPECT_GT(autumn[TILESET_DIRT], summer[TILESET_DIRT]);

    std::map<Tileset, int> winter = tilesetCountsAfterReload(0); // January.
    EXPECT_EQ(winter[TILESET_GRASS], 0);
    EXPECT_GT(winter[TILESET_SNOW], 0);
}

GAME_TEST(Mm6, SaveLoadRoundtrip) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    game.tick(1);

    // Party-state mutations that must survive the .mm6 save.
    pParty->SetGold(1234);
    pParty->_questBits[static_cast<QuestBit>(100)] = true;

    // Map-delta mutation on New Sorpigal: the first placed peasant of oute3.ddm dies.
    auto isFirstPeasant = [](const Actor &actor) {
        return std::to_underlying(actor.monsterId) == 123 && actor.initialPosition == Vec3f(-10296, -7528, 160);
    };
    auto peasant = std::ranges::find_if(pActors, isFirstPeasant);
    ASSERT_NE(peasant, pActors.end());
    peasant->aiState = Dead;
    peasant->hp = 0;

    // Leave through a REAL transition - the Abandoned Temple of Baa door (event 102) - because
    // that's the path that autosaves and thereby serializes oute3's delta into the save's
    // map-delta set. The teleportTo() test shortcut skips the autosave on purpose.
    const BLVFace *door = nullptr;
    for (const BSPModel &model : pOutdoor->pBModels) {
        for (const BLVFace &face : model.faces) {
            if (face.eventId == 102 && face.Clickable()) {
                door = &face;
                break;
            }
        }
    }
    ASSERT_NE(door, nullptr);
    Vec3f doorCenter = door->boundingBox.center();
    Vec3f doorPos = doorCenter + door->facePlane.normal * 130;
    doorPos.z = door->boundingBox.z1;
    int yawDegrees = TrigLUT.atan2(doorCenter.x - doorPos.x, doorCenter.y - doorPos.y) * 90 / 512;
    game.teleportTo(engine->_currentLoadedMapId, doorPos, yawDegrees); // Same-map teleport, no transition.
    game.tick(1);
    game.pressAndReleaseKey(PlatformKey::KEY_SPACE);
    game.tick(1);
    game.pressAndReleaseKey(PlatformKey::KEY_Y);
    game.tick(5);
    ASSERT_EQ(pMapStats->pInfos[engine->_currentLoadedMapId].fileName, "d02.blv");

    // Indoor map-delta mutation: chest 0 has been opened.
    ASSERT_FALSE(vChests.empty());
    EXPECT_FALSE(vChests[0].flags & CHEST_OPENED);
    vChests[0].flags |= CHEST_OPENED;

    Vec3f savedPos = pParty->pos;

    // Save, then load the save back.
    Blob save = game.saveGame();
    game.loadGame(save);
    game.tick(1);

    // We're back in the Abandoned Temple with the party state intact.
    EXPECT_EQ(pMapStats->pInfos[engine->_currentLoadedMapId].fileName, "d02.blv");
    EXPECT_EQ(uCurrentlyLoadedLevelType, LEVEL_INDOOR);
    EXPECT_NEAR(pParty->pos.x, savedPos.x, 1);
    EXPECT_NEAR(pParty->pos.y, savedPos.y, 1);
    EXPECT_NEAR(pParty->pos.z, savedPos.z, 1);
    EXPECT_EQ(pParty->GetGold(), 1234);
    EXPECT_TRUE(pParty->_questBits[static_cast<QuestBit>(100)]);
    EXPECT_TRUE(pParty->_questBits[static_cast<QuestBit>(81)]); // New-game quest bits are still there.
    EXPECT_TRUE(pParty->hasItem(static_cast<ItemId>(505))); // Roderick still carries The Letter.
    EXPECT_TRUE(vChests[0].flags & CHEST_OPENED); // The current map's delta came from the save.

    // Returning to New Sorpigal reloads its delta from the save rather than respawning the map:
    // the peasant is still dead.
    MapId newSorpigal = pMapStats->GetMapInfo("oute3.odm");
    ASSERT_NE(newSorpigal, MAP_INVALID);
    game.teleportTo(newSorpigal, Vec3f(-9728, -11319, 160), 0);
    game.tick(1);
    auto deadPeasant = std::ranges::find_if(pActors, isFirstPeasant);
    ASSERT_NE(deadPeasant, pActors.end());
    EXPECT_EQ(deadPeasant->aiState, Dead);
    EXPECT_EQ(deadPeasant->hp, 0);
}

GAME_TEST(Mm6, EndgameWinAndLose) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // The Hive's event 60 ends the game: step 11 is EnterHouse(600) = Win (party destroyed the
    // reactor with the Ritual of the Void in hand), step 5 is EnterHouse(601) = Lose (without
    // it, the blast consumes the world). Both show the endgame certificate screen.
    MapId hive = pMapStats->GetMapInfo("hive.blv");
    ASSERT_NE(hive, MAP_INVALID);
    game.teleportTo(hive, Vec3f(0, 0, 0), 0);
    ASSERT_FALSE(pIndoor->pSpawnPoints.empty());
    game.teleportTo(hive, pIndoor->pSpawnPoints[0].position, 0);
    game.tick(1);

    // Win: the certificate window opens and the game is NOT over - MM6 lets you play on.
    eventProcessor(60, Pid(), 1, 11);
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_GAMEOVER_WINDOW);
    EXPECT_EQ(uGameState, GAME_STATE_FINAL_WINDOW);
    ASSERT_TRUE(pGameOverWindow);
    game.pressAndReleaseButton(BUTTON_LEFT, 320, 240); // First click shows the credits popup...
    game.tick(2);
    game.pressAndReleaseButton(BUTTON_LEFT, 320, 240); // ...second click closes the window.
    game.tick(2);
    EXPECT_FALSE(pGameOverWindow);
    EXPECT_EQ(uGameState, GAME_STATE_PLAYING);
    EXPECT_EQ(current_screen_type, SCREEN_GAME);
    EXPECT_EQ(engine->_currentLoadedMapId, hive); // Still in the Hive, game continues.

    // Lose: certificate again, but closing it ends the session - back to the main menu.
    eventProcessor(60, Pid(), 1, 5);
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_GAMEOVER_WINDOW);
    ASSERT_TRUE(pGameOverWindow);
    game.pressAndReleaseButton(BUTTON_LEFT, 320, 240);
    game.tick(2);
    game.pressAndReleaseButton(BUTTON_LEFT, 320, 240);
    for (int i = 0; i < 50 && GetCurrentMenuID() != MENU_MAIN; i++)
        game.tick(1);
    EXPECT_EQ(GetCurrentMenuID(), MENU_MAIN);
}

GAME_TEST(Mm6, Mm7MapPinsDoNotMisfire) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    game.tick(1);

    // MM6 map ids collide with MM7's MapId enum, so MM7's hardcoded per-map special cases must
    // not fire in an MM6 session. New Sorpigal's map id is MM7's MAP_SHOALS - walking off the
    // east map edge must not offer MM7 foot travel to Avlee. (MM6 border travel is a separate,
    // not yet implemented model.)
    EXPECT_EQ(pOutdoor->getTravelDestination(100000, 0), MAP_INVALID);

    // Gharik's Forge has MM7's MAP_BREEDING_ZONE map id - its monsters must not have their
    // exp and loot zeroed by MM7's spawning-grounds rule.
    MapId gharik = pMapStats->GetMapInfo("d18.blv");
    ASSERT_NE(gharik, MAP_INVALID);
    game.teleportTo(gharik, Vec3f(0, 0, 0), 0);
    ASSERT_FALSE(pIndoor->pSpawnPoints.empty());
    game.teleportTo(gharik, pIndoor->pSpawnPoints[0].position, 0);
    game.tick(1);
    ASSERT_FALSE(pActors.empty());
    EXPECT_TRUE(std::ranges::any_of(pActors, [](const Actor &actor) { return actor.monsterInfo.exp > 0; }));
}

GAME_TEST(Mm6, DeathRespawnsInNewSorpigal) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    game.tick(1);

    // Die somewhere far from home - in Goblinwatch.
    MapId goblinwatch = pMapStats->GetMapInfo("d01.blv");
    ASSERT_NE(goblinwatch, MAP_INVALID);
    game.teleportTo(goblinwatch, Vec3f(0, 0, 0), 0);
    ASSERT_FALSE(pIndoor->pSpawnPoints.empty());
    game.teleportTo(goblinwatch, pIndoor->pSpawnPoints[0].position, 0);
    game.tick(1);
    for (Character &character : pParty->pCharacters)
        character.conditions.set(CONDITION_DEAD, pParty->GetPlayingTime());
    game.tick(10);

    // MM6 death respawn: back at the New Sorpigal new-game start pose, not MM7's
    // Harmondale/Emerald Isle logic (whose map ids are ordinary MM6 maps).
    EXPECT_EQ(uGameState, GAME_STATE_PLAYING);
    EXPECT_EQ(pParty->uNumDeaths, 1);
    EXPECT_EQ(pMapStats->pInfos[engine->_currentLoadedMapId].fileName, "oute3.odm");
    EXPECT_EQ(pParty->pos.x, -9728);
    EXPECT_EQ(pParty->pos.y, -11319);
}

GAME_TEST(Mm6, FootTravelAcrossBorders) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame(); // New Sorpigal, oute3 = grid cell E3.

    // MM6 border travel is grid arithmetic on the "out<column><row>.odm" file name: E3 connects
    // west to D3 (Castle Ironfist) and north to E2 (Misty Islands); east and south are off-grid.
    MapId ironfist = pMapStats->GetMapInfo("outd3.odm");
    MapId mist = pMapStats->GetMapInfo("oute2.odm");
    MapId bootlegBay = pMapStats->GetMapInfo("outd2.odm");
    ASSERT_NE(ironfist, MAP_INVALID);
    ASSERT_NE(mist, MAP_INVALID);
    ASSERT_NE(bootlegBay, MAP_INVALID);
    EXPECT_EQ(pOutdoor->getTravelDestination(-23000, 0), ironfist);
    EXPECT_EQ(pOutdoor->getTravelDestination(23000, 0), MAP_INVALID);
    EXPECT_EQ(pOutdoor->getTravelDestination(0, 23000), mist);
    EXPECT_EQ(pOutdoor->getTravelDestination(0, -23000), MAP_INVALID);
    // Both axes are checked independently, so corner crossings go diagonally: off the
    // northwest corner of E3 lies D2 (Bootleg Bay); the other three corners are off-grid.
    EXPECT_EQ(pOutdoor->getTravelDestination(-23000, 23000), bootlegBay);
    EXPECT_EQ(pOutdoor->getTravelDestination(23000, 23000), MAP_INVALID);
    EXPECT_EQ(pOutdoor->getTravelDestination(-23000, -23000), MAP_INVALID);
    EXPECT_EQ(pOutdoor->getTravelDestination(23000, -23000), MAP_INVALID);
    EXPECT_EQ(getTravelTime(), 5); // Walking always takes 5 days in MM6.

    // Find dry land on the west border - the travel prompt won't open over water.
    float borderY = 0.0f;
    float borderZ = 0.0f;
    bool found = false;
    for (int y = -20000; y <= 20000 && !found; y += 512) {
        bool isOnWater = false;
        int floorFaceId = -1;
        float z = ODM_GetFloorLevel(Vec3f(-22400, y, 3000), &isOnWater, &floorFaceId);
        if (!isOnWater) {
            borderY = y;
            borderZ = z;
            found = true;
        }
    }
    ASSERT_TRUE(found);

    game.teleportTo(engine->_currentLoadedMapId, Vec3f(-22400, borderY, borderZ), 270);
    game.tick(3);

    // Step across the border - the on-foot travel prompt opens, Y confirms.
    Time timeBefore = pParty->GetPlayingTime();
    pParty->pos.x = -22700;
    game.tick(3);
    ASSERT_EQ(current_screen_type, SCREEN_CHANGE_LOCATION);
    game.pressAndReleaseKey(PlatformKey::KEY_Y);
    game.tick(10);

    // 5 days later the party is at the opposite (east) border of Castle Ironfist,
    // north-south position preserved.
    EXPECT_EQ(pMapStats->pInfos[engine->_currentLoadedMapId].fileName, "outd3.odm");
    EXPECT_EQ(uCurrentlyLoadedLevelType, LEVEL_OUTDOOR);
    EXPECT_GT(pParty->pos.x, 20000);
    EXPECT_NEAR(pParty->pos.y, borderY, 1500);
    EXPECT_EQ((pParty->GetPlayingTime() - timeBefore).days(), 5);
    game.tick(10);
}

// Walks up to the outdoor door face wired to the given local event and opens it with SPACE.
static void enterHouseThroughDoor(EngineController &game, int eventId) {
    const BLVFace *door = nullptr;
    for (const BSPModel &model : pOutdoor->pBModels) {
        for (const BLVFace &face : model.faces) {
            if (face.eventId == eventId && face.Clickable()) {
                door = &face;
                break;
            }
        }
    }
    ASSERT_NE(door, nullptr);
    Vec3f doorCenter = door->boundingBox.center();
    Vec3f pos = doorCenter + door->facePlane.normal * 130;
    pos.z = door->boundingBox.z1;
    int yawDegrees = TrigLUT.atan2(doorCenter.x - pos.x, doorCenter.y - pos.y) * 90 / 512;
    game.teleportTo(engine->_currentLoadedMapId, pos, yawDegrees);
    game.tick(1);
    game.pressAndReleaseKey(PlatformKey::KEY_SPACE);
    game.tick(2);
    ASSERT_EQ(current_screen_type, SCREEN_HOUSE);
}

// Advances whole days until the given transport house has an active route today.
static void advanceToTravelDay(EngineController &game, HouseId houseId) {
    for (int i = 0; i < 8 && !isTravelAvailable(houseId); i++) {
        pParty->GetPlayingTime() += Duration::fromDays(1);
        game.tick(1);
    }
    ASSERT_TRUE(isTravelAvailable(houseId));
}

// Clicks the Nth transport schedule line in an open stables/dock dialogue. The option buttons
// are re-laid-out to rendered-text metrics on draw, so locate them by message param.
static void selectTransportSchedule(EngineController &game, DialogueId scheduleLine) {
    ASSERT_NE(pDialogueWindow, nullptr);
    const GUIButton *option = nullptr;
    for (const GUIButton *button : pDialogueWindow->vButtons)
        if (button->msg == UIMSG_SelectProprietorDialogueOption && button->msg_param == std::to_underlying(scheduleLine))
            option = button;
    ASSERT_NE(option, nullptr);
    game.pressAndReleaseButton(BUTTON_LEFT, option->rect.x + option->rect.w / 2,
                               option->rect.y + option->rect.h / 2);
    game.tick(2);
}

GAME_TEST(Mm6, TravelByCoachAndBoat) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    pParty->SetGold(2000);

    // New Sorpigal's transport houses from 2dEvents: 48 = stables, 57 = dock.
    ASSERT_EQ(houseTable[HouseId(48)].uType, HOUSE_TYPE_STABLE);
    ASSERT_EQ(houseTable[HouseId(57)].uType, HOUSE_TYPE_BOAT);

    // Ride the coach to Castle Ironfist (schedule entry 0: Mon/Wed/Fri, 2 days).
    advanceToTravelDay(game, HouseId(48));
    enterHouseThroughDoor(game, 15); // oute3 event 15 = EnterHouse(48), the stables door.
    ASSERT_NE(window_SpeakInHouse, nullptr);
    ASSERT_EQ(window_SpeakInHouse->houseId(), HouseId(48));

    int goldBefore = pParty->GetGold();
    Time timeBefore = pParty->GetPlayingTime();
    selectTransportSchedule(game, DIALOGUE_TRANSPORT_SCHEDULE_1);
    game.tick(10);

    EXPECT_EQ(pMapStats->pInfos[engine->_currentLoadedMapId].fileName, "outd3.odm"); // Castle Ironfist.
    EXPECT_EQ(current_screen_type, SCREEN_GAME);
    EXPECT_NEAR(pParty->pos.x, 14317, 8); // MM6.EXE TravelInfo[0] arrival pose.
    EXPECT_NEAR(pParty->pos.y, 2696, 8);
    EXPECT_LT(pParty->GetGold(), goldBefore);
    EXPECT_EQ((pParty->GetPlayingTime() - timeBefore).days(), 2);

    // Back to New Sorpigal and sail to the Misty Islands (schedule entry 15: Tue/Thu/Sat, 3 days).
    // The dock's EnterHouse(57) event (oute3 event 29) isn't wired to a clickable building face,
    // so enter the house directly through the same path EVENT_SpeakInHouse takes.
    game.teleportTo(pMapStats->GetMapInfo("oute3.odm"), Vec3f(-9728, -11319, 160), 0);
    game.tick(2);
    advanceToTravelDay(game, HouseId(57));
    ASSERT_TRUE(enterHouse(HouseId(57)));
    createHouseUI(HouseId(57));
    game.tick(2);
    ASSERT_EQ(current_screen_type, SCREEN_HOUSE);
    ASSERT_NE(window_SpeakInHouse, nullptr);
    ASSERT_EQ(window_SpeakInHouse->houseId(), HouseId(57));

    goldBefore = pParty->GetGold();
    timeBefore = pParty->GetPlayingTime();
    selectTransportSchedule(game, DIALOGUE_TRANSPORT_SCHEDULE_1);
    game.tick(10);

    EXPECT_EQ(pMapStats->pInfos[engine->_currentLoadedMapId].fileName, "oute2.odm"); // Misty Islands.
    EXPECT_NEAR(pParty->pos.x, -4225, 8); // MM6.EXE TravelInfo[15] arrival pose.
    EXPECT_NEAR(pParty->pos.y, -14604, 8);
    EXPECT_LT(pParty->GetGold(), goldBefore);
    EXPECT_EQ((pParty->GetPlayingTime() - timeBefore).days(), 3);
    game.tick(10);
}

GAME_TEST(Mm6, TownPortal) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // Cast Town Portal (all six MM6 towns are open - MM6 has no unlock quest bits) and
    // click Free Haven on MM6's own map image.
    engine->config->debug.AllMagic.setValue(true);
    game.castSpell(1, SPELL_WATER_TOWN_PORTAL);
    game.tick(2);
    game.pressGuiButton("TownPortalBook_Marker1"); // Free Haven.
    game.tick(2);
    game.skipLoadingScreen();
    game.tick(10);

    EXPECT_EQ(pMapStats->pInfos[engine->_currentLoadedMapId].fileName, "outc2.odm"); // Free Haven.
    EXPECT_NEAR(pParty->pos.x, 6991, 8); // MM6.EXE TownPortalInfo[1] fountain pose.
    EXPECT_NEAR(pParty->pos.y, 13438, 8);
    game.tick(10);
}

GAME_TEST(Mm6, LloydBeaconSaveRoundtrip) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // Beacons serialize their map as a games.lod file index (MM6: 1-based over the sorted map
    // files); both an outdoor and an indoor map id must survive a save/load roundtrip.
    MapId goblinwatch = pMapStats->GetMapInfo("d01.blv");
    MapId newSorpigal = pMapStats->GetMapInfo("oute3.odm");
    ASSERT_NE(goblinwatch, MAP_INVALID);

    LloydBeacon outdoorBeacon;
    outdoorBeacon.uBeaconTime = pParty->GetPlayingTime() + Duration::fromDays(7);
    outdoorBeacon._partyPos = Vec3f(-9728, -11319, 160);
    outdoorBeacon._partyViewYaw = 512;
    outdoorBeacon.mapId = newSorpigal;
    outdoorBeacon.image = GraphicsImage::Create(render->MakeViewportScreenshot(92, 68));
    pParty->pCharacters[0].vBeacons[0] = outdoorBeacon;

    LloydBeacon indoorBeacon = outdoorBeacon;
    indoorBeacon.mapId = goblinwatch;
    pParty->pCharacters[0].vBeacons[4] = indoorBeacon;

    Blob save = game.saveGame();
    game.loadGame(save);
    game.tick(2);

    ASSERT_TRUE(pParty->pCharacters[0].vBeacons[0].has_value());
    EXPECT_EQ(pParty->pCharacters[0].vBeacons[0]->mapId, newSorpigal);
    EXPECT_EQ(pParty->pCharacters[0].vBeacons[0]->_partyPos, Vec3f(-9728, -11319, 160));
    ASSERT_TRUE(pParty->pCharacters[0].vBeacons[4].has_value());
    EXPECT_EQ(pParty->pCharacters[0].vBeacons[4]->mapId, goblinwatch);
}

GAME_TEST(Mm6, DevLeftoverOpcodesAreNoOps) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // MM6.EXE's event dispatch table routes opcodes 20/27/28 (ModifyItem / RndPassword / RndAnswer -
    // dev leftovers, never implemented) straight to the step-advance path, so they are no-ops in the
    // shipped game. Snergle's Iron Mines (d09.blv) has RndPassword at step 0 of switch event 21 whose
    // step 1 opens a door: execution must pass THROUGH the no-op and drive the door.
    MapId snergleMines = pMapStats->GetMapInfo("d09.blv");
    ASSERT_NE(snergleMines, MAP_INVALID);
    game.teleportTo(snergleMines, Vec3f(0, 0, 0), 0);
    ASSERT_FALSE(pIndoor->pSpawnPoints.empty());
    game.teleportTo(snergleMines, pIndoor->pSpawnPoints[0].position, 0);
    game.tick(1);

    ASSERT_TRUE(engine->_localEventMap.hasEvent(21));
    const std::vector<EvtInstruction> &script = engine->_localEventMap.function(21);
    auto rndPassword = std::ranges::find_if(script, [](const EvtInstruction &ir) { return ir.opcode == EVENT_RandomPassword; });
    ASSERT_NE(rndPassword, script.end());
    auto doorStep = std::ranges::find_if(script, [](const EvtInstruction &ir) { return ir.opcode == EVENT_ChangeDoorState; });
    ASSERT_NE(doorStep, script.end());

    BLVDoor *door = nullptr;
    for (BLVDoor &candidate : pIndoor->doors) {
        if (candidate.doorId == static_cast<uint32_t>(doorStep->data.door_descr.door_id)) {
            door = &candidate;
            break;
        }
    }
    ASSERT_NE(door, nullptr);
    DoorState stateBefore = door->state;

    eventProcessor(21, Pid(), 1, 0);
    game.tick(2);
    EXPECT_NE(door->state, stateBefore); // The no-op step didn't halt the script.

    // Event 39 is a bookshelf whose only non-marker step is a RndAnswer - firing it must be a
    // clean no-op ("You thumb through the books, but find nothing of interest.").
    ASSERT_TRUE(engine->_localEventMap.hasEvent(39));
    eventProcessor(39, Pid(), 1, 0);
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_GAME);
}

GAME_TEST(Mm6, OverlaysRenderAndExpire) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    game.tick(1);

    // MM6's doverlay.bin binds real sprite framesets (MM7 has the same 96 overlay ids, all pointing
    // at the "null" sprite). Spot-check an impact spark, a portrait buff fx and two status icons.
    auto descById = [](int overlayId) -> const OverlayDesc * {
        for (const OverlayDesc &desc : pOverlayList->pOverlays)
            if (desc.uOverlayID == overlayId)
                return &desc;
        return nullptr;
    };
    EXPECT_EQ(pOverlayList->pOverlays.size(), 96);
    for (int overlayId : {904, 10000, 10009, 10015}) {
        const OverlayDesc *desc = descById(overlayId);
        ASSERT_NE(desc, nullptr);
        ASSERT_NE(desc->uSpriteFramesetID, 0);
        SpriteFrame *frame = pSpriteFrameTable->GetFrame(desc->uSpriteFramesetID, 0_ticks);
        ASSERT_NE(frame, nullptr);
        EXPECT_NE(frame->spriteName, "null");
        ASSERT_NE(frame->sprites[0], nullptr);
    }

    // An elemental hit spawns a one-shot spark overlay attached to the actor; the per-frame update
    // then animates it in the 3D view and frees the slot when the animation ends.
    pActiveOverlayList->Reset();
    ASSERT_FALSE(pActors.empty());
    Actor::AddOnDamageOverlay(0, 1, 100); // Fire damage spark, overlay 904.
    ActiveOverlay &spark = pActiveOverlayList->pOverlays[0];
    EXPECT_EQ(spark.pid, Pid(OBJECT_Actor, 0));
    EXPECT_GT(spark.animLength, 0);
    EXPECT_GT(spark.fpDamageMod, 0);
    EXPECT_EQ(spark.spriteFrameTime, 0);
    game.tick(2);
    EXPECT_GT(spark.spriteFrameTime, 0); // The draw loop ran and advanced the animation.
    for (int i = 0; i < 100 && spark.animLength > 0; i++)
        game.tick(1);
    EXPECT_LE(spark.animLength, 0); // One-shot expired and freed its slot.
    EXPECT_EQ(spark.pid, Pid());

    // Screen-anchored buff fx: anchored over character 0's portrait, deduplicated per anchor, kept
    // alive past its animation length until the owning SpellBuff resets the slot.
    int slotIndex = pActiveOverlayList->addScreenOverlay(10000, 310, 0_ticks, 65536);
    ASSERT_GT(slotIndex, 0);
    ActiveOverlay &buffFx = pActiveOverlayList->pOverlays[slotIndex - 1];
    EXPECT_EQ(buffFx.target, 310);
    EXPECT_EQ(buffFx.screenSpaceX, 19);
    EXPECT_EQ(buffFx.screenSpaceY, 456);
    EXPECT_TRUE(buffFx.flags & OVERLAY_FLAG_BUFF_OWNED);
    EXPECT_EQ(pActiveOverlayList->addScreenOverlay(10000, 310, 0_ticks, 65536), slotIndex);
    game.tick(5);
    EXPECT_GT(buffFx.animLength, 0); // Not expired by the update loop.
    SpellBuff buff;
    buff.Apply(pParty->GetPlayingTime() + Duration::fromHours(1), MASTERY_NOVICE, 5, slotIndex, 0);
    buff.Reset();
    EXPECT_LE(buffFx.animLength, 0); // Freed together with the buff.

    // Party-buff status icons (the y=254 row) draw while the buff is active - smoke-test the draw
    // pass headless.
    pParty->pPartyBuffs[PARTY_BUFF_WIZARD_EYE].Apply(pParty->GetPlayingTime() + Duration::fromHours(1), MASTERY_NOVICE, 5, 0, 0);
    game.tick(3);
    pParty->pPartyBuffs[PARTY_BUFF_WIZARD_EYE].Reset();
}

GAME_TEST(Mm6, SpellCastFx) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    engine->config->debug.AllMagic.setValue(true); // Cast lands every time - no mana/skill/mastery gating.
    game.tick(1);

    // Count active one-shot cast-fx overlays sitting on a character-portrait anchor (target 100..103) that
    // resolve to a given doverlay overlay id.
    auto portraitFxFor = [](int overlayId) {
        int count = 0;
        for (const ActiveOverlay &slot : pActiveOverlayList->pOverlays) {
            if (slot.animLength <= 0 || slot.target < 100 || slot.target > 103)
                continue;
            if (slot.indexToOverlayList < 0 || slot.indexToOverlayList >= static_cast<int>(pOverlayList->pOverlays.size()))
                continue;
            if (pOverlayList->pOverlays[slot.indexToOverlayList].uOverlayID == overlayId)
                count++;
        }
        return count;
    };

    // MM6 spell id 12 is Wizard Eye, a party-wide utility buff. Casting it flashes overlay 2000 over the
    // portraits (MM6.EXE CastSpell dispatch 0x422C93). It applies a party buff without running through the
    // MM7 SetPlayerBuffAnim path, so its cast fx must come from the shared cast tail, not the buff-anim hook.
    pActiveOverlayList->Reset();
    EXPECT_EQ(portraitFxFor(2000), 0); // Fail-first anchor: nothing spawned before the cast.
    pushSpellOrRangedAttack(static_cast<SpellId>(12), 0, CombinedSkillValue::none(), 0, 1);
    game.tick(1);
    // A party-target spell loops the fx over all four portraits (anchors 100..103).
    EXPECT_EQ(portraitFxFor(2000), 4);
    // The cast fx is a one-shot (not a buff-owned persistent overlay), so it expires on its own.
    for (const ActiveOverlay &slot : pActiveOverlayList->pOverlays)
        if (slot.animLength > 0 && slot.target >= 100 && slot.target <= 103)
            EXPECT_FALSE(slot.flags & OVERLAY_FLAG_BUFF_OWNED);

    // A character-targeted spell flashes over only the targeted portrait. MM6 spell id 68 is First Aid,
    // whose effect (Heal) targets a single character; drive it through the target-picking path onto
    // character 2 and check the fx lands on anchor 102 alone.
    pActiveOverlayList->Reset();
    pushSpellOrRangedAttack(static_cast<SpellId>(68), 0, CombinedSkillValue::none(), 0, 0);
    spellTargetPicked(Pid(), 2);
    game.tick(1);
    EXPECT_EQ(portraitFxFor(7010), 1); // Only the targeted portrait.
    bool onChar2 = false;
    for (const ActiveOverlay &slot : pActiveOverlayList->pOverlays)
        if (slot.animLength > 0 && slot.target == 102 &&
                pOverlayList->pOverlays[slot.indexToOverlayList].uOverlayID == 7010)
            onChar2 = true;
    EXPECT_TRUE(onChar2);

    // The MM6-unique spells (Create Food, the stat-buff family, Day of the Gods, ...) run through
    // castMm6UniqueSpell, which applies the cast tail itself - their fx must spawn there too. Day of the
    // Gods (MM6 id 83, overlay 8050) is always party-wide. Casting it right after the picked-target First
    // Aid above also proves a reused cast-queue slot doesn't leak the old target onto a party cast.
    pActiveOverlayList->Reset();
    pushSpellOrRangedAttack(static_cast<SpellId>(83), 0, CombinedSkillValue::none(), 0, 1);
    game.tick(1);
    EXPECT_EQ(portraitFxFor(8050), 4);

    // Power (MM6 id 75) is the one spell with two cast-fx add sites: MM6.EXE spawns a secondary overlay
    // 6030 over character 0's portrait alongside the primary 7080.
    pActiveOverlayList->Reset();
    pushSpellOrRangedAttack(static_cast<SpellId>(75), 0, CombinedSkillValue::none(), 0, 1);
    game.tick(1);
    EXPECT_EQ(portraitFxFor(7080), 4); // AllMagic casts at grandmaster, so the buff is party-wide.
    EXPECT_EQ(portraitFxFor(6030), 1);
    bool secondaryOnChar0 = false;
    for (const ActiveOverlay &slot : pActiveOverlayList->pOverlays)
        if (slot.animLength > 0 && slot.target == 100 &&
                pOverlayList->pOverlays[slot.indexToOverlayList].uOverlayID == 6030)
            secondaryOnChar0 = true;
    EXPECT_TRUE(secondaryOnChar0);

    // Below Master the single-stat buffs pick one character, and the fx follows onto that portrait alone:
    // Lucky Day (MM6 id 48, overlay 5030) picked onto character 1.
    engine->config->debug.AllMagic.setValue(false);
    pActiveOverlayList->Reset();
    pushSpellOrRangedAttack(static_cast<SpellId>(48), 0, CombinedSkillValue(10, MASTERY_NOVICE), 0, 0);
    spellTargetPicked(Pid(), 1);
    game.tick(1);
    EXPECT_EQ(portraitFxFor(5030), 1);
    bool onChar1 = false;
    for (const ActiveOverlay &slot : pActiveOverlayList->pOverlays)
        if (slot.animLength > 0 && slot.target == 101 &&
                pOverlayList->pOverlays[slot.indexToOverlayList].uOverlayID == 5030)
            onChar1 = true;
    EXPECT_TRUE(onChar1);
}

GAME_TEST(Mm6, TurnBasedCombatIcon) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    game.tick(1);

    // MM6 has no dift.bin turn-based icons (MM7's "turnstart"/"turnhour"/...), so the overlay falls back to
    // drawing two sprite framesets: newhand1 while the party can act, newglas1 while monsters take their turn
    // (MM6.EXE 0x435F03, at screen anchor (444, 326)). loadIcons() resolved and loaded them.
    ASSERT_TRUE(turnBasedOverlay.usesMm6Sprites());
    int hand = pSpriteFrameTable->FastFindSprite("newhand1");
    int glass = pSpriteFrameTable->FastFindSprite("newglas1");
    ASSERT_GT(hand, 0);
    ASSERT_GT(glass, 0);
    // The framesets were InitializeSprite'd, so their frames carry real (non-null) sprites.
    ASSERT_NE(pSpriteFrameTable->GetFrame(hand, 0_ticks)->sprites[0], nullptr);
    ASSERT_NE(pSpriteFrameTable->GetFrame(glass, 0_ticks)->sprites[0], nullptr);

    // The overlay tracks the turn stage directly (no MM7-style opening-hand phase): hourglass on the
    // monsters' turn, hand on the party's attack or movement steps, nothing when combat ends.
    turnBasedOverlay.update(8_ticks, TE_WAIT);
    EXPECT_EQ(turnBasedOverlay.state(), TURN_BASED_OVERLAY_WAIT);
    turnBasedOverlay.draw(); // Headless smoke test - draws the hourglass without crashing.

    turnBasedOverlay.update(8_ticks, TE_ATTACK);
    EXPECT_EQ(turnBasedOverlay.state(), TURN_BASED_OVERLAY_ATTACK);
    turnBasedOverlay.draw(); // Draws the hand.

    turnBasedOverlay.update(8_ticks, TE_MOVEMENT);
    EXPECT_EQ(turnBasedOverlay.state(), TURN_BASED_OVERLAY_MOVEMENT);

    turnBasedOverlay.update(8_ticks, TE_NONE);
    EXPECT_EQ(turnBasedOverlay.state(), TURN_BASED_OVERLAY_NONE);
    turnBasedOverlay.draw(); // No overlay when combat is over - must be a no-op.

    turnBasedOverlay.reset();
}

GAME_TEST(Mm6, SpellNamesLoad) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // MM6's spells.txt is parsed natively: the same 1..99 spell ids as MM7, but read with MM6's column
    // layout. A few sampled ids: 2 = Flame Arrow, 81 = Slow, 99 = Dark Containment.
    EXPECT_EQ(pSpellStats->pInfos[SPELL_FIRE_FIRE_BOLT].name, "Flame Arrow");
    EXPECT_EQ(pSpellStats->pInfos[SPELL_LIGHT_PARALYZE].name, "Slow");
    EXPECT_FALSE(pSpellStats->pInfos[SPELL_DARK_SOULDRINKER].name.empty()); // MM6 id 99 = Dark Containment
}

// The MM6 and MM7 spell tables share the identical 9-school x 11-spell id layout, but the spell that sits
// at a given id often differs between the two games. The cast runtime dispatches on MM7-named SpellId
// constants, so an MM6 spell has to be routed through translateForCast to the MM7 spell whose effect (and
// targeting mode) matches. These two gates cast such remapped MM6 spells and check that the intended effect,
// not MM7's same-id effect, runs. (They are the fail-first tests for that wiring: without translateForCast in
// the dispatch path both produce no projectile.)
GAME_TEST(Mm6, CastShiftedSpell) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    engine->config->debug.AllMagic.setValue(true); // Cast lands every time - no mana/skill/mastery gating.

    // MM6 spell id 30 is Acid Burst, a projectile. MM7's id 30 is Enchant Item, an inventory-target spell that
    // launches no projectile and opens the item-enchant window instead. This is one half of MM6's id29/id30
    // swap (Enchant Item and Acid Burst trade slots relative to MM7). Casting the MM6 spell must fire an acid
    // burst, not enter item-enchant targeting.
    // Quick-cast it (nonzero overrideSoundId, as the quick-spell button does) so a projectile spell fires
    // immediately instead of opening an actor-targeting window.
    pushSpellOrRangedAttack(static_cast<SpellId>(30), 0, CombinedSkillValue::none(), 0, 1);
    game.tick(1);

    // The item-enchant targeting mode must NOT have been entered, and a projectile carrying the native MM6
    // spell id 30 must have been launched (the Acid Burst effect ran).
    EXPECT_FALSE(IsEnchantingInProgress);
    int projectiles = std::ranges::count_if(pSpriteObjects, [](const SpriteObject &obj) {
        return obj.uSpellID == static_cast<SpellId>(30) && obj.uObjectDescID != 0;
    });
    EXPECT_GE(projectiles, 1);

    // The projectile sprite stays keyed off the NATIVE id (MM6's dobjlist projectile objects are indexed by
    // the native MM6 spell slot). MM6 Acid Burst is native id 30, whose slot carries the acid projectile
    // object; the effect id (MM7 Acid Burst, id 29) is MM6's Enchant Item slot, which has no projectile object
    // at all - so an effect-keyed sprite would create no projectile here.
    EXPECT_NE(pObjectList->ObjectIDByItemID(SpellSpriteMapping[static_cast<SpellId>(30)]), 0u);
    EXPECT_EQ(pObjectList->ObjectIDByItemID(SpellSpriteMapping[SPELL_WATER_ACID_BURST]), 0u);
}

GAME_TEST(Mm6, CastUniqueSpellAnalog) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    engine->config->debug.AllMagic.setValue(true);

    // MM6 spell id 8 is Fire Blast, a fireball-like projectile with no same-id MM7 equivalent - MM7's id 8 is
    // Immolation, a self-only party buff that launches no projectile. translateForCast maps the MM6 spell to
    // MM7's Fireball projectile effect. Without it, casting id 8 would just apply the Immolation buff and
    // create nothing to observe.
    pushSpellOrRangedAttack(static_cast<SpellId>(8), 0, CombinedSkillValue::none(), 0, 1);
    game.tick(1);

    // A projectile carrying the native MM6 spell id 8 must have been launched (a fire projectile effect ran).
    int projectiles = std::ranges::count_if(pSpriteObjects, [](const SpriteObject &obj) {
        return obj.uSpellID == static_cast<SpellId>(8) && obj.uObjectDescID != 0;
    });
    EXPECT_GE(projectiles, 1);
}

// MM6 monster spell attacks come from MM6's monsters.txt, which names spells by MM6's own spells.txt names -
// and the spell that sits at a given id differs from MM7's. ParseSpellType resolves each name against the
// loaded MM6 spell table into a NATIVE MM6 SpellId; castSpell()'s translateForCast then maps it to the matching
// MM7 effect. Before this wiring the names were matched against MM7's hardcoded name map, resolving them to the
// wrong id, or (for MM6-unique names) to SPELL_NONE with an "Unknown monster spell" warning.
GAME_TEST(Mm6, MonsterSpellNamesResolve) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // Goblin C (monster id 78) casts "Fire Bolt". In MM6 "Fire Bolt" is native spell id 4 (spells.txt row 4),
    // whereas MM7's id 4 is Fire Aura - so the native id lands on SPELL_FIRE_FIRE_AURA's slot, NOT MM7's id-2
    // Fire Bolt that the old name map returned. translateForCast maps the native id to MM7's Fire Bolt effect.
    const MonsterInfo &goblin = pMonsterStats->infos[MonsterId(78)];
    EXPECT_GT(goblin.spell1UseChance, 0);
    EXPECT_EQ(goblin.spell1Id, SPELL_FIRE_FIRE_AURA); // Native MM6 id 4 = "Fire Bolt".
    EXPECT_TRUE(isRegularSpell(goblin.spell1Id));
    EXPECT_EQ(translateForCast(goblin.spell1Id, GAME_VERSION_MM6), SPELL_FIRE_FIRE_BOLT);

    // Ooze B (id 116) casts "Poison Spray", an MM6-only name absent from MM7's monster-spell map (it resolved
    // to SPELL_NONE plus a warning before the fix). MM6's "Poison Spray" is native id 26.
    const MonsterInfo &ooze = pMonsterStats->infos[MonsterId(116)];
    EXPECT_GT(ooze.spell1UseChance, 0);
    EXPECT_EQ(ooze.spell1Id, SPELL_WATER_ICE_BOLT); // Native MM6 id 26 = "Poison Spray".
    EXPECT_TRUE(isRegularSpell(ooze.spell1Id));

    // Across the whole monster table, every spell-casting monster resolves to a real regular spell, except a
    // small handful of shipped-data typos ("Dispell Magic" with a doubled L on Beholder C / Lich A / Lich B,
    // "Psychic Shockt" on Titan B) that match no spells.txt name - those keep no spell, as in the original.
    int resolved = 0;
    int unresolved = 0;
    for (MonsterId id : pMonsterStats->infos.indices()) {
        const MonsterInfo &info = pMonsterStats->infos[id];
        if (info.spell1UseChance == 0)
            continue;
        if (isRegularSpell(info.spell1Id))
            resolved++;
        else
            unresolved++;
    }
    EXPECT_GE(resolved, 50); // 55 casters minus the 4 typo rows.
    EXPECT_LE(unresolved, 5);
}

// Monsters don't cast through castSpell() - they use Actor::AI_SpellAttack, which switches on the spell id.
// Now that MM6 monster spells resolve to NATIVE MM6 ids (MonsterSpellNamesResolve), AI_SpellAttack has to
// translate that id to the matching MM7 effect for its switch, exactly like castSpell does - otherwise a
// native MM6 id with no MM7 case (e.g. Fire Bolt = native id 4 = SPELL_FIRE_FIRE_AURA's slot) hits the
// switch's default: assert(false) and aborts. Asset/data reads inside the cases stay native (MM6's
// projectile bank is native-indexed).
GAME_TEST(Mm6, MonsterCastsSpell) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // Goblinwatch is indoor, so the projectile's sector lookup is well-defined.
    MapId goblinwatch = pMapStats->GetMapInfo("d01.blv");
    ASSERT_NE(goblinwatch, MAP_INVALID);
    game.teleportTo(goblinwatch, Vec3f(-1850, 4304, -512), 0); // First spawn point of d01.blv.
    ASSERT_FALSE(pActors.empty());

    // Make an actor cast MM6 Goblin C's spell attack ("Fire Bolt" = native spell id 4, a projectile). Its
    // native id is SPELL_FIRE_FIRE_AURA's slot; translateForCast maps it to MM7's Fire Bolt effect.
    Actor &caster = pActors[0];
    caster.monsterInfo = pMonsterStats->infos[MonsterId(78)];
    SpellId nativeSpell = caster.monsterInfo.spell1Id;
    ASSERT_EQ(nativeSpell, SPELL_FIRE_FIRE_AURA); // MM6 native id 4 = "Fire Bolt".
    ASSERT_EQ(translateForCast(nativeSpell, GAME_VERSION_MM6), SPELL_FIRE_FIRE_BOLT);

    size_t spritesBefore = pSpriteObjects.size();

    AIDirection dir;
    dir.uDistance = 1500;
    dir.uDistanceXZ = 1500;
    // Before the AI_SpellAttack fix this aborts on the switch's default: assert(false); after it, the Fire
    // Bolt case launches a projectile carrying the NATIVE spell id with MM6's native-indexed projectile sprite.
    Actor::AI_SpellAttack(0, &dir, nativeSpell, ABILITY_SPELL1, caster.monsterInfo.spell1SkillMastery);

    int projectiles = std::ranges::count_if(pSpriteObjects, [nativeSpell](const SpriteObject &obj) {
        return obj.uSpellID == nativeSpell && obj.uObjectDescID != 0;
    });
    EXPECT_GE(projectiles, 1);
    EXPECT_GT(pSpriteObjects.size(), spritesBefore);
}

// A shifted MM6 damage spell must actually deal impact damage. Two layers are needed and both are exercised
// here: (1) processSpellImpact dispatches on the EFFECT's sprite (the native-slot sprite of e.g. Fire Bolt is
// Fire Aura's, which has no impact case -> the projectile hit the damage-less default), and (2) CalcSpellDamage
// resolves the magnitude via the effect spell (the native-slot data is a 0-damage buff). Missing either leaves
// MM6 Fire Bolt at 0 damage. This drives a player cast into a monster; the same CalcSpellDamage chokepoint and
// processSpellImpact serve the monster->party direction (see Mm6.MonsterCastsSpell for the monster projectile).
GAME_TEST(Mm6, ShiftedSpellDealsImpactDamage) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    engine->config->debug.AllMagic.setValue(true); // Casts always land - no mana/skill gating.

    MapId goblinwatch = pMapStats->GetMapInfo("d01.blv");
    ASSERT_NE(goblinwatch, MAP_INVALID);
    game.teleportTo(goblinwatch, Vec3f(-1850, 4304, -512), 0); // Indoor, so the projectile's sector is defined.
    game.tick(1);

    int monId = -1;
    for (size_t i = 0; i < pActors.size(); i++) {
        if (pActors[i].hp > 0) {
            monId = static_cast<int>(i);
            break;
        }
    }
    ASSERT_NE(monId, -1);

    // Quick-cast a native MM6 fire projectile, then impact it straight onto the monster; return HP lost.
    auto castImpactDamage = [&](int nativeId) -> int {
        pActors[monId].hp = 500;
        pActors[monId].monsterInfo.resFire = 0; // Deterministic: the found monster must not resist the damage.
        pushSpellOrRangedAttack(static_cast<SpellId>(nativeId), 0, CombinedSkillValue::none(), 0, 1);
        game.tick(1);
        int proj = -1;
        for (size_t j = 0; j < pSpriteObjects.size(); j++)
            if (pSpriteObjects[j].uSpellID == static_cast<SpellId>(nativeId))
                proj = static_cast<int>(j);
        EXPECT_NE(proj, -1) << "native spell " << nativeId << " formed no projectile";
        if (proj == -1)
            return 0;
        // The projectile may have clipped scenery during its creating tick; restore its fresh pre-impact native
        // sprite so the impact runs cleanly on the monster (updateSpriteOnImpact asserts a projectile sprite).
        SpriteObject &p = pSpriteObjects[proj];
        p.uSpellID = static_cast<SpellId>(nativeId);
        p.spriteId = SpellSpriteMapping[static_cast<SpellId>(nativeId)];
        p.uObjectDescID = pObjectList->ObjectIDByItemID(p.spriteId);
        pActors[monId].hp = 500;
        processSpellImpact(proj, Pid(OBJECT_Actor, monId));
        return 500 - pActors[monId].hp;
    };

    // Shifted: MM6 Fire Bolt (native id 4) - the most common monster attack spell. 0 before either layer.
    EXPECT_GT(castImpactDamage(4), 0);

    // Aligned: MM6 Flame Arrow (native id 2) already sits on the Fire Bolt sprite/data - damage must still
    // land, proving the central CalcSpellDamage remap didn't break the aligned (identity) path.
    EXPECT_GT(castImpactDamage(2), 0);
}

// AI_SpellAttack's switch only had cases for the effects MM7 monsters cast. MM6 monsters cast more: some are
// damage projectiles now added to the launch group (Ooze Poison Spray, Druidess Deadly Swarm, Cleric Flying
// Fist), while a few (Minotaur's Finger of Death -> Souldrinker, and the two data-typo SPELL_NONE spells) have
// no case and must no-op gracefully instead of hitting the switch's default: assert(false).
GAME_TEST(Mm6, MonsterCastsUncoveredSpell) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    MapId goblinwatch = pMapStats->GetMapInfo("d01.blv");
    ASSERT_NE(goblinwatch, MAP_INVALID);
    game.teleportTo(goblinwatch, Vec3f(-1850, 4304, -512), 0);
    ASSERT_FALSE(pActors.empty());

    Actor &caster = pActors[0];
    AIDirection dir;
    dir.uDistance = 1500;
    dir.uDistanceXZ = 1500;

    // Added case: Ooze B (id 116) casts "Poison Spray" (native id 26 -> effect Poison Spray), now a
    // projectile-launch case, so a projectile carrying the native id is fired.
    caster.monsterInfo = pMonsterStats->infos[MonsterId(116)];
    SpellId oozeSpell = caster.monsterInfo.spell1Id;
    ASSERT_EQ(translateForCast(oozeSpell, GAME_VERSION_MM6), SPELL_WATER_POISON_SPRAY);
    Actor::AI_SpellAttack(0, &dir, oozeSpell, ABILITY_SPELL1, caster.monsterInfo.spell1SkillMastery);
    int poisonProjectiles = std::ranges::count_if(pSpriteObjects, [oozeSpell](const SpriteObject &obj) {
        return obj.uSpellID == oozeSpell && obj.uObjectDescID != 0;
    });
    EXPECT_GE(poisonProjectiles, 1);

    // Still-uncovered effect: some shipped MM6 monsters name their spell with a typo ("Dispell Magic",
    // "Psychic Shockt") that matches no spells.txt entry and resolves to SPELL_NONE. That has no AI_SpellAttack
    // case, so the MM6-gated default must no-op it - no abort, no projectile. Reaching the asserts below at all
    // means it did not abort on the switch's default: assert(false). (Finger of Death used to sit here; it is
    // now a real monster cast - see Mm6.MonsterCastsFingerOfDeath.)
    MonsterId typoCaster = MONSTER_INVALID;
    for (MonsterId id : pMonsterStats->infos.indices()) {
        const MonsterInfo &info = pMonsterStats->infos[id];
        if (info.spell1UseChance > 0 && !isRegularSpell(info.spell1Id)) {
            typoCaster = id;
            break;
        }
    }
    ASSERT_NE(typoCaster, MONSTER_INVALID);
    caster.monsterInfo = pMonsterStats->infos[typoCaster];
    SpellId typoSpell = caster.monsterInfo.spell1Id;
    ASSERT_FALSE(isRegularSpell(typoSpell));
    size_t spritesBefore = pSpriteObjects.size();
    Actor::AI_SpellAttack(0, &dir, typoSpell, ABILITY_SPELL1, caster.monsterInfo.spell1SkillMastery);
    EXPECT_EQ(pSpriteObjects.size(), spritesBefore);
}

// MM6 Finger of Death is also a monster spell (Minotaur C among others). Monsters cast through AI_SpellAttack,
// whose switch now handles Souldrinker (Finger of Death's effect id) for MM6: the monster tries to slay one
// party member outright, 3/4/5% per point of skill. It used to be a documented no-op (see the earlier form of
// Mm6.MonsterCastsUncoveredSpell). No MM7 monster casts Souldrinker, so the case is MM6-only.
GAME_TEST(Mm6, MonsterCastsFingerOfDeath) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    MapId goblinwatch = pMapStats->GetMapInfo("d01.blv");
    ASSERT_NE(goblinwatch, MAP_INVALID);
    game.teleportTo(goblinwatch, Vec3f(-1850, 4304, -512), 0);
    ASSERT_FALSE(pActors.empty());

    for (const Character &character : pParty->pCharacters)
        ASSERT_FALSE(character.conditions.has(CONDITION_DEAD)); // All four start alive.

    Actor &caster = pActors[0];
    caster.monsterInfo = pMonsterStats->infos[MonsterId(108)]; // Minotaur C casts Finger of Death.
    SpellId fingerSpell = caster.monsterInfo.spell1Id;
    ASSERT_EQ(translateForCast(fingerSpell, GAME_VERSION_MM6), SPELL_DARK_SOULDRINKER);

    AIDirection dir;
    dir.uDistance = 1500;
    dir.uDistanceXZ = 1500;
    // Skill 20 Master -> 5% * 20 = 100% success, so exactly one party member is slain.
    Actor::AI_SpellAttack(0, &dir, fingerSpell, ABILITY_SPELL1, CombinedSkillValue(20, MASTERY_MASTER));

    int dead = 0;
    for (const Character &character : pParty->pCharacters)
        if (character.conditions.has(CONDITION_DEAD))
            dead++;
    EXPECT_EQ(dead, 1);
}

// MM6 Golden Touch (native id 79) converts a chosen inventory item into gold. It has no MM7 counterpart -
// translateForCast maps it onto Dispel Magic, which would dispel every creature in sight instead. It is an
// inventory-target spell (like Enchant Item): the MM6 targeting override routes it into the item picker, and
// castMm6UniqueSpell runs the conversion once an item is chosen.
GAME_TEST(Mm6, GoldenTouch) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    game.tick(1);
    pParty->SetGold(0);

    // Put a plain, valuable item in character 0's backpack to convert.
    Character &caster = pParty->pCharacters[0];
    InventoryEntry item = caster.inventory.add(Item(ItemId(84)));
    ASSERT_TRUE(item);
    int value = item->GetValue();
    ASSERT_GT(value, 0);
    int itemIndex = item.index();

    // Casting non-quick (overrideSoundId 0) must enter the inventory item picker, proving the MM6 targeting
    // override routed Golden Touch to item targeting rather than Dispel's targetless mass cast.
    pushSpellOrRangedAttack(static_cast<SpellId>(79), 0, CombinedSkillValue(10, MASTERY_MASTER), 0, 0);
    game.tick(1);
    ASSERT_TRUE(IsEnchantingInProgress);

    // Mirror the inventory-item click (UICharacter.cpp): hand the queued cast its item target and clear the
    // enchant-in-progress state so castSpell runs it next tick.
    CastSpellInfo *info = pGUIWindow_CastTargetedSpell->spellInfo();
    info->flags &= ~ON_CAST_TargetedEnchantment;
    info->targetCharacterIndex = 0;
    info->targetInventoryIndex = itemIndex;
    IsEnchantingInProgress = false;
    game.tick(1);

    // Skill 10 -> 100% success; Master -> 80% of the item's value in gold, and the item is consumed.
    EXPECT_EQ(pParty->GetGold(), value * 80 / 100);
    EXPECT_FALSE(caster.inventory.entry(itemIndex));
}

GAME_TEST(Mm6, SpellManaCosts) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // pSpellDatas is statically initialized with MM7 numbers; applyMm6SpellDatas() must have replaced the
    // mana costs and recovery times with the MM6 values extracted from MM6.EXE's SpellInfo table (VA
    // 0x4BDD70). These native ids assert values that DIFFER from MM7, proving the MM6 table was applied.
    // Cross-validated against tartarus.rpgclassics.com/mm6 and the MM7-vs-EXE decode. In MM6, unlike MM7,
    // mana can drop with mastery and Light/Dark magic is far more expensive.

    // Native id 2 = MM6 "Flame Arrow": mana 2/1/0 across Novice/Expert/Master (MM7 Fire Bolt is a flat 2).
    EXPECT_EQ(pSpellDatas[SPELL_FIRE_FIRE_BOLT].mana_per_skill[MASTERY_NOVICE], 2);
    EXPECT_EQ(pSpellDatas[SPELL_FIRE_FIRE_BOLT].mana_per_skill[MASTERY_EXPERT], 1);
    EXPECT_EQ(pSpellDatas[SPELL_FIRE_FIRE_BOLT].mana_per_skill[MASTERY_MASTER], 0);
    // MM6 recovery for that spell is 100/90/80 (MM7 is 110/110/100).
    EXPECT_EQ(pSpellDatas[SPELL_FIRE_FIRE_BOLT].recovery_per_skill[MASTERY_NOVICE], Duration::fromTicks(100));
    EXPECT_EQ(pSpellDatas[SPELL_FIRE_FIRE_BOLT].recovery_per_skill[MASTERY_MASTER], Duration::fromTicks(80));

    // Native id 78 = first Light spell: 20 mana in MM6 vs 5 in MM7, recovery 100 vs MM7's 110.
    EXPECT_EQ(pSpellDatas[SPELL_LIGHT_LIGHT_BOLT].mana_per_skill[MASTERY_NOVICE], 20);
    EXPECT_EQ(pSpellDatas[SPELL_LIGHT_LIGHT_BOLT].recovery_per_skill[MASTERY_NOVICE], Duration::fromTicks(100));

    // Native id 89 = first Dark spell: 20 mana in MM6 vs 10 in MM7.
    EXPECT_EQ(pSpellDatas[SPELL_DARK_REANIMATE].mana_per_skill[MASTERY_NOVICE], 20);

    // MM6 has no Grandmaster tier, so the GM slot must mirror the Master value.
    EXPECT_EQ(pSpellDatas[SPELL_DARK_SOULDRINKER].mana_per_skill[MASTERY_GRANDMASTER],
              pSpellDatas[SPELL_DARK_SOULDRINKER].mana_per_skill[MASTERY_MASTER]);
    EXPECT_EQ(pSpellDatas[SPELL_DARK_SOULDRINKER].mana_per_skill[MASTERY_NOVICE], 200); // MM7 is 60.
}

GAME_TEST(Mm6, SpellLearnMastery) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // MM6 caps mastery at Master (no Grandmaster tier). applyMm6SpellDatas() must clamp the min-mastery of
    // each school's 11th spell down from MM7's Grandmaster to Master; otherwise the spellbook learn gate
    // (Character.cpp, requiredMastery > val.mastery()) would leave those 9 spells permanently unlearnable.
    // Native id 11 = Incinerate, the 11th Fire spell (MASTERY_GRANDMASTER in MM7's pSpellDatas).
    EXPECT_EQ(pSpellDatas[SPELL_FIRE_INCINERATE].skillMastery, MASTERY_MASTER);
    EXPECT_EQ(pSpellDatas[SPELL_DARK_SOULDRINKER].skillMastery, MASTERY_MASTER); // native id 99, also GM in MM7.

    // Non-top spells keep their existing (sub-Grandmaster) tier - the clamp only touches Grandmaster rows.
    EXPECT_EQ(pSpellDatas[SPELL_FIRE_TORCH_LIGHT].skillMastery, MASTERY_NOVICE); // native id 1.
}

// MM6 Create Food (native id 78, the first Light spell) has no MM7 counterpart - translateForCast maps it onto
// First Aid, which would heal a targeted character instead of stocking the party's food. castMm6UniqueSpell
// runs the real effect: it fills the party's food up to 1 day + 1/2/3 days per 10 skill (Novice/Expert/Master),
// but only when the current supply is lower.
GAME_TEST(Mm6, CreateFood) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // Master, skill 10 -> 1 + 3 * (10 / 10) = 4 days of food. Quick-cast (nonzero overrideSoundId) so it casts
    // immediately with no target picker; the MM6 targeting override keeps this a party-wide, targetless cast.
    pParty->SetFood(0);
    pushSpellOrRangedAttack(static_cast<SpellId>(78), 0, CombinedSkillValue(10, MASTERY_MASTER), 0, 1);
    game.tick(1);
    EXPECT_EQ(pParty->GetFood(), 4);

    // Casting again with more food already on hand than the spell would create is a no-op - it fills up to the
    // amount, never adds on top.
    pParty->SetFood(20);
    pushSpellOrRangedAttack(static_cast<SpellId>(78), 0, CombinedSkillValue(10, MASTERY_MASTER), 0, 1);
    game.tick(1);
    EXPECT_EQ(pParty->GetFood(), 20);
}

// MM6 Finger of Death (native id 95) tries to instantly slay a single creature, 3/4/5% per point of skill at
// Novice/Expert/Master. translateForCast maps it onto Souldrinker (a viewport-wide life-drain AoE), so without
// castMm6UniqueSpell it would drain the whole room instead of gambling on one target. On success the target
// dies outright and the party is rewarded exactly like any other kill.
GAME_TEST(Mm6, FingerOfDeath) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    MapId goblinwatch = pMapStats->GetMapInfo("d01.blv");
    ASSERT_NE(goblinwatch, MAP_INVALID);
    game.teleportTo(goblinwatch, Vec3f(-1850, 4304, -512), 0); // First spawn point of d01.blv.
    game.tick(1);

    int monId = -1;
    for (size_t i = 0; i < pActors.size(); i++) {
        if (pActors[i].CanAct() && pActors[i].hp > 0 && pActors[i].monsterInfo.exp > 0) {
            monId = static_cast<int>(i);
            break;
        }
    }
    ASSERT_NE(monId, -1);

    int expBefore = 0;
    for (const Character &character : pParty->pCharacters)
        expBefore += character.experience;

    // Point at the target so castSpell picks it, then cast at skill 20 Master -> 5% * 20 = 100% success (a
    // guaranteed kill). Quick-cast so no targeting window opens; the mouse target is used directly.
    mouse->uPointingObjectID = Pid(OBJECT_Actor, monId);
    pushSpellOrRangedAttack(static_cast<SpellId>(95), 0, CombinedSkillValue(20, MASTERY_MASTER), 0, 1);
    game.tick(1);

    EXPECT_EQ(pActors[monId].aiState, Dying); // Slain outright.
    EXPECT_LE(pActors[monId].hp, 0);

    int expAfter = 0;
    for (const Character &character : pParty->pCharacters)
        expAfter += character.experience;
    EXPECT_GT(expAfter, expBefore); // The kill rewarded party experience.
}

// MM6's single-stat buff family - Lucky Day (48, Luck), Meditation (56, Intellect+Personality),
// Precision (59, Accuracy), Speed (73, Speed) and Power (75, Might+Endurance) - has no MM7 counterpart,
// so translateForCast runs each as an unrelated analog. castMm6UniqueSpell instead applies the real
// per-stat character buff: +(10 + 2/skill at Novice, 3/skill at Expert & Master) for one hour per skill
// point (MM6.EXE CastSpell dispatch 0x422C93; the duration lea-chain is a flat 3600*L game-ticks for
// every mastery, the same unit that makes Torch Light "1 hour per point of skill"). Novice/Expert buff a
// single chosen character; Master hits the whole party (spells.txt "Spell affects entire party").
GAME_TEST(Mm6, SingleStatBuffs) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // Precision at Novice, skill 10 -> +(2*10 + 10) = 30 Accuracy for 10 hours, on the picked character
    // (char 2) alone. Passing a CombinedSkillValue makes it a free, exact-mastery cast; the MM6 targeting
    // override opens the character picker below Master, driven here with spellTargetPicked.
    Character &c0 = pParty->pCharacters[0];
    Character &c2 = pParty->pCharacters[2];
    int accBefore = c2.GetActualAccuracy();
    Time castStart = pParty->GetPlayingTime();
    pushSpellOrRangedAttack(static_cast<SpellId>(59), 0, CombinedSkillValue(10, MASTERY_NOVICE), 0, 0);
    spellTargetPicked(Pid(), 2);
    game.tick(1);
    Time castEnd = pParty->GetPlayingTime();
    EXPECT_TRUE(c2.pCharacterBuffs[CHARACTER_BUFF_ACCURACY].Active());
    EXPECT_EQ(c2.pCharacterBuffs[CHARACTER_BUFF_ACCURACY].power, 30);
    EXPECT_GE(c2.pCharacterBuffs[CHARACTER_BUFF_ACCURACY].expireTime, castStart + Duration::fromHours(10));
    EXPECT_LE(c2.pCharacterBuffs[CHARACTER_BUFF_ACCURACY].expireTime, castEnd + Duration::fromHours(10));
    EXPECT_EQ(c2.GetActualAccuracy(), accBefore + 30); // The buff feeds the actual stat.
    EXPECT_FALSE(c0.pCharacterBuffs[CHARACTER_BUFF_ACCURACY].Active()); // Single target - char 0 untouched.

    // Expert bumps the per-skill bonus to 3: +(3*10 + 10) = 40, still single target.
    pushSpellOrRangedAttack(static_cast<SpellId>(59), 0, CombinedSkillValue(10, MASTERY_EXPERT), 0, 0);
    spellTargetPicked(Pid(), 2);
    game.tick(1);
    EXPECT_EQ(c2.pCharacterBuffs[CHARACTER_BUFF_ACCURACY].power, 40);
    EXPECT_FALSE(c0.pCharacterBuffs[CHARACTER_BUFF_ACCURACY].Active());

    // Master keeps the 3/skill bonus (40) but affects the entire party - no picker.
    pushSpellOrRangedAttack(static_cast<SpellId>(59), 0, CombinedSkillValue(10, MASTERY_MASTER), 0, 1);
    game.tick(1);
    for (Character &character : pParty->pCharacters)
        EXPECT_EQ(character.pCharacterBuffs[CHARACTER_BUFF_ACCURACY].power, 40);

    // Each family member buffs its own attribute(s). Cast every one at Master, skill 10 -> power 40; the
    // five spells touch disjoint stats, so all buffs coexist on the party.
    struct StatBuff { int id; std::vector<CharacterBuff> stats; };
    std::vector<StatBuff> family = {
        {48, {CHARACTER_BUFF_LUCK}},
        {56, {CHARACTER_BUFF_INTELLIGENCE, CHARACTER_BUFF_PERSONALITY}},
        {59, {CHARACTER_BUFF_ACCURACY}},
        {73, {CHARACTER_BUFF_SPEED}},
        {75, {CHARACTER_BUFF_STRENGTH, CHARACTER_BUFF_ENDURANCE}},
    };
    for (const StatBuff &spell : family) {
        pushSpellOrRangedAttack(static_cast<SpellId>(spell.id), 0, CombinedSkillValue(10, MASTERY_MASTER), 0, 1);
        game.tick(1);
    }
    for (Character &character : pParty->pCharacters)
        for (const StatBuff &spell : family)
            for (CharacterBuff buff : spell.stats)
                EXPECT_EQ(character.pCharacterBuffs[buff].power, 40)
                    << "spell " << spell.id << " buff " << std::to_underlying(buff);
}

// MM6 Day of the Gods (83) casts the whole single-stat buff family - Power, Meditation, Speed, Lucky Day
// and Precision - on the entire party at an effective strength of 2x/3x/4x Light skill (Novice/Expert/
// Master), i.e. +(mult*L + 10) to each of the seven attributes for mult*L hours (MM6.EXE 0x428A43). It maps
// onto MM7's Day of the Gods, whose case asserts(false) on Novice (MM7 has no Novice Day of the Gods) - so
// without the MM6 handling a Novice cast aborts. (MM6 also folds in Guardian Angel; no OpenEnroth buff yet.)
GAME_TEST(Mm6, DayOfTheGods) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    const std::array<CharacterBuff, 7> stats = {
        CHARACTER_BUFF_STRENGTH, CHARACTER_BUFF_ENDURANCE, CHARACTER_BUFF_INTELLIGENCE,
        CHARACTER_BUFF_PERSONALITY, CHARACTER_BUFF_ACCURACY, CHARACTER_BUFF_SPEED, CHARACTER_BUFF_LUCK};

    // (mastery, effective multiplier). Novice does NOT abort - that is the bug this fixes.
    struct Tier { Mastery mastery; int mult; };
    for (const Tier &tier : {Tier{MASTERY_NOVICE, 2}, Tier{MASTERY_EXPERT, 3}, Tier{MASTERY_MASTER, 4}}) {
        int expectedPower = tier.mult * 10 + 10;
        Duration expectedDuration = Duration::fromHours(tier.mult * 10);
        Time castStart = pParty->GetPlayingTime();
        pushSpellOrRangedAttack(static_cast<SpellId>(83), 0, CombinedSkillValue(10, tier.mastery), 0, 1);
        game.tick(1);
        Time castEnd = pParty->GetPlayingTime();
        for (Character &character : pParty->pCharacters) {
            for (CharacterBuff buff : stats) {
                EXPECT_TRUE(character.pCharacterBuffs[buff].Active());
                EXPECT_EQ(character.pCharacterBuffs[buff].power, expectedPower)
                    << "mastery " << std::to_underlying(tier.mastery) << " buff " << std::to_underlying(buff);
                EXPECT_GE(character.pCharacterBuffs[buff].expireTime, castStart + expectedDuration);
                EXPECT_LE(character.pCharacterBuffs[buff].expireTime, castEnd + expectedDuration);
            }
        }
    }
}

// MM6 computes spell damage with its own per-spell formulas (MM6.EXE CalcSpellDamage @0x47F0A0), keyed on the
// NATIVE spell id and independent of mastery. CalcSpellDamage must reproduce them exactly instead of borrowing
// the MM7 effect spell's numbers through the translateForCast remap. Cross-checked against MM6's spells.txt
// descriptions (which agree with the EXE everywhere except Acid Burst, see below).
GAME_TEST(Mm6, SpellDamageNumbers) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // Deterministic "base + 1 per point of skill" spells at skill 10. All of these differ from the MM7 effect
    // spell's dice (e.g. Ring of Fire rides Inferno's effect, which rolls 12 + skill x d1 = 22, not 16), and
    // MM6 damage never varies with mastery.
    for (Mastery mastery : {MASTERY_NOVICE, MASTERY_EXPERT, MASTERY_MASTER}) {
        EXPECT_EQ(CalcSpellDamage(static_cast<SpellId>(7), 10, mastery, 0), 16);   // Ring of Fire: 6 + skill.
        EXPECT_EQ(CalcSpellDamage(static_cast<SpellId>(9), 10, mastery, 0), 18);   // Meteor Shower: 8 + skill.
        EXPECT_EQ(CalcSpellDamage(static_cast<SpellId>(10), 10, mastery, 0), 22);  // Inferno: 12 + skill.
        EXPECT_EQ(CalcSpellDamage(static_cast<SpellId>(15), 10, mastery, 0), 12);  // Sparks: 2 + skill.
        EXPECT_EQ(CalcSpellDamage(static_cast<SpellId>(22), 10, mastery, 0), 30);  // Starburst: 20 + skill.
        EXPECT_EQ(CalcSpellDamage(static_cast<SpellId>(43), 10, mastery, 0), 30);  // Death Blossom: 20 + skill.
        EXPECT_EQ(CalcSpellDamage(static_cast<SpellId>(84), 10, mastery, 0), 35);  // Prismatic Light: 25 + skill.
        EXPECT_EQ(CalcSpellDamage(static_cast<SpellId>(99), 10, mastery, 0), 60);  // Dark Containment: 50 + skill.
    }

    // Flat-dice spells ignore skill entirely - even at skill 30 they stay within their fixed range.
    for (int i = 0; i < 64; i++) {
        int flameArrow = CalcSpellDamage(static_cast<SpellId>(2), 30, MASTERY_MASTER, 0);   // 1d8.
        EXPECT_GE(flameArrow, 1);
        EXPECT_LE(flameArrow, 8);
        int staticCharge = CalcSpellDamage(static_cast<SpellId>(13), 30, MASTERY_MASTER, 0); // 2-6.
        EXPECT_GE(staticCharge, 2);
        EXPECT_LE(staticCharge, 6);
        int coldBeam = CalcSpellDamage(static_cast<SpellId>(24), 30, MASTERY_MASTER, 0);     // 2d3 = 2-6.
        EXPECT_GE(coldBeam, 2);
        EXPECT_LE(coldBeam, 6);
        int magicArrow = CalcSpellDamage(static_cast<SpellId>(35), 30, MASTERY_MASTER, 0);   // 3-8.
        EXPECT_GE(magicArrow, 3);
        EXPECT_LE(magicArrow, 8);
        int spiritArrow = CalcSpellDamage(static_cast<SpellId>(45), 30, MASTERY_MASTER, 0);  // 1d6.
        EXPECT_GE(spiritArrow, 1);
        EXPECT_LE(spiritArrow, 6);
    }

    // Per-skill dice: Fire Bolt (native id 4) rolls skill x d4, so [10, 40] at skill 10 - the current MM7
    // remap can't produce values this low once skill dice differ.
    for (int i = 0; i < 64; i++) {
        int fireBolt = CalcSpellDamage(static_cast<SpellId>(4), 10, MASTERY_NOVICE, 0);
        EXPECT_GE(fireBolt, 10);
        EXPECT_LE(fireBolt, 40);
        int sunRay = CalcSpellDamage(static_cast<SpellId>(87), 10, MASTERY_NOVICE, 0);       // 20 + skill x d20.
        EXPECT_GE(sunRay, 30);
        EXPECT_LE(sunRay, 220);
    }

    // Acid Burst (native id 30): the EXE rolls 9 + skill x (0..8) - a 0-based die, unlike the "9 plus 1-9 per
    // point of skill" its description claims. At skill 1 the range is [9, 17] and the 0 face makes min == 9
    // (an MM7-style 9 + 1d9 would bottom out at 10); over 256 rolls P(no 0 seen) = (8/9)^256 ~ 8e-14.
    int acidMin = 1000, acidMax = 0;
    for (int i = 0; i < 256; i++) {
        int acid = CalcSpellDamage(static_cast<SpellId>(30), 1, MASTERY_NOVICE, 0);
        acidMin = std::min(acidMin, acid);
        acidMax = std::max(acidMax, acid);
    }
    EXPECT_EQ(acidMin, 9);
    EXPECT_LE(acidMax, 17);

    // Mass Distortion (native id 44): 25% of the target's current HP plus 2% per point of skill, deterministic.
    EXPECT_EQ(CalcSpellDamage(static_cast<SpellId>(44), 10, MASTERY_NOVICE, 1000), 450);

    // Non-damage spells stay at zero (native id 5 = MM6 Haste; the EXE default case returns 0).
    EXPECT_EQ(CalcSpellDamage(static_cast<SpellId>(5), 10, MASTERY_MASTER, 0), 0);
}

// MM6 heal spells cure their native spells.txt amounts, not their MM7 effect analogs': Healing Touch (47)
// heals a random 3-7/5-9/7-11 at Novice/Expert/Master, First Aid (68) a flat 5/7/10, Cure Wounds (71)
// 5 plus 2 per point of skill, Power Cure (77) 10 plus 2 per point of skill on every character, and
// Shared Life (54) adds 1/2/3 points per point of skill to the pooled party health. The mechanics
// (targeting, fx, the Shared Life redistribution) stay the shared engine paths - only the amounts differ.
GAME_TEST(Mm6, SpellHealNumbers) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    Character &c2 = pParty->pCharacters[2];

    // Healing Touch (47): a random 3-7/5-9/7-11 by mastery, independent of skill. Pin the exact dice by
    // sampling - 48 casts miss a face of a 5-face die with P ~ 1e-4, and the game RNG is deterministic
    // anyway. The MM7 analog (First Aid) would heal 2*10 + 5 = 25 at Novice.
    struct HealRange { Mastery mastery; int lo; int hi; };
    for (const HealRange &tier : {HealRange{MASTERY_NOVICE, 3, 7}, HealRange{MASTERY_EXPERT, 5, 9},
                                  HealRange{MASTERY_MASTER, 7, 11}}) {
        int healMin = 1000, healMax = 0;
        for (int i = 0; i < 48; i++) {
            c2.health = 1;
            pushSpellOrRangedAttack(static_cast<SpellId>(47), 0, CombinedSkillValue(10, tier.mastery), 0, 0);
            spellTargetPicked(Pid(), 2);
            game.tick(1);
            healMin = std::min(healMin, c2.health - 1);
            healMax = std::max(healMax, c2.health - 1);
        }
        EXPECT_EQ(healMin, tier.lo) << "mastery " << std::to_underlying(tier.mastery);
        EXPECT_EQ(healMax, tier.hi) << "mastery " << std::to_underlying(tier.mastery);
    }

    // First Aid (68): a flat 5/7/10 by mastery, independent of skill (MM7's own First Aid at this id
    // scales with skill - 2/3/4 * L + 5).
    struct FlatHeal { Mastery mastery; int amount; };
    for (const FlatHeal &tier : {FlatHeal{MASTERY_NOVICE, 5}, FlatHeal{MASTERY_EXPERT, 7},
                                 FlatHeal{MASTERY_MASTER, 10}}) {
        c2.health = 1;
        pushSpellOrRangedAttack(static_cast<SpellId>(68), 0, CombinedSkillValue(10, tier.mastery), 0, 0);
        spellTargetPicked(Pid(), 2);
        game.tick(1);
        EXPECT_EQ(c2.health, 1 + tier.amount) << "mastery " << std::to_underlying(tier.mastery);
    }

    // Cure Wounds (71): 5 + 2 per point of skill at every mastery. (At Novice this coincides with the
    // MM7 analog's 2L+5 - Expert and Master are what the analog would get wrong: 26/33 at skill 7.)
    for (Mastery mastery : {MASTERY_NOVICE, MASTERY_EXPERT, MASTERY_MASTER}) {
        c2.health = 1;
        pushSpellOrRangedAttack(static_cast<SpellId>(71), 0, CombinedSkillValue(7, mastery), 0, 0);
        spellTargetPicked(Pid(), 2);
        game.tick(1);
        EXPECT_EQ(c2.health, 1 + 5 + 2 * 7) << "mastery " << std::to_underlying(mastery);
    }

    // Power Cure (77): 10 + 2 per point of skill to every character, at every mastery (the MM7 spell
    // heals 5L + 10 = 35 at skill 5, capping several starting characters at max health).
    for (Mastery mastery : {MASTERY_NOVICE, MASTERY_EXPERT, MASTERY_MASTER}) {
        for (Character &character : pParty->pCharacters)
            character.health = 1;
        pushSpellOrRangedAttack(static_cast<SpellId>(77), 0, CombinedSkillValue(5, mastery), 0, 1);
        game.tick(1);
        for (Character &character : pParty->pCharacters)
            EXPECT_EQ(character.health, 1 + 10 + 2 * 5) << "mastery " << std::to_underlying(mastery);
    }

    // Shared Life (54): pools current party health plus 1/2/3 points per point of skill at N/E/M and
    // redistributes it evenly (MM7 adds a flat 3L below Grandmaster, so Novice and Expert differ).
    struct PoolAdd { Mastery mastery; int add; };
    for (const PoolAdd &tier : {PoolAdd{MASTERY_NOVICE, 10}, PoolAdd{MASTERY_EXPERT, 20}}) {
        for (Character &character : pParty->pCharacters)
            character.health = 10;
        pushSpellOrRangedAttack(static_cast<SpellId>(54), 0, CombinedSkillValue(10, tier.mastery), 0, 1);
        game.tick(1);
        for (Character &character : pParty->pCharacters)
            EXPECT_EQ(character.health, (4 * 10 + tier.add) / 4) << "mastery " << std::to_underlying(tier.mastery);
    }
}

// MM6 Mass Curse (native id 91, the third Dark spell) has no MM7 counterpart, so translateForCast runs it as
// Toxic Cloud - a poison AoE. Its real effect (spells.txt: "Inflicts the cursed condition on all monsters in
// the sight of the caster"; MM6.EXE 0x42928b) is to curse every monster in the caster's line of sight for
// 2/3/4 minutes per point of skill at Novice/Expert/Master. A cursed monster misses every attack
// (Actor::ActorHitOrMiss returns false) until the curse expires. The state is a transient
// Actor::cursedExpireTime, not a persisted buff - MM7 never curses monsters, so the hook is a no-op there.
GAME_TEST(Mm6, MassCurse) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    MapId goblinwatch = pMapStats->GetMapInfo("d01.blv");
    ASSERT_NE(goblinwatch, MAP_INVALID);
    game.teleportTo(goblinwatch, Vec3f(-1850, 4304, -512), 0); // First spawn point of d01.blv, a goblin in view.
    game.tick(1);

    // Mass Curse only reaches monsters in the caster's line of sight, so work with whatever is actually in the
    // viewport rather than teleporting a monster around (indoor rendering culls by sector). Respawn placement
    // is RNG-dependent, so the spawn view can start empty - but with MM6 hostility live the dungeon's monsters
    // pursue the party on sight, so ticking a little always brings some into view.
    std::vector<Actor *> inView = render->getActorsInViewport(4096);
    for (int i = 0; i < 300 && inView.empty(); i++) {
        game.tick(1);
        inView = render->getActorsInViewport(4096);
    }
    ASSERT_FALSE(inView.empty());
    Actor *mon = inView[0];
    Character &target = pParty->pCharacters[0];

    // Effect hook: a cursed monster misses every attack; an un-cursed one lands some over many attempts.
    mon->cursedExpireTime = Time();
    int hitsWhenUncursed = 0;
    for (int i = 0; i < 500; i++)
        if (mon->ActorHitOrMiss(&target))
            hitsWhenUncursed++;
    EXPECT_GT(hitsWhenUncursed, 0);

    mon->cursedExpireTime = pParty->GetPlayingTime() + Duration::fromMinutes(10);
    for (int i = 0; i < 500; i++)
        EXPECT_FALSE(mon->ActorHitOrMiss(&target));

    mon->cursedExpireTime = Time(); // Clear before exercising the real cast.

    // Cast Mass Curse at Novice, skill 10 -> 2 min/skill * 10 = 20 minutes of curse on every monster in sight.
    // Nothing moves the party or monsters between this snapshot and the cast, so the same actors are in view.
    Time castStart = pParty->GetPlayingTime();
    pushSpellOrRangedAttack(static_cast<SpellId>(91), 0, CombinedSkillValue(10, MASTERY_NOVICE), 0, 1);
    game.tick(1);
    Time castEnd = pParty->GetPlayingTime();

    // At least the monsters that were in view got cursed, and every cursed monster has exactly the Novice
    // duration (2 minutes per skill point).
    Actor *cursedMon = nullptr;
    for (Actor *actor : inView) {
        if (actor->cursedExpireTime > castStart) {
            cursedMon = actor;
            EXPECT_GE(actor->cursedExpireTime, castStart + Duration::fromMinutes(20));
            EXPECT_LE(actor->cursedExpireTime, castEnd + Duration::fromMinutes(20));
        }
    }
    ASSERT_NE(cursedMon, nullptr) << "Mass Curse should have cursed at least one monster in view";
    EXPECT_FALSE(cursedMon->ActorHitOrMiss(&target)); // End to end: the cast cursed it, so it now misses.
}

// MM6 has no hostile.txt and no monster factions (MMExtension defines HostileTxt and the IsAgainst relation
// method for MM7+ only): a monster's aggression toward the party is its own monsters.txt "Hst" column - 4 for
// every regular monster, 0 for the true Peasant rows - kept as mutable per-actor state (Charm zeroes it,
// damage escalates it), and monsters NEVER fight each other (MM6 has no Berserk/Enslave spells and no
// infighting; MM6.EXE's AI target is always the party, Pid 4 verbatim in the engage path @0x40203A). With
// hostile.txt absent the relations table is all-friendly, which used to pacify every MM6 monster permanently:
// _SelectTarget never picked the party, and UpdateActorAI then reset the actor's hostility every frame.
GAME_TEST(Mm6, MonsterHostility) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    game.tick(1);

    // Street peasants (New Sorpigal's oute3 is full of them) are harmless: monsters.txt hostility 0.
    auto peasant = std::ranges::find_if(pActors, [](Actor &actor) { return actor.IsPeasant(); });
    ASSERT_NE(peasant, pActors.end());
    EXPECT_EQ(peasant->GetActorsRelation(nullptr), HOSTILITY_FRIENDLY);

    MapId goblinwatch = pMapStats->GetMapInfo("d01.blv");
    ASSERT_NE(goblinwatch, MAP_INVALID);
    game.teleportTo(goblinwatch, Vec3f(-1850, 4304, -512), 0); // First spawn point of d01.blv, a goblin in view.
    game.tick(1);

    // Pick the nearest live monster with line of sight to the party. Respawn placement is RNG-dependent
    // (and the RNG stream now differs run-to-run with street hostiles fighting on oute3), so don't
    // depend on any specific spawn layout or on the rendered viewport.
    Actor *mon = nullptr;
    int monId = -1;
    float bestDist = 5120; // _SelectTarget targets the party inside ranges[HOSTILITY_LONG] = 5120.
    for (size_t i = 0; i < pActors.size(); i++) {
        Actor &actor = pActors[i];
        if (actor.aiState == Dead || actor.aiState == Removed)
            continue;
        float dist = (actor.pos - pParty->pos).length();
        if (dist < bestDist && Detect_Between_Objects(Pid(OBJECT_Actor, i), Pid(OBJECT_Character, 0))) {
            bestDist = dist;
            mon = &actor;
            monId = i;
        }
    }
    ASSERT_NE(mon, nullptr);

    // Every regular MM6 monster is Hst 4 in monsters.txt, so its relation to the party is hostile...
    EXPECT_EQ(mon->GetActorsRelation(nullptr), HOSTILITY_LONG);

    // ...but monsters are always friendly to each other, even across families (rats vs goblins here).
    Actor *otherFamily = nullptr;
    for (Actor &actor : pActors) {
        if (monsterTypeForMonsterId(actor.monsterInfo.id) != monsterTypeForMonsterId(mon->monsterInfo.id)) {
            otherFamily = &actor;
            break;
        }
    }
    ASSERT_NE(otherFamily, nullptr);
    EXPECT_EQ(mon->GetActorsRelation(otherFamily), HOSTILITY_FRIENDLY);
    EXPECT_EQ(otherFamily->GetActorsRelation(mon), HOSTILITY_FRIENDLY);

    // Target selection picks the party for the monster in sight.
    Pid target;
    Actor::_SelectTarget(monId, &target, true);
    EXPECT_EQ(target, Pid(OBJECT_Character, 0));

    // And the live AI actually engages: the goblin (or a cave-mate) pursues or attacks within a few
    // seconds, without the party having thrown a single punch.
    bool aggro = false;
    for (int i = 0; i < 150 && !aggro; i++) {
        game.tick(1);
        for (const Actor &actor : pActors) {
            if (actor.aiState == Pursuing || actor.aiState == AttackingMelee ||
                    actor.aiState == AttackingRanged1 || actor.aiState == AttackingRanged2 ||
                    actor.aiState == AttackingRanged3 || actor.aiState == AttackingRanged4) {
                aggro = true;
                break;
            }
        }
    }
    EXPECT_TRUE(aggro);
}

// MM6 Guardian Angel (native id 50) has no MM7 counterpart, so translateForCast runs it as Preservation
// (which turns a lethal blow into unconsciousness). Its real effect (spells.txt) is a whole-party compact:
// while it is active, a total party defeat resurrects the party for HALF its gold instead of the normal
// all-gold-lost respawn, restoring 1 / half / full HP per character at Novice/Expert/Master. It lasts 1 hour
// per point of skill at every mastery (MM6.EXE 0x426b97, the same 3600*L tick chain as the stat buffs). The
// state is a transient party field (Party::_mm6GuardianAngelExpireTime), like Actor::cursedExpireTime for
// Mass Curse - MM7 never sets it, so nothing here fires in an MM7 game.
GAME_TEST(Mm6, GuardianAngel) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // Cast: whole party (no target picker, like Day of the Gods), 1 hour per skill point at every mastery,
    // with the mastery stored to pick the resurrect HP tier. skill 10 -> 10 hours.
    Time castStart = pParty->GetPlayingTime();
    pushSpellOrRangedAttack(static_cast<SpellId>(50), 0, CombinedSkillValue(10, MASTERY_MASTER), 0, 1);
    game.tick(1);
    Time castEnd = pParty->GetPlayingTime();
    EXPECT_GT(pParty->_mm6GuardianAngelExpireTime, castEnd); // Active.
    EXPECT_GE(pParty->_mm6GuardianAngelExpireTime, castStart + Duration::fromHours(10));
    EXPECT_LE(pParty->_mm6GuardianAngelExpireTime, castEnd + Duration::fromHours(10));
    EXPECT_EQ(pParty->_mm6GuardianAngelMastery, MASTERY_MASTER);

    auto killWholeParty = [&]() {
        for (Character &character : pParty->pCharacters)
            character.conditions.set(CONDITION_DEAD, pParty->GetPlayingTime());
        game.tick(10);
    };

    // While Guardian Angel is active, a total party defeat resurrects everyone for HALF the party's gold (not
    // all of it) with HP by mastery, and the game keeps playing. Novice = 1 HP, Expert = half HP, Master =
    // full HP.
    auto expectResurrect = [&](Mastery mastery, auto hpForMax) {
        pParty->_mm6GuardianAngelExpireTime = pParty->GetPlayingTime() + Duration::fromHours(10);
        pParty->_mm6GuardianAngelMastery = mastery;
        pParty->SetGold(1000);
        std::array<int, 4> maxHealth;
        for (int i = 0; i < 4; i++)
            maxHealth[i] = pParty->pCharacters[i].GetMaxHealth();
        killWholeParty();
        EXPECT_EQ(uGameState, GAME_STATE_PLAYING);
        EXPECT_EQ(pParty->GetGold(), 500) << "mastery " << std::to_underlying(mastery);
        for (int i = 0; i < 4; i++) {
            EXPECT_TRUE(pParty->pCharacters[i].CanAct());
            EXPECT_EQ(pParty->pCharacters[i].health, hpForMax(maxHealth[i]))
                << "mastery " << std::to_underlying(mastery) << " char " << i;
        }
    };
    expectResurrect(MASTERY_NOVICE, [](int) { return 1; });               // Novice: 1 HP each.
    expectResurrect(MASTERY_EXPERT, [](int maxHp) { return maxHp / 2; }); // Expert: half HP.
    expectResurrect(MASTERY_MASTER, [](int maxHp) { return maxHp; });     // Master: full HP.

    // Guardian Angel is a resurrection compact, not the week-long defeat penalty, so it does not advance the
    // calendar - it survives a resurrect and keeps protecting the party until its own timer expires.
    EXPECT_GT(pParty->_mm6GuardianAngelExpireTime, pParty->GetPlayingTime());

    // Control: with no Guardian Angel active, the same defeat loses ALL the gold and leaves everyone at 1 HP.
    pParty->_mm6GuardianAngelExpireTime = Time();
    pParty->SetGold(1000);
    killWholeParty();
    EXPECT_EQ(uGameState, GAME_STATE_PLAYING);
    EXPECT_EQ(pParty->GetGold(), 0);
    for (Character &character : pParty->pCharacters)
        EXPECT_EQ(character.health, 1);
}

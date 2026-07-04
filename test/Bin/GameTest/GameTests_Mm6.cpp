#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "Testing/Game/GameTest.h"

#include "Engine/Engine.h"
#include "Engine/Evt/EvtProgram.h"
#include "Engine/MapEnumFunctions.h"
#include "Engine/MapInfo.h"
#include "Engine/Party.h"
#include "Engine/Resources/ResourceManager.h"
#include "Engine/mm7_data.h"

#include "Utility/Math/TrigLut.h"
#include "Engine/Graphics/BSPModel.h"
#include "Engine/Graphics/Image.h"
#include "Engine/Graphics/Indoor.h"
#include "Engine/Graphics/Outdoor.h"
#include "Engine/Graphics/LocationFunctions.h"
#include "Engine/Objects/Actor.h"
#include "Engine/Objects/Chest.h"
#include "Engine/Objects/CombinedSkillValue.h"
#include "Engine/Objects/MonsterEnumFunctions.h"
#include "Engine/Objects/Monsters.h"
#include "Engine/Objects/SpriteObject.h"
#include "Engine/Spells/SpellEnums.h"
#include "Engine/Tables/HouseTable.h"
#include "Engine/Tables/ItemTable.h"
#include "Engine/Tables/MessageScrollTable.h"
#include "Engine/Tables/NPCTable.h"

#include "GUI/GUIButton.h"
#include "GUI/GUIWindow.h"
#include "GUI/UI/UIHouses.h"
#include "GUI/UI/UIMessageScroll.h"
#include "GUI/UI/Houses/Shops.h"

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

GAME_TEST(Mm6, PeasantNews) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // Find a placed peasant.
    auto peasantPos = std::ranges::find_if(pActors, [](const Actor &actor) {
        int monsterId = std::to_underlying(actor.monsterId);
        return monsterId >= 121 && monsterId <= 135 && actor.CanAct();
    })->pos;

    // Teleport right next to it, facing it.
    Vec3f pos = peasantPos + Vec3f(-160, 0, 0);
    int yawDegrees = TrigLUT.atan2(peasantPos.x - pos.x, peasantPos.y - pos.y) * 90 / 512;
    game.teleportTo(engine->_currentLoadedMapId, pos, yawDegrees);
    game.tick(1);

    // Talking to a peasant should surface a regional news line (MM6's npcnews.txt): either New Sorpigal
    // local news or a kingdom-wide rumor.
    game.pressAndReleaseKey(PlatformKey::KEY_SPACE);
    game.tick(1);
    EXPECT_FALSE(branchless_dialogue_str.empty());
    const std::vector<RegionalNewsEntry> &localNews = pNPCStats->pRegionalNews[engine->_currentLoadedMapId];
    EXPECT_EQ(localNews.size(), 30u); // New Sorpigal's share of npcnews.txt.
    auto saysIt = [](const RegionalNewsEntry &entry) { return entry.text == branchless_dialogue_str; };
    EXPECT_TRUE(std::ranges::any_of(localNews, saysIt) || std::ranges::any_of(pNPCStats->pGeneralNews, saysIt));

    // Any key dismisses the dialogue.
    game.pressAndReleaseKey(PlatformKey::KEY_SPACE);
    game.tick(2);
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

    // In the original game a new party starts with the letter and quest bit 81 already set; new-game
    // defaults are not MM6-aware yet, so arrange that state by hand - minus the letter, to exercise
    // the refusal branch first.
    pParty->_questBits.set(static_cast<QuestBit>(81));

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

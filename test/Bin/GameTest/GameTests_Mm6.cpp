#include <algorithm>
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
#include "Engine/Graphics/Indoor.h"
#include "Engine/Graphics/Outdoor.h"
#include "Engine/Graphics/LocationFunctions.h"
#include "Engine/Objects/Actor.h"
#include "Engine/Objects/Chest.h"
#include "Engine/Objects/SpriteObject.h"
#include "Engine/Tables/ItemTable.h"
#include "Engine/Tables/NPCTable.h"

#include "GUI/GUIWindow.h"

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

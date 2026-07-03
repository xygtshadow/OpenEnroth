#include <algorithm>
#include <utility>
#include <vector>

#include "Testing/Game/GameTest.h"

#include "Engine/Engine.h"
#include "Engine/MapInfo.h"
#include "Engine/Party.h"
#include "Engine/mm7_data.h"

#include "Utility/Math/TrigLut.h"
#include "Engine/Graphics/Indoor.h"
#include "Engine/Graphics/LocationFunctions.h"
#include "Engine/Objects/Actor.h"
#include "Engine/Objects/Chest.h"
#include "Engine/Objects/SpriteObject.h"
#include "Engine/Tables/NPCTable.h"

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
    // like in the original.
    teleportNextTo(pActors[peasantId].pos, 350);
    game.pressAndReleaseKey(PlatformKey::KEY_SPACE);
    game.tick(2);
    int goldFound = pParty->GetGold() - goldBefore;
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

#include <algorithm>
#include <utility>

#include "Testing/Game/GameTest.h"

#include "Engine/Engine.h"
#include "Engine/MapInfo.h"
#include "Engine/Graphics/LocationFunctions.h"
#include "Engine/Objects/Actor.h"
#include "Engine/Objects/Chest.h"
#include "Engine/Objects/SpriteObject.h"

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

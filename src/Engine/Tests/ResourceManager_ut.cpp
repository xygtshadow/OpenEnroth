#include "Testing/Game/GameTest.h"

#include "Engine/Engine.h"
#include "Engine/Resources/ResourceManager.h"

#include "Utility/Memory/Blob.h"

// eventsDataIfPresent returns the entry's contents when the table exists in the events LOD, and an
// empty Blob (rather than throwing) when it doesn't. This lets SecondaryInitialization load the
// MM6-optional tables (placemon/hostile/history, which are absent from MM6's icons.lod) uniformly:
// an absent table becomes an empty blob, and each table's Initialize() no-ops on empty input.
GAME_TEST(ResourceManager, EventsDataIfPresent) {
    ResourceManager *resources = engine->resources();

    // A table that is always present (MM7 events.lod / MM6 icons.lod) -> non-empty contents.
    EXPECT_FALSE(resources->eventsDataIfPresent("monsters.txt").empty());

    // A non-existent entry -> empty blob, and crucially no exception.
    EXPECT_TRUE(resources->eventsDataIfPresent("this_table_does_not_exist.txt").empty());
}

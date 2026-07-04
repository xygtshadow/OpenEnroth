#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Testing/Game/GameTest.h"

#include "Engine/Data/FrameEnums.h"
#include "Engine/Graphics/TurnBasedOverlay.h"
#include "Engine/Snapshots/EntitySnapshots.h"
#include "Engine/Snapshots/TableSerialization.h"
#include "Engine/Tables/IconFrameTable.h"
#include "Engine/TurnEngine/TurnEngineEnums.h"

#include "Utility/Memory/Blob.h"

// Builds a raw dift.bin blob - a uint32 record count followed by IconFrameData_MM7 records, matching the
// count-prefixed vector format that deserialize(Blob, IconFrameTable *) reads.
static Blob makeDiftBlob(const std::vector<IconFrameData_MM7> &frames) {
    std::string bytes;
    uint32_t count = frames.size();
    bytes.append(reinterpret_cast<const char *>(&count), sizeof(count));
    for (const IconFrameData_MM7 &frame : frames)
        bytes.append(reinterpret_cast<const char *>(&frame), sizeof(frame));
    return Blob::fromString(std::move(bytes));
}

static IconFrameData_MM7 makeIconFrame(std::string_view animationName, std::string_view textureName,
                                       int16_t frameLength, int16_t animationLength, FrameFlags flags) {
    IconFrameData_MM7 result = {};
    animationName.copy(result.animationName.data(), result.animationName.size() - 1);
    textureName.copy(result.textureName.data(), result.textureName.size() - 1);
    result.frameLength = frameLength;
    result.animationLength = animationLength;
    result.flags = static_cast<int16_t>(static_cast<FrameFlags::underlying_type>(flags));
    return result;
}

// MM6's icon frame table (dift.bin) has no turn-based combat animations at all - its only named animations are
// glow01..glow05 and fire, so every MM7 turn-combat name (turnstart/turnstop/turnhour/turn0..turn4) misses and
// animationId returns -1. Unguarded, loadIcons() then called animationLength(-1), indexing _frames[-1] and
// aborting the MM6 launch with an _STL_VERIFY "vector subscript out of range". loadIcons() must tolerate the
// missing icons; on real MM6 data it falls back to the newhand1/newglas1 sprite framesets, but with the MM7
// sprite data this test runs on those are absent too, so the overlay stays disabled.
GAME_TEST(TurnBasedOverlayMm6, BootsPastMissingIcons) {
    // An icon frame table shaped like MM6's: some animations, but none of the turn-combat ones.
    IconFrameTable table;
    deserialize(makeDiftBlob({
        makeIconFrame("glow01", "glow01a", 1, 6, FRAME_FIRST | FRAME_HAS_MORE),
        makeIconFrame("", "glow01b", 1, 0, FRAME_HAS_MORE),
        makeIconFrame("fire", "fr1", 1, 1, FRAME_FIRST),
    }), &table);
    ASSERT_EQ(table.animationId("turnstart"), -1);

    IconFrameTable *oldTable = std::exchange(pIconsFrameTable, &table);

    TurnBasedOverlay overlay;
    overlay.loadIcons(); // Must not abort on the missing turn-combat animations.
    overlay.update(1_ticks, TE_WAIT); // Must stay disabled and not touch the missing icons.
    overlay.update(1_ticks, TE_ATTACK);

    pIconsFrameTable = oldTable;
}

// Guards the MM7 path through the missing-icons refactor: with the turn-combat animations present, loadIcons
// must still resolve them.
GAME_TEST(TurnBasedOverlayMm7, LoadsIcons) {
    IconFrameTable table;
    deserialize(makeDiftBlob({
        makeIconFrame("turnstart", "ia01-001", 4, 8, FRAME_FIRST | FRAME_HAS_MORE),
        makeIconFrame("", "ia01-002", 4, 0, FRAME_HAS_MORE),
        makeIconFrame("turn0", "ia01-015", 3, 3, FRAME_FIRST),
        makeIconFrame("turn1", "ia01-014", 3, 3, FRAME_FIRST),
        makeIconFrame("turn2", "ia01-013", 3, 3, FRAME_FIRST),
        makeIconFrame("turn3", "ia01-012", 3, 3, FRAME_FIRST),
        makeIconFrame("turn4", "ia01-011", 3, 3, FRAME_FIRST),
        makeIconFrame("turnstop", "ia01-010", 4, 4, FRAME_FIRST),
        makeIconFrame("turnhour", "ia02-001", 2, 2, FRAME_FIRST),
    }), &table);

    IconFrameTable *oldTable = std::exchange(pIconsFrameTable, &table);

    TurnBasedOverlay overlay;
    overlay.loadIcons();
    overlay.update(1_ticks, TE_WAIT); // Enters the initial animation, exercising _initialAnimationLength.

    pIconsFrameTable = oldTable;

    EXPECT_EQ(table.animationId("turnstart"), 0);
    EXPECT_EQ(table.animationLength(0), 64_ticks); // reconstruct() scales on-disk frame times by 8 ticks each.
    EXPECT_EQ(table.animationId("turnhour"), 8);
}

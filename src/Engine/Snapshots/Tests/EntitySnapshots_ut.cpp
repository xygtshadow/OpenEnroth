#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "Testing/Game/GameTest.h"

#include "Engine/Snapshots/EntitySnapshots.h"
#include "Engine/Snapshots/CompositeSnapshots.h"
#include "Engine/Graphics/Sprites.h"
#include "Engine/Graphics/SpriteEnums.h"
#include "Engine/Time/Duration.h"

#include "Utility/Memory/Blob.h"
#include "Utility/Streams/BlobInputStream.h"

static SpriteFrame_MM6 makeMm6Frame(int32_t scale, int32_t flags, int16_t frameLength) {
    SpriteFrame_MM6 frame = {};
    frame.scale = scale;
    frame.flags = flags;
    frame.frameLength = frameLength;
    return frame;
}

// Serializes a SpriteFrameTable in MM6 on-disk layout: u32 frameCount, u32 eframeCount, then
// `frameCount` 56-byte SpriteFrame_MM6 records, then `eframeCount` u16 eframes.
static Blob makeMm6SpriteTableBlob(const std::vector<SpriteFrame_MM6> &frames, const std::vector<uint16_t> &eframes) {
    std::string bytes;
    auto append = [&bytes](const void *data, size_t size) {
        bytes.append(static_cast<const char *>(data), size);
    };

    uint32_t frameCount = static_cast<uint32_t>(frames.size());
    uint32_t eframeCount = static_cast<uint32_t>(eframes.size());
    append(&frameCount, sizeof(frameCount));
    append(&eframeCount, sizeof(eframeCount));
    for (const SpriteFrame_MM6 &frame : frames)
        append(&frame, sizeof(frame));
    for (uint16_t eframe : eframes)
        append(&eframe, sizeof(eframe));

    return Blob::fromString(std::move(bytes));
}

// MM6 sprite-frame records are 56 bytes (MM7 added animationLength + padding for 60). The MM6
// deserializer must use the 56-byte stride, or records desync.
GAME_TEST(SpriteFrameTableMm6, DeserializeUsesMm6RecordStride) {
    std::vector<SpriteFrame_MM6> frames = {
        makeMm6Frame(65536, static_cast<int32_t>(SPRITE_FRAME_HAS_MORE), 10),
        makeMm6Frame(131072, 0, 20),
    };
    std::vector<uint16_t> eframes = {0, 1};

    BlobInputStream input(makeMm6SpriteTableBlob(frames, eframes));
    SpriteFrameTable_MM6 table;
    deserialize(input, &table);

    EXPECT_EQ(table.frameCount, 2u);
    EXPECT_EQ(table.eframeCount, 2u);
    ASSERT_EQ(table.frames.size(), 2u);
    EXPECT_EQ(table.frames[0].scale, 65536);
    EXPECT_EQ(table.frames[0].frameLength, 10);
    EXPECT_EQ(table.frames[1].scale, 131072);
    EXPECT_EQ(table.frames[1].frameLength, 20);
    ASSERT_EQ(table.eframes.size(), 2u);
    EXPECT_EQ(table.eframes[0], 0);
    EXPECT_EQ(table.eframes[1], 1);
}

// MM6 doesn't store the total animation length per frame, so reconstruct must derive it as the
// sum of frame lengths within each SPRITE_FRAME_HAS_MORE group, stored on the group's first frame.
GAME_TEST(SpriteFrameTableMm6, ReconstructDerivesAnimationLength) {
    std::vector<SpriteFrame_MM6> frames = {
        // Group 1: two chained frames (10 + 20 = 30).
        makeMm6Frame(65536, static_cast<int32_t>(SPRITE_FRAME_HAS_MORE), 10),
        makeMm6Frame(65536, 0, 20),
        // Group 2: single standalone frame (5).
        makeMm6Frame(65536, 0, 5),
    };

    BlobInputStream input(makeMm6SpriteTableBlob(frames, {}));
    SpriteFrameTable_MM6 table;
    deserialize(input, &table);

    SpriteFrameTable dst;
    reconstruct(table, &dst);

    ASSERT_EQ(dst.pSpriteSFrames.size(), 3u);
    // Frame lengths and animation lengths are stored in 1/16s units in the file, *8 -> ticks.
    EXPECT_EQ(dst.pSpriteSFrames[0].frameLength, Duration::fromTicks(10 * 8));
    EXPECT_EQ(dst.pSpriteSFrames[0].animationLength, Duration::fromTicks(30 * 8));
    EXPECT_EQ(dst.pSpriteSFrames[1].frameLength, Duration::fromTicks(20 * 8));
    EXPECT_EQ(dst.pSpriteSFrames[2].animationLength, Duration::fromTicks(5 * 8));
}

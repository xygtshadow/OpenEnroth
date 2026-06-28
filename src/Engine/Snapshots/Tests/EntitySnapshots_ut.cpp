#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Testing/Game/GameTest.h"

#include "Engine/Snapshots/EntitySnapshots.h"
#include "Engine/Snapshots/CompositeSnapshots.h"
#include "Engine/Graphics/Sprites.h"
#include "Engine/Graphics/SpriteEnums.h"
#include "Engine/Objects/DecorationList.h"
#include "Engine/Objects/DecorationEnums.h"
#include "Engine/Objects/ObjectList.h"
#include "Engine/Objects/Monsters.h"
#include "Engine/Objects/ActorEnums.h"
#include "Engine/Time/Duration.h"

#include "Media/Audio/SoundInfo.h"

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

static DecorationDesc_MM6 makeMm6Decoration(std::string_view internalName, std::string_view hint, int16_t type,
                                            uint16_t height, int16_t radius, int16_t lightRadius, uint16_t spriteId,
                                            uint16_t flags, int16_t soundId) {
    DecorationDesc_MM6 desc = {};
    std::copy(internalName.begin(), internalName.end(), desc.internalName.begin());
    std::copy(hint.begin(), hint.end(), desc.hint.begin());
    desc.uType = type;
    desc.uDecorationHeight = height;
    desc.uRadius = radius;
    desc.uLightRadius = lightRadius;
    desc.uSpriteID = spriteId;
    desc.uFlags = flags;
    desc.uSoundID = soundId;
    return desc;
}

static Blob makeMm6DecorationBlob(const std::vector<DecorationDesc_MM6> &decorations) {
    std::string bytes;
    for (const DecorationDesc_MM6 &desc : decorations)
        bytes.append(reinterpret_cast<const char *>(&desc), sizeof(desc));
    return Blob::fromString(std::move(bytes));
}

// MM6's ddeclist.bin uses 80-byte DecorationDesc records (MM7 added a 4-byte colored-light field for
// 84). The MM6 deserializer must use the 80-byte stride, or records desync.
GAME_TEST(DecorationListMm6, DeserializeUsesMm6RecordStride) {
    std::vector<DecorationDesc_MM6> decorations = {
        makeMm6Decoration("dec01", "trash heap", 1, 256, 8, 0, 100, 0, 0),
        makeMm6Decoration("torch01", "torch", 2, 512, 16, 512, 200,
                          static_cast<uint16_t>(DECORATION_DESC_EMITS_FIRE), 10),
    };

    BlobInputStream input(makeMm6DecorationBlob(decorations));
    DecorationDesc_MM6 first;
    DecorationDesc_MM6 second;
    deserialize(input, &first);
    deserialize(input, &second);

    EXPECT_EQ(std::string(first.internalName.data()), "dec01");
    EXPECT_EQ(first.uType, 1);
    EXPECT_EQ(first.uSpriteID, 100);
    // The second record reads back correctly only if the first was consumed at the 80-byte MM6 stride
    // (reading it as an 84-byte MM7 record would misalign everything that follows).
    EXPECT_EQ(std::string(second.internalName.data()), "torch01");
    EXPECT_EQ(second.uType, 2);
    EXPECT_EQ(second.uLightRadius, 512);
    EXPECT_EQ(second.uSpriteID, 200);
    EXPECT_EQ(second.uSoundID, 10);
}

// MM6 decorations have no colored-light field (an MM7 addition), so reconstruct must default it to white.
GAME_TEST(DecorationListMm6, ReconstructDefaultsColoredLightToWhite) {
    DecorationDesc_MM6 src = makeMm6Decoration("torch01", "torch", 31, 512, 16, 512, 200,
                                               static_cast<uint16_t>(DECORATION_DESC_EMITS_FIRE), 10);

    DecorationDesc dst;
    reconstruct(src, &dst);

    EXPECT_EQ(dst.internalName, "torch01");
    EXPECT_EQ(dst.hint, "torch");
    EXPECT_EQ(dst.uType, 31);
    EXPECT_EQ(dst.uDecorationHeight, 512);
    EXPECT_EQ(dst.uRadius, 16);
    EXPECT_EQ(dst.uLightRadius, 512);
    EXPECT_EQ(dst.uSpriteID, 200);
    EXPECT_TRUE(dst.uFlags & DECORATION_DESC_EMITS_FIRE);
    EXPECT_EQ(dst.uSoundID, static_cast<SoundId>(10));
    EXPECT_EQ(dst.uColoredLight.r, 255);
    EXPECT_EQ(dst.uColoredLight.g, 255);
    EXPECT_EQ(dst.uColoredLight.b, 255);
    EXPECT_EQ(dst.uColoredLight.a, 255);
}

static ObjectDesc_MM6 makeMm6Object(std::string_view name, int16_t objectId, int16_t radius, int16_t height,
                                    int16_t flags, uint16_t spriteId, int16_t lifetime, int16_t speed, uint8_t r,
                                    uint8_t g, uint8_t b) {
    ObjectDesc_MM6 desc = {};
    std::copy(name.begin(), name.end(), desc.nameUnused.begin());
    desc.uObjectID = objectId;
    desc.uRadius = radius;
    desc.uHeight = height;
    desc.uFlags = flags;
    desc.uSpriteID = spriteId;
    desc.uLifetime = lifetime;
    desc.uSpeed = speed;
    desc.uParticleTrailColorR = r;
    desc.uParticleTrailColorG = g;
    desc.uParticleTrailColorB = b;
    return desc;
}

static Blob makeMm6ObjectBlob(const std::vector<ObjectDesc_MM6> &objects) {
    std::string bytes;
    for (const ObjectDesc_MM6 &desc : objects)
        bytes.append(reinterpret_cast<const char *>(&desc), sizeof(desc));
    return Blob::fromString(std::move(bytes));
}

// MM6's dobjlist.bin uses 52-byte ObjectDesc records; MM7 widened the (unused) packed particle-trail
// color from 16 to 32 bits and the trailing padding for 56-byte records. The MM6 deserializer must use
// the 52-byte stride, or records desync.
GAME_TEST(ObjectListMm6, DeserializeUsesMm6RecordStride) {
    std::vector<ObjectDesc_MM6> objects = {
        makeMm6Object("arrow", 1, 2, 3, 0, 100, 0, 500, 0, 0, 0),
        makeMm6Object("fireball", 5, 6, 7, static_cast<int16_t>(OBJECT_DESC_TRAIL_FIRE), 200, 128, 600, 255, 128, 0),
    };

    BlobInputStream input(makeMm6ObjectBlob(objects));
    ObjectDesc_MM6 first;
    ObjectDesc_MM6 second;
    deserialize(input, &first);
    deserialize(input, &second);

    EXPECT_EQ(std::string(first.nameUnused.data()), "arrow");
    EXPECT_EQ(first.uObjectID, 1);
    EXPECT_EQ(first.uSpriteID, 100);
    // The second record reads back correctly only if the first was consumed at the 52-byte MM6 stride
    // (reading it as a 56-byte MM7 record would misalign everything that follows).
    EXPECT_EQ(std::string(second.nameUnused.data()), "fireball");
    EXPECT_EQ(second.uObjectID, 5);
    EXPECT_EQ(second.uSpriteID, 200);
    EXPECT_EQ(second.uLifetime, 128);
    EXPECT_EQ(second.uSpeed, 600);
    EXPECT_EQ(second.uParticleTrailColorR, 255);
}

// MM6 carries the same per-channel R/G/B particle-trail color bytes as MM7 (only the unused packed
// color field differs in width), so reconstruct must build the color from those bytes.
GAME_TEST(ObjectListMm6, ReconstructMapsParticleTrailColor) {
    ObjectDesc_MM6 src = makeMm6Object("fireball", 5, 6, 7, static_cast<int16_t>(OBJECT_DESC_TRAIL_FIRE), 200, 128,
                                       600, 255, 128, 0);

    ObjectDesc dst;
    reconstruct(src, &dst);

    EXPECT_EQ(dst.uObjectID, static_cast<SpriteId>(5));
    EXPECT_EQ(dst.uRadius, 6);
    EXPECT_EQ(dst.uHeight, 7);
    EXPECT_TRUE(dst.uFlags & OBJECT_DESC_TRAIL_FIRE);
    EXPECT_EQ(dst.uSpriteID, 200);
    EXPECT_EQ(dst.uLifetime, Duration::fromTicks(128));
    EXPECT_EQ(dst.uSpeed, 600);
    EXPECT_EQ(dst.uParticleTrailColor.r, 255);
    EXPECT_EQ(dst.uParticleTrailColor.g, 128);
    EXPECT_EQ(dst.uParticleTrailColor.b, 0);
}

static MonsterDesc_MM6 makeMm6Monster(std::string_view name, uint16_t height, uint16_t radius, uint16_t speed,
                                      int16_t toHitRadius, uint16_t sound0, std::string_view sprite0) {
    MonsterDesc_MM6 desc = {};
    desc.monsterHeight = height;
    desc.monsterRadius = radius;
    desc.movementSpeed = speed;
    desc.toHitRadius = toHitRadius;
    desc.soundSampleIds[0] = sound0;
    std::copy(name.begin(), name.end(), desc.internalMonsterName.begin());
    std::copy(sprite0.begin(), sprite0.end(), desc.spriteNames[0].begin());
    return desc;
}

static Blob makeMm6MonsterBlob(const std::vector<MonsterDesc_MM6> &monsters) {
    std::string bytes;
    for (const MonsterDesc_MM6 &desc : monsters)
        bytes.append(reinterpret_cast<const char *>(&desc), sizeof(desc));
    return Blob::fromString(std::move(bytes));
}

// MM6's dmonlist.bin uses 148-byte MonsterDesc records; MM7 inserted a 4-byte tintColor field for 152.
// The MM6 deserializer must use the 148-byte stride, or records desync.
GAME_TEST(MonsterListMm6, DeserializeUsesMm6RecordStride) {
    std::vector<MonsterDesc_MM6> monsters = {
        makeMm6Monster("ArcherA", 173, 161, 140, 40, 1000, "archA"),
        makeMm6Monster("BatB", 200, 180, 220, 50, 2000, "batB"),
    };

    BlobInputStream input(makeMm6MonsterBlob(monsters));
    MonsterDesc_MM6 first;
    MonsterDesc_MM6 second;
    deserialize(input, &first);
    deserialize(input, &second);

    EXPECT_EQ(std::string(first.internalMonsterName.data()), "ArcherA");
    EXPECT_EQ(first.monsterHeight, 173);
    EXPECT_EQ(first.soundSampleIds[0], 1000);
    // The second record reads back correctly only if the first was consumed at the 148-byte MM6 stride
    // (reading it as a 152-byte MM7 record would misalign everything that follows).
    EXPECT_EQ(std::string(second.internalMonsterName.data()), "BatB");
    EXPECT_EQ(second.monsterHeight, 200);
    EXPECT_EQ(second.movementSpeed, 220);
    EXPECT_EQ(second.soundSampleIds[0], 2000);
}

// MM6 monsters have no tint-color field (an MM7 addition), so reconstruct must default it to white.
GAME_TEST(MonsterListMm6, ReconstructDefaultsTintColorToWhite) {
    MonsterDesc_MM6 src = makeMm6Monster("ArcherA", 173, 161, 140, 40, 1000, "archA");

    MonsterDesc dst;
    reconstruct(src, &dst);

    EXPECT_EQ(dst.internalMonsterName, "ArcherA");
    EXPECT_EQ(dst.monsterHeight, 173);
    EXPECT_EQ(dst.monsterRadius, 161);
    EXPECT_EQ(dst.movementSpeed, 140);
    EXPECT_EQ(dst.toHitRadius, 40);
    EXPECT_EQ(dst.soundSampleIds[ACTOR_SOUND_FIRST], static_cast<SoundId>(1000));
    EXPECT_EQ(dst.spriteNames[ANIM_First], "archA");
    EXPECT_EQ(dst.tintColor.r, 255);
    EXPECT_EQ(dst.tintColor.g, 255);
    EXPECT_EQ(dst.tintColor.b, 255);
    EXPECT_EQ(dst.tintColor.a, 255);
}

static SoundInfo_MM6 makeMm6Sound(std::string_view name, uint32_t soundId, uint32_t type, uint32_t flags) {
    SoundInfo_MM6 desc = {};
    std::copy(name.begin(), name.end(), desc.name.begin());
    desc.soundId = soundId;
    desc.type = type;
    desc.flags = flags;
    return desc;
}

static Blob makeMm6SoundBlob(const std::vector<SoundInfo_MM6> &sounds) {
    std::string bytes;
    for (const SoundInfo_MM6 &desc : sounds)
        bytes.append(reinterpret_cast<const char *>(&desc), sizeof(desc));
    return Blob::fromString(std::move(bytes));
}

// MM6's dsounds.bin uses 112-byte SoundInfo records; MM7 appended two (always-zero) sound3dId and
// decompressed fields for 120-byte records. The MM6 deserializer must use the 112-byte stride, or records
// desync.
GAME_TEST(SoundListMm6, DeserializeUsesMm6RecordStride) {
    std::vector<SoundInfo_MM6> sounds = {
        makeMm6Sound("fireball", 8, 1, 2),
        makeMm6Sound("openchest", 208, 0, 0),
    };

    BlobInputStream input(makeMm6SoundBlob(sounds));
    SoundInfo_MM6 first;
    SoundInfo_MM6 second;
    deserialize(input, &first);
    deserialize(input, &second);

    EXPECT_EQ(std::string(first.name.data()), "fireball");
    EXPECT_EQ(first.soundId, 8u);
    EXPECT_EQ(first.flags, 2u);
    // The second record reads back correctly only if the first was consumed at the 112-byte MM6 stride
    // (reading it as a 120-byte MM7 record would misalign everything that follows).
    EXPECT_EQ(std::string(second.name.data()), "openchest");
    EXPECT_EQ(second.soundId, 208u);
    EXPECT_EQ(second.type, 0u);
}

// MM6 sounds carry the same name/id/type/flags as MM7 (only the unused trailing fields differ), so
// reconstruct must map them across.
GAME_TEST(SoundListMm6, ReconstructMapsFields) {
    SoundInfo_MM6 src = makeMm6Sound("fireball", 8, 1, 2);

    SoundInfo dst;
    reconstruct(src, &dst);

    EXPECT_EQ(dst.name, "fireball");
    EXPECT_EQ(dst.soundId, static_cast<SoundId>(8));
    EXPECT_EQ(dst.type, static_cast<SoundType>(1));
    EXPECT_TRUE(dst.flags & SOUND_FLAG_3D);
}

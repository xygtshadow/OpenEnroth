#include <algorithm>
#include <array>
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

static SpriteFrame_MM6 makeMm6Frame(int32_t scale, uint16_t flags, int16_t frameLength, int16_t animationLength) {
    SpriteFrame_MM6 frame = {};
    frame.scale = scale;
    frame.flags = flags;
    frame.frameLength = frameLength;
    frame.animationLength = animationLength;
    return frame;
}

// Builds one 56-byte MM6 sprite frame record byte by byte, independently of the SpriteFrame_MM6
// struct layout, so these tests catch field-offset mistakes in the struct itself.
static std::string makeMm6FrameBytes(std::string_view groupName, std::string_view textureName, int32_t scale,
                                     uint16_t bits, int16_t lightRadius, int16_t paletteId, int16_t paletteIndex,
                                     int16_t time, int16_t totalTime) {
    std::string bytes;
    auto appendPod = [&bytes](const auto &value) {
        bytes.append(reinterpret_cast<const char *>(&value), sizeof(value));
    };
    std::array<char, 12> name = {};
    std::copy(groupName.begin(), groupName.end(), name.begin());
    bytes.append(name.data(), name.size());
    name = {};
    std::copy(textureName.begin(), textureName.end(), name.begin());
    bytes.append(name.data(), name.size());
    std::array<int16_t, 8> hwSpriteIds = {};
    appendPod(hwSpriteIds);
    appendPod(scale);
    appendPod(bits);
    appendPod(lightRadius);
    appendPod(paletteId);
    appendPod(paletteIndex);
    appendPod(time);
    appendPod(totalTime);
    return bytes;
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
        makeMm6Frame(65536, static_cast<uint16_t>(SPRITE_FRAME_HAS_MORE), 10, 30),
        makeMm6Frame(131072, 0, 20, 0),
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

// MM6's SFT records pack the flag bits into TWO bytes (MM7 widened the field to 4), so every field
// after them sits 2 bytes earlier than in MM7: lightRadius, paletteId, paletteIndex, then BOTH the
// per-frame time and the group total time - MM6 does store the total, on the group's first frame.
// Reading the flags as 4 bytes shifted all of these: frameLength picked up the total time (which is
// 0 on every chained frame, freezing every sprite animation on its first frame), glowRadius picked
// up the palette id (hanging a spurious light on every billboard), paletteId picked up the
// always-zero paletteIndex, and a nonzero light radius leaked into the flag word's high bits
// (SPRITE_FRAME_GLOWING / SPRITE_FRAME_TRANSPARENT are up there).
GAME_TEST(SpriteFrameTableMm6, DeserializeReadsMm6FieldOffsets) {
    // The first two frames of MM6's bat-A walk group (dsft.bin records 1525-1526), byte-exact,
    // plus a self-lit frame with a real light radius.
    std::string bytes;
    uint32_t frameCount = 3;
    uint32_t eframeCount = 0;
    bytes.append(reinterpret_cast<const char *>(&frameCount), sizeof(frameCount));
    bytes.append(reinterpret_cast<const char *>(&eframeCount), sizeof(eframeCount));
    bytes += makeMm6FrameBytes("bAwlka", "bhwlka", 32768, 0xE005, 0, 159, 0, 2, 12);
    bytes += makeMm6FrameBytes("", "bhwlkb", 32768, 0xE001, 0, 159, 0, 2, 0);
    bytes += makeMm6FrameBytes("torch", "torch1", 65536, 0x0016, 300, 3, 0, 5, 5);

    BlobInputStream input(Blob::fromString(std::move(bytes)));
    SpriteFrameTable_MM6 table;
    deserialize(input, &table);

    SpriteFrameTable dst;
    reconstruct(table, &dst);

    ASSERT_EQ(dst.pSpriteSFrames.size(), 3u);
    const SpriteFrame &first = dst.pSpriteSFrames[0];
    const SpriteFrame &chained = dst.pSpriteSFrames[1];
    const SpriteFrame &lit = dst.pSpriteSFrames[2];

    EXPECT_EQ(first.flags, SPRITE_FRAME_HAS_MORE | SPRITE_FRAME_FIRST | SPRITE_FRAME_MIRROR_5 |
                               SPRITE_FRAME_MIRROR_6 | SPRITE_FRAME_MIRROR_7);
    EXPECT_EQ(first.paletteId, 159);
    EXPECT_EQ(first.glowRadius, 0);
    // Frame times are 1/16s units in the file, *8 -> ticks.
    EXPECT_EQ(first.frameLength, Duration::fromTicks(2 * 8));
    EXPECT_EQ(first.animationLength, Duration::fromTicks(12 * 8));

    // The chained frame carries its own frame time - GetFrame() advances the walk cycle through it.
    EXPECT_EQ(chained.frameLength, Duration::fromTicks(2 * 8));
    EXPECT_EQ(chained.paletteId, 159);

    // A real light radius lands in glowRadius and doesn't leak into the flag word.
    EXPECT_EQ(lit.flags, SPRITE_FRAME_LIT | SPRITE_FRAME_FIRST | SPRITE_FRAME_IMAGE1);
    EXPECT_EQ(lit.glowRadius, 300);
    EXPECT_EQ(lit.paletteId, 3);
    EXPECT_EQ(lit.frameLength, Duration::fromTicks(5 * 8));
    EXPECT_EQ(lit.animationLength, Duration::fromTicks(5 * 8));
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

// MM6 monsters have no tint-color field (an MM7 addition), so reconstruct must default it to no-tint.
// MM7's own dmonlist.bin stores 0x00000000 for every untinted monster, and no record has a nonzero
// alpha byte. A nonzero alpha routes outdoor actor billboards into the additive-blend path
// (TransformBillboard's opaquetest), which drew all MM6 actors see-through.
GAME_TEST(MonsterListMm6, ReconstructDefaultsTintColorToNoTint) {
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
    EXPECT_EQ(dst.tintColor.r, 0);
    EXPECT_EQ(dst.tintColor.g, 0);
    EXPECT_EQ(dst.tintColor.b, 0);
    EXPECT_EQ(dst.tintColor.a, 0);
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

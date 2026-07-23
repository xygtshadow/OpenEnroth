#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "Testing/Game/GameTest.h"

#include "Application/Paths/GameVersion.h"

#include "Engine/Evt/EvtProgram.h"
#include "Engine/Evt/EvtEnums.h"
#include "Engine/Objects/ItemEnums.h"

#include "Utility/Memory/Blob.h"

// Builds a raw .evt blob from records. Each record is [sizeByte][eventId:u16-le][step:u8][opcode:u8][operands...],
// where sizeByte = (total record length) - 1, matching EvtProgram::load's `size = *pos + 1` framing.
static Blob makeEvtBlob(const std::vector<std::vector<uint8_t>> &records) {
    std::string bytes;
    for (const std::vector<uint8_t> &rec : records)
        for (uint8_t b : rec)
            bytes += static_cast<char>(b);
    return Blob::fromString(std::move(bytes));
}

// MM6's event bytecode differs from MM7's: the variable-family opcodes (Compare/Add/Subtract/Set) store the
// variable id as a uint8 rather than MM7's uint16 (and MM6's variable numbering diverges from MM7's above 0x2D),
// some opcode bytes mean different things (12, 24), and a few opcodes don't exist in MM7 at all (20, 27, 28).
// EvtProgram::load parses MM6 bytecode into the engine's version-independent representation.
GAME_TEST(EvtProgramMm6, Parses) {
    Blob blob = makeEvtBlob({
        // A real MM6 global.evt Compare record: size=11, eventId=1, step=1, opcode=14 (Compare), then a uint8
        // variable id (0x11 = party has item) + uint32 value (0x000001f9) + uint8 target_step (0x04).
        {0x0a, 0x01, 0x00, 0x01, 0x0e, 0x11, 0xf9, 0x01, 0x00, 0x00, 0x04},
        // A real MM6 oute3.evt Set record: eventId=2, step=6, opcode=18 (Set), variable id 0x69, value 1.
        // MM6 0x69 is MapVar0; in MM7's numbering (which the engine uses) 0x69 is the Cursed condition.
        {0x09, 0x02, 0x00, 0x06, 0x12, 0x69, 0x01, 0x00, 0x00, 0x00},
        // A real MM6 oute3.evt SetTextureOutdoors record: eventId=3, step=5, opcode=12 (EVENT_ShowMovie in MM7),
        // model 84, face 42, texture "T1swBu".
        {0x13, 0x03, 0x00, 0x05, 0x0c, 0x54, 0x00, 0x00, 0x00, 0x2a, 0x00, 0x00, 0x00, 0x54, 0x31, 0x73, 0x77, 0x42, 0x75, 0x00},
    });

    EvtProgram program = EvtProgram::load(blob, GAME_VERSION_MM6);

    ASSERT_TRUE(program.hasEvent(1));
    const EvtInstruction &compare = program.function(1)[0];
    EXPECT_EQ(compare.opcode, EVENT_Compare);
    EXPECT_EQ(compare.data.variable_descr.type, VAR_PlayerItemInHands);
    EXPECT_EQ(compare.data.variable_descr.value, 505);
    EXPECT_EQ(compare.target_step, 4);

    const EvtInstruction &set = program.function(2)[0];
    EXPECT_EQ(set.opcode, EVENT_Set);
    EXPECT_EQ(set.data.variable_descr.type, VAR_MapPersistentVariable_0);
    EXPECT_EQ(set.data.variable_descr.value, 1);

    const EvtInstruction &setTexture = program.function(3)[0];
    EXPECT_EQ(setTexture.opcode, EVENT_SetTextureOutdoors);
    EXPECT_EQ(setTexture.data.outdoor_texture_descr.model, 84);
    EXPECT_EQ(setTexture.data.outdoor_texture_descr.face, 42);
    EXPECT_EQ(setTexture.str, "T1swBu");
}

// MM6 numbers damage types Phys=0, Magic=1, Fire=2, Elec=3, Cold=4, Poison=5, Energy=6 - a different coding
// from the engine's MM7-shaped DamageType (Fire=0, Air=1, Water=2, Earth=3, Physical=4, Magic=5). ReceiveDamage
// records store the raw MM6 byte, so parsing must translate it or every event-driven trap resolves against the
// wrong resistance (e.g. pyramid.blv's unresistable physical trap would be checked against fire resistance).
GAME_TEST(EvtProgramMm6, ReceiveDamageTypes) {
    // ReceiveDamage records: size=11, opcode=9, then [who:u8][damageType:u8][damage:u32-le].
    // One record per MM6 damage code 0..6, eventId = code + 1.
    std::vector<std::vector<uint8_t>> records;
    for (uint8_t code = 0; code <= 6; code++)
        records.push_back({0x0a, static_cast<uint8_t>(code + 1), 0x00, 0x01, 0x09, 0x05, code, 0x05, 0x00, 0x00, 0x00});

    EvtProgram program = EvtProgram::load(makeEvtBlob(records), GAME_VERSION_MM6);

    static constexpr std::array<DamageType, 7> expected = {
        DAMAGE_PHYSICAL, DAMAGE_MAGIC, DAMAGE_FIRE, DAMAGE_AIR, DAMAGE_WATER, DAMAGE_EARTH, DAMAGE_ENERGY};
    for (int code = 0; code <= 6; code++) {
        const EvtInstruction &damage = program.function(code + 1)[0];
        EXPECT_EQ(damage.opcode, EVENT_ReceiveDamage);
        EXPECT_EQ(damage.data.damage_descr.damage_type, expected[code]) << "MM6 damage code " << code;
        EXPECT_EQ(damage.data.damage_descr.damage, 5);
    }
}

// MM7 ReceiveDamage records already store the engine's DamageType numbering; the byte must stay untranslated.
GAME_TEST(EvtProgramMm7, ReceiveDamageTypeIsRaw) {
    Blob blob = makeEvtBlob({
        // ReceiveDamage: eventId=1, who=CHOOSE_PARTY (5), damage type 4 (DAMAGE_PHYSICAL), 5 damage.
        {0x0a, 0x01, 0x00, 0x01, 0x09, 0x05, 0x04, 0x05, 0x00, 0x00, 0x00},
    });

    EvtProgram program = EvtProgram::load(blob, GAME_VERSION_MM7);

    const EvtInstruction &damage = program.function(1)[0];
    EXPECT_EQ(damage.opcode, EVENT_ReceiveDamage);
    EXPECT_EQ(damage.data.damage_descr.damage_type, DAMAGE_PHYSICAL);
    EXPECT_EQ(damage.data.damage_descr.damage, 5);
}

// Guards the MM7 parse path through the version-parameter refactor: a valid MM7 record must still parse.
GAME_TEST(EvtProgramMm7, Parses) {
    // EVENT_Exit record: size=6, eventId=1, step=1, opcode=1 (Exit), then the single always-0 trailing byte.
    Blob blob = makeEvtBlob({
        {0x05, 0x01, 0x00, 0x01, 0x01, 0x00},
    });

    EvtProgram program = EvtProgram::load(blob, GAME_VERSION_MM7);

    ASSERT_TRUE(program.hasEvent(1));
    ASSERT_EQ(program.function(1).size(), 1u);
    EXPECT_EQ(program.function(1)[0].opcode, EVENT_Exit);
}

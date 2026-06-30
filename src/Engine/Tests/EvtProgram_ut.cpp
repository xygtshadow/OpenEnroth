#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "Testing/Game/GameTest.h"

#include "Application/Paths/GameVersion.h"

#include "Engine/Evt/EvtProgram.h"
#include "Engine/Evt/EvtEnums.h"

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
// EvtVariable type as a uint8 rather than MM7's uint16, so an MM6 record is one byte shorter than MM7's parser
// expects and parsing it as MM7 underflows the binary stream (and PressAnyKey trips an assert). Until the MM6
// event model exists (docs/pending/mm6-events-and-overlays.md), EvtProgram::load boots past MM6 data: it logs
// and returns an empty program so engine bring-up proceeds.
GAME_TEST(EvtProgramMm6, BootsPast) {
    // A real MM6 global.evt Compare record: size=11, eventId=1, step=1, opcode=14 (Compare), then a uint8
    // variable type (0x11) + uint32 value (0x000001f9) + uint8 target_step (0x04). MM7's parser reads the type
    // as a uint16 and runs off the end of the (correctly sized) record, throwing a binary-stream underflow.
    Blob blob = makeEvtBlob({
        {0x0a, 0x01, 0x00, 0x01, 0x0e, 0x11, 0xf9, 0x01, 0x00, 0x00, 0x04},
    });

    EvtProgram program;
    EXPECT_NO_THROW(program = EvtProgram::load(blob, GAME_VERSION_MM6));
    EXPECT_FALSE(program.hasEvent(1));
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

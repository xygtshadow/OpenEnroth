#include "EvtProgram.h"

#include <ranges>
#include <tuple>
#include <vector>
#include <utility>
#include <string>

#include "Engine/Tables/HouseTable.h"
#include "Engine/Party.h"
#include "Engine/Engine.h"

#include "Library/Logger/Logger.h"

#include "Library/Binary/BinarySerialization.h"

#include "Utility/Memory/Blob.h"
#include "Utility/Streams/MemoryInputStream.h"
#include "Utility/MapAccess.h"
#include "Utility/Exception.h"

EvtProgram EvtProgram::load(const Blob &rawData, GameVersion version) {
    EvtProgram result;

    if (version == GAME_VERSION_MM6) {
        // MM6's event bytecode is laid out differently from MM7's, so parsing it with MM7's per-opcode field
        // widths corrupts the stream. The variable-family opcodes (Compare/Add/Subtract/Set) store the
        // EvtVariable type as a uint8 rather than MM7's uint16, making each such record one byte shorter than
        // EvtInstruction::parse expects (it then underflows reading the next field); MM6 also uses opcodes such
        // as PressAnyKey with operands MM7's parser treats as a read-nothing no-op (tripping an assert). Building
        // the MM6 event VM (opcode set, EvtVariable mapping, SetSnow/seasons, overlays) is a separate task (see
        // docs/pending/mm6-events-and-overlays.md); for now MM6 events are deliberately left unloaded so engine
        // bring-up can proceed. Global scripting and per-map local events will not fire for MM6 until then.
        logger->warning("MM6 event bytecode parsing is not implemented yet - events will not fire. MM6's .evt "
                        "opcode/operand layout differs from MM7's (e.g. the Compare/Add/Subtract/Set variable "
                        "type is a uint8, not a uint16), so it needs the MM6 event model.");
        return result;
    }

    const uint8_t *pos = reinterpret_cast<const uint8_t *>(rawData.data());
    const uint8_t *const end = pos + rawData.size();
    while (pos < end) {
        size_t size = *pos + 1; // +1 because we also count the size byte.
        if (size < 5)
            throw Exception("Invalid evt record size: expected at least {}, got {}", 5, size);
        if (pos + size > end)
            throw Exception("Encountered corrupted evt binary data");
        MemoryInputStream stream(pos + 1, size - 1); // offset is 1 because we are skipping the `size` byte - it was already read
        uint16_t eventId = fromStream<uint16_t>(stream);
        result.add(eventId, EvtInstruction::parse(stream, size));
        pos += size;
    }

    return result;
}

void EvtProgram::add(int eventId, EvtInstruction ir) {
    _eventsById[eventId].push_back(std::move(ir));
}

void EvtProgram::clear() {
    _eventsById.clear();
}

const EvtInstruction &EvtProgram::instruction(int eventId, int step) const {
    for (const EvtInstruction &ir : function(eventId))
        if (ir.step == step)
            return ir;
    throw Exception("Event {}:{} not found", eventId, step);
}

const std::vector<EvtInstruction>& EvtProgram::function(int eventId) const {
    const auto *result = valuePtr(_eventsById, eventId);
    if (!result)
        throw Exception("Event {} not found", eventId);
    return *result;
}

std::vector<EventTrigger> EvtProgram::enumerateTriggers(EvtOpcode triggerType) {
    std::vector<EventTrigger> result;

    for (const auto &[id, events] : _eventsById) {
        for (const EvtInstruction &event : events) {
            // As retarded as it might look, there are scripts that have THREE EVENT_OnLongTimer instructions.
            // Thus, we might have several event triggers for the same event id.
            if (event.opcode == triggerType) {
                EventTrigger trigger;
                trigger.eventId = id;
                trigger.eventStep = event.step;
                result.push_back(trigger);
            }
        }
    }

    // Need to sort the result so that the order doesn't depend on how the events were laid out in the hash map.
    std::ranges::sort(result, std::less(), [] (const EventTrigger &value) { return std::tie(value.eventId, value.eventStep); }); // NOLINT
    return result;
}

bool EvtProgram::hasHint(int eventId) const {
    const auto* events = valuePtr(_eventsById, eventId);
    if (!events || events->size() < 2)
        return false;

    return (*events)[0].opcode == EVENT_MouseOver && (*events)[1].opcode == EVENT_Exit;
}

std::string EvtProgram::hint(int eventId) const {
    std::string result;
    bool mouseOverFound = false;

    const auto* events = valuePtr(_eventsById, eventId);
    if (!events) { // no entry in .evt file
        return result;
    }

    for (const EvtInstruction &ir : *events) {
        if (ir.opcode == EVENT_MouseOver) {
            mouseOverFound = true;
            if (ir.data.text_id < engine->_levelStrings.size()) {
                result = engine->_levelStrings[ir.data.text_id];
            }
        }
        if (mouseOverFound && ir.opcode == EVENT_SpeakInHouse) {
            if (houseTable.indices().contains(ir.data.house_id)) {
                result = houseTable[ir.data.house_id].name;
            }
            break;
        }
    }

    return result;
}

void EvtProgram::dump(int eventId) const {
    const auto *events = valuePtr(_eventsById, eventId);
    if (events) {
        logger->trace("Event: {}", eventId);
        for (const EvtInstruction &ir : *events) {
            logger->trace("{}", ir.toString());
        }
    } else {
        logger->trace("Event {} not found", eventId);
    }
}

void EvtProgram::dumpAll() const {
    for (const auto &[id, _] : _eventsById) {
        dump(id);
    }
}

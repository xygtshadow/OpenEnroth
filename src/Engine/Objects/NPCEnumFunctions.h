#pragma once

#include <utility>

#include "NPCEnums.h"

#include "Utility/Segment.h"

inline Segment<MerchantPhrase> allMerchantPhrases() {
    return {MERCHANT_PHRASE_NOT_ENOUGH_GOLD, MERCAHNT_PHRASE_STOLEN_ITEM};
}

inline Segment<NpcProfession> allNpcProfessions() {
    return {NPC_PROFESSION_FIRST, NPC_PROFESSION_LAST};
}

/**
 * Translates an MM6 npcprof.txt / npcdata.txt profession id into the engine's `NpcProfession` value.
 *
 * MM6 ids 1-22, 25-48 and 50-51 name the same professions as MM7's and map onto themselves; the rest
 * (23, 24, 49, 52-77) are MM6-only professions living past MM7's id range in the engine enum. MM6 id 69
 * ("Hunter") maps to `Hunter2`, not MM7's `Hunter` - MM6's is a benefit-less commoner trade.
 *
 * @param id                            Profession id from an MM6 data table.
 * @return                              Corresponding `NpcProfession`, or `NoProfession` for ids outside
 *                                      MM6's 1-77 range.
 */
inline NpcProfession npcProfessionFromMm6Id(int id) {
    switch (id) {
        case 23: return Counselor;
        case 24: return Barrister;
        case 49: return Negotiator;
        default:
            if (id >= 1 && id <= 51)
                return static_cast<NpcProfession>(id); // Shared with MM7: Smith(1)..Scout(22), Tinker(25)..Gypsy(48), Duper(50), Burglar(51).
            if (id >= 52 && id <= 77)
                return static_cast<NpcProfession>(id - 52 + std::to_underlying(Peasant)); // MM6-only block: Peasant(52)..Child(77), contiguous.
            return NoProfession;
    }
}

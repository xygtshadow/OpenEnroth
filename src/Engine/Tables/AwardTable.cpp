#include "AwardTable.h"

#include <array>
#include <string>
#include <utility>

#include "Library/Serialization/Serialization.h"

#include "Utility/Memory/Blob.h"
#include "Utility/String/Split.h"
#include "Utility/String/Transformations.h"

IndexedArray<AwardData, AWARD_FIRST, AWARD_LAST> pAwards;

// MM6 has no priority column; the character sheet colors awards by id band instead
// (MM6.EXE 0x4164a4: <8, <32, <37, <64, <81, rest), and its bands are monotonic in id,
// so storing the band as the priority also keeps the priority sort in id order.
static int mm6AwardPriority(int awardId) {
    if (awardId < 8)
        return 0;
    if (awardId < 32)
        return 1;
    if (awardId < 37)
        return 2;
    if (awardId < 64)
        return 3;
    if (awardId < 81)
        return 4;
    return 5;
}

void initializeAwards(const Blob &awards, GameVersion version) {
    // awards.txt table structure: index | text (localized) | priority (MM7; absent in MM6).
    for (std::string_view line : split(awards.str()).by("\r\n").drop(1).skip("")) {
        std::array<std::string_view, 3> chunks = split(line).by('\t');

        if (version == GAME_VERSION_MM6) {
            if (chunks[1].empty())
                continue; // Rows 88+ carry just the index.

            AwardId awardId = static_cast<AwardId>(fromString<int>(chunks[0]));
            pAwards[awardId].pText = removeQuotes(chunks[1]);
            pAwards[awardId].uPriority = mm6AwardPriority(std::to_underlying(awardId));
            continue;
        }

        if (chunks[2].empty())
            continue; // Truncated lines with just the index exist in the file.

        AwardId awardId = static_cast<AwardId>(fromString<int>(chunks[0]));
        pAwards[awardId].pText = removeQuotes(chunks[1]);
        pAwards[awardId].uPriority = fromString<int>(chunks[2]);
    }
}

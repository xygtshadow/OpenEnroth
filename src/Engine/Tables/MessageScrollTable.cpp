#include "MessageScrollTable.h"

#include <array>
#include <map>
#include <string>

#include "Library/Serialization/Serialization.h"

#include "Utility/Memory/Blob.h"
#include "Utility/String/Split.h"
#include "Utility/String/Transformations.h"

std::map<ItemId, std::string> pMessageScrolls;

void initializeMessageScrolls(const Blob &scrolls) {
    // scroll.txt table structure: item id | message text (localized) | location/notes (not used).
    // The table is keyed by item id and is structurally identical in both games - only the id range
    // differs (MM6: 500-581, MM7: 700-781). Line breaks embedded in quoted message cells are bare
    // '\n', while rows terminate with "\r\n", so every row is a single "\r\n"-line.
    pMessageScrolls.clear();
    for (std::string_view line : split(scrolls.str()).by("\r\n").drop(1).skip("")) {
        std::array<std::string_view, 2> tokens = split(line).by('\t');
        if (tokens[0].empty())
            continue; // Skip tab-only trailing lines.

        ItemId i = static_cast<ItemId>(fromString<int>(tokens[0]));
        pMessageScrolls[i] = removeQuotes(tokens[1]);
    }
}

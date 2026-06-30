#include "MessageScrollTable.h"

#include <array>
#include <string>

#include "Library/Logger/Logger.h"
#include "Library/Serialization/Serialization.h"

#include "Utility/Memory/Blob.h"
#include "Utility/String/Split.h"
#include "Utility/String/Transformations.h"

IndexedArray<std::string, ITEM_FIRST_MESSAGE_SCROLL, ITEM_LAST_MESSAGE_SCROLL> pMessageScrolls;

void initializeMessageScrolls(const Blob &scrolls, GameVersion version) {
    if (version == GAME_VERSION_MM6) {
        // MM6's scroll.txt uses item ids 500-581, whereas MM7's run 700-781 (both 82 entries). The ItemId enum
        // is MM7-shaped, so pMessageScrolls is indexed [ITEM_FIRST_MESSAGE_SCROLL(700), ITEM_LAST_MESSAGE_SCROLL(781)]
        // and MM6's first row (id 500) falls outside that range. The +200 offset lines up positionally, but the
        // scroll text is MM6-specific content (different dungeons/story), so storing it under MM7's
        // ITEM_MESSAGE_FROM_ERATHIA-style ids is only meaningful once MM6's item/scroll model exists. Modelling
        // that is a separate task (see docs/pending/mm6-item-model.md); for now MM6's message scrolls are
        // deliberately left unpopulated so engine bring-up can proceed.
        logger->warning("MM6 scroll.txt parsing is not implemented yet - message scrolls will be empty. "
                        "MM6's scroll item ids (500-581) fall outside the MM7-shaped pMessageScrolls range and "
                        "its scroll content is MM6-specific, so it needs the MM6 item model.");
        return;
    }

    // scroll.txt table structure: item index | message text (localized) | scroll title (localized, not used) | (empty).
    for (std::string_view line : split(scrolls.str()).by("\r\n").drop(1).skip("")) {
        std::array<std::string_view, 2> tokens = split(line).by('\t');
        if (tokens[0].empty())
            continue; // Skip tab-only trailing lines.

        ItemId i = static_cast<ItemId>(fromString<int>(tokens[0]));
        pMessageScrolls[i] = removeQuotes(tokens[1]);
    }
}

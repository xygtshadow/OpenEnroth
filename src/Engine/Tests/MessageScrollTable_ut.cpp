#include <map>
#include <string>
#include <utility>
#include <vector>

#include "Testing/Game/GameTest.h"

#include "Engine/Tables/MessageScrollTable.h"
#include "Engine/Objects/ItemEnums.h"

#include "Utility/Memory/Blob.h"

// Joins cells with '\t' and rows with '\r\n' to mimic the on-disk scroll.txt table format that
// initializeMessageScrolls parses (it drops the first header row, then splits the rest by "\r\n").
static Blob makeScrollsBlob(const std::vector<std::vector<std::string>> &rows) {
    std::string bytes;
    for (const std::vector<std::string> &row : rows) {
        for (size_t i = 0; i < row.size(); i++) {
            if (i != 0)
                bytes += '\t';
            bytes += row[i];
        }
        bytes += "\r\n";
    }
    return Blob::fromString(std::move(bytes));
}

// scroll.txt is keyed by item id and structurally identical in both games - only the id range
// differs (MM6: 500-581, MM7: 700-781), which is why pMessageScrolls is a map keyed by item id
// rather than an IndexedArray over the MM7 id range (MM6's id 500 used to fall outside it and
// aborted bring-up with an "array subscript out of range" _STL_VERIFY).
//
// initializeMessageScrolls rebuilds the file-scope global pMessageScrolls from scratch, so these
// tests snapshot and restore the whole map to stay hermetic for other tests sharing the process.
GAME_TEST(MessageScrollsMm6, Parses) {
    std::map<ItemId, std::string> saved = std::move(pMessageScrolls);

    Blob blob = makeScrollsBlob({
        {"Item#", "message text", " dungeon #", "Notes"},     // Header.
        {"500", "III = 16 & IV = 4", "D1", "Button combo notation."},
        {"505", "\"My Dear Sulman, you have done well\"", "T7", ""},
    });

    initializeMessageScrolls(blob);

    EXPECT_EQ(pMessageScrolls[ItemId(500)], "III = 16 & IV = 4");
    EXPECT_EQ(pMessageScrolls[ItemId(505)], "My Dear Sulman, you have done well"); // Quotes stripped.

    pMessageScrolls = std::move(saved);
}

GAME_TEST(MessageScrollsMm7, Parses) {
    std::map<ItemId, std::string> saved = std::move(pMessageScrolls);

    Blob blob = makeScrollsBlob({
        {"#", "message text", "Location", "Item Name"},       // Header.
        {"700", "A Message From Erathia", "Castle Harmondale", "Message From Erathia"},
        {"701", "An encrypted cipher", "Free Haven", "Message Cipher"},
    });

    initializeMessageScrolls(blob);

    EXPECT_EQ(pMessageScrolls[ITEM_MESSAGE_FROM_ERATHIA], "A Message From Erathia");
    EXPECT_EQ(pMessageScrolls[ITEM_MESSAGE_CIPHER], "An encrypted cipher");

    pMessageScrolls = std::move(saved);
}

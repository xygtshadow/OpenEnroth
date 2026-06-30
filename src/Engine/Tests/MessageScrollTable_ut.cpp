#include <string>
#include <utility>
#include <vector>

#include "Testing/Game/GameTest.h"

#include "Application/Paths/GameVersion.h"

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

// MM6's scroll.txt uses item ids 500-581, whereas MM7's run 700-781 (both 82 entries). The ItemId enum is
// MM7-shaped, so pMessageScrolls is indexed [ITEM_FIRST_MESSAGE_SCROLL(700), ITEM_LAST_MESSAGE_SCROLL(781)]
// and MM6's first row (id 500) falls outside that range, which previously aborted bring-up with an
// "array subscript out of range" _STL_VERIFY. For MM6 the parser logs and returns, leaving pMessageScrolls
// empty so engine bring-up proceeds; the faithful MM6 scroll model is deferred (see docs/pending/mm6-item-model.md).
GAME_TEST(MessageScrollsMm6, BootsPast) {
    // id 500 is below the MM7-shaped range [700, 781]; parsing it as an ItemId index would crash.
    Blob blob = makeScrollsBlob({
        {"#", "message text", "dungeon #", "Notes"},          // Header.
        {"500", "Some MM6-specific scroll text", "12", ""},
    });

    EXPECT_NO_THROW(initializeMessageScrolls(blob, GAME_VERSION_MM6));
}

// Guards the MM7 parse path through the version-parameter refactor: MM7's scroll ids are in range, so the
// table must still populate as before.
GAME_TEST(MessageScrollsMm7, Parses) {
    // initializeMessageScrolls writes the file-scope global pMessageScrolls; snapshot and restore the slots
    // we touch so the test stays hermetic for other tests sharing the process.
    std::string saved700 = pMessageScrolls[ITEM_MESSAGE_FROM_ERATHIA];
    std::string saved701 = pMessageScrolls[ITEM_MESSAGE_CIPHER];

    Blob blob = makeScrollsBlob({
        {"#", "message text", "Location", "Item Name"},       // Header.
        {"700", "A Message From Erathia", "Castle Harmondale", "Message From Erathia"},
        {"701", "An encrypted cipher", "Free Haven", "Message Cipher"},
    });

    EXPECT_NO_THROW(initializeMessageScrolls(blob, GAME_VERSION_MM7));

    EXPECT_EQ(pMessageScrolls[ITEM_MESSAGE_FROM_ERATHIA], "A Message From Erathia");
    EXPECT_EQ(pMessageScrolls[ITEM_MESSAGE_CIPHER], "An encrypted cipher");

    pMessageScrolls[ITEM_MESSAGE_FROM_ERATHIA] = saved700;
    pMessageScrolls[ITEM_MESSAGE_CIPHER] = saved701;
}

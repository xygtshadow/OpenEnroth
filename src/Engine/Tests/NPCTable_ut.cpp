#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Testing/Game/GameTest.h"

#include "Application/Paths/GameVersion.h"

#include "Engine/Tables/NPCTable.h"

#include "Utility/Memory/Blob.h"

// Joins cells with '\t' and rows with '\r\n' to mimic the on-disk npcdata.txt table format that
// InitializeNPCData parses (it drops the first 2 header rows, then splits the rest by "\r\n").
static Blob makeNpcDataBlob(const std::vector<std::vector<std::string>> &rows) {
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

// MM6's npcdata.txt has 14 columns where MM7 has 17: it carries State/Fame/Rep (cols 3-5) instead of
// MM7's group columns, has no greeting-index column, only 3 event columns (A/B/C, cols 10-12), a numeric
// 0/1 "Join" flag at col 8, and free-form text from col 13 on ("Notes"). Parsing MM6 with MM7's columns
// throws ("'gives money...' is not a number") when the numeric read of dialogue_4 (tokens[13]) lands on
// the text Notes column. The MM6 parse path reads the join flag from col 8, maps events A/B/C to
// dialogue 1/2/3, and leaves greetingIndex / dialogue 4-6 at their defaults.
GAME_TEST(NPCTableMm6, ParsesMm6Layout) {
    Blob blob = makeNpcDataBlob({
        {"NPC Data (Special)"},                                                          // Header 1.
        {"#", "Name", "Pic", "State", "Fame", "Rep", "2D Location", " 1 - 76", "Y / N",  // Header 2.
         "Y / N", "# A", "# B", "# C", "Notes"},
        // index | name | pic | state | fame | rep | house | profession | join | news | A | B | C | notes.
        {"1", "Andover Potbello", "81", "0", "0", "0", "92", "74", "1", "0", "1", "296", "0",
         "gives money for the sixth letter and retrieve candelabra quest"},
        {"2", "Maria", "126", "0", "0", "0", "92", "48", "0", "1", "8", "0", "0",
         "gives clue for marketing promotion."},
    });

    auto stats = std::make_unique<NPCStats>();
    EXPECT_NO_THROW(stats->InitializeNPCData(blob, GAME_VERSION_MM6));

    const NPCData &npc1 = stats->pOriginalNPCData[1];
    EXPECT_EQ(npc1.name, "Andover Potbello");
    EXPECT_EQ(npc1.portraitId, 81u);
    EXPECT_EQ(static_cast<int>(npc1.house), 92);
    EXPECT_EQ(static_cast<int>(npc1.profession), 74);
    EXPECT_TRUE(npc1.canJoin);                 // MM6 join flag from col 8 (numeric 1).
    EXPECT_EQ(npc1.greetingIndex, 0);          // MM6 has no greeting-index column -> default.
    EXPECT_EQ(npc1.dialogue_1_evt_id, 1u);     // Event #A.
    EXPECT_EQ(npc1.dialogue_2_evt_id, 296u);   // Event #B.
    EXPECT_EQ(npc1.dialogue_3_evt_id, 0u);     // Event #C.
    EXPECT_EQ(npc1.dialogue_4_evt_id, 0u);     // No event D/E/F columns -> default (the Notes column at
    EXPECT_EQ(npc1.dialogue_5_evt_id, 0u);     // col 13 is NOT parsed as a number).
    EXPECT_EQ(npc1.dialogue_6_evt_id, 0u);

    const NPCData &npc2 = stats->pOriginalNPCData[2];
    EXPECT_EQ(npc2.name, "Maria");
    EXPECT_FALSE(npc2.canJoin);                // col 8 = 0.
    EXPECT_EQ(npc2.dialogue_1_evt_id, 8u);
}

// MM6's npcnews.txt is a different feature from MM7's: it is per-map "Regional News" (279 rows of
// index | Map | Topic | News Text) preceded by TWO header rows ("Regional News" then "# Map Topic News
// Text"), whereas MM7 is a 52-entry NPC catch-phrase list (index | text | notes) with a single header.
// Parsing MM6 with the MM7 layout throws ("'#' is not a number") because the parser drops only one
// header row, so tokens[0] lands on the literal '#'; even past that, the 279 indices would overflow
// pCatchPhrases (size 52) and tokens[1] is a numeric Map id, not phrase text. MM6 is therefore booted
// past (left unpopulated) until a dedicated regional-news model is built - same interim treatment as
// spells.txt / items.txt.
GAME_TEST(NPCTableNewsMm6, BootsPastRegionalNews) {
    Blob blob = makeNpcDataBlob({
        {"Regional News", "", "", ""},               // Header 1.
        {"#", "Map", "Topic", "News Text"},          // Header 2.
        {"1", "40", "Goblinwatch", "The keep on the hill is now home to a pack of goblins."},
        {"2", "40", "Baa Temple", "A new Temple dedicated to Baa lies west of here."},
    });

    auto stats = std::make_unique<NPCStats>();
    EXPECT_NO_THROW(stats->InitializeNPCNews(blob, GAME_VERSION_MM6));

    // Boot-past leaves the MM7-shaped catch-phrase array at its defaults.
    EXPECT_TRUE(stats->pCatchPhrases[0].empty());
    EXPECT_TRUE(stats->pCatchPhrases[1].empty());
}

// Guards the MM7 npcnews parse path through the version-parameter refactor: single header row, then
// index | text | notes, stored as pCatchPhrases[index] = text.
GAME_TEST(NPCTableNewsMm7, ParsesCatchPhrases) {
    Blob blob = makeNpcDataBlob({
        {"News # ", "Text", "Notes"},                    // Single header row.
        {"0", "Group 0.", "Default change me text"},
        {"1", "Good luck in the contest!", "Peasant"},
    });

    auto stats = std::make_unique<NPCStats>();
    EXPECT_NO_THROW(stats->InitializeNPCNews(blob, GAME_VERSION_MM7));

    EXPECT_EQ(stats->pCatchPhrases[0], "Group 0.");
    EXPECT_EQ(stats->pCatchPhrases[1], "Good luck in the contest!");
}

// MM6's npctext.txt has TWO header rows ("Text Number From NPC Events.Doc" then "# Text Notes") where
// MM7 has ONE. The columns otherwise correspond (index | text), so the only difference is the header
// count: parsing MM6 with MM7's single .drop(1) leaves the "# Text Notes" row as data and throws ("'#'
// is not a number"). InitializeNPCText writes pNPCTopics[index - 1].pText.
GAME_TEST(NPCTableTextMm6, DropsTwoHeaderRows) {
    Blob blob = makeNpcDataBlob({
        {"Text Number From NPC Events.Doc", "", ""},     // Header 1.
        {"#", "Text", "Notes"},                          // Header 2.
        {"1", "Here, take this money.", ""},
        {"2", "You got your gold!", ""},
    });

    NPCTopic saved0 = pNPCTopics[0];
    NPCTopic saved1 = pNPCTopics[1];

    auto stats = std::make_unique<NPCStats>();
    EXPECT_NO_THROW(stats->InitializeNPCText(blob, GAME_VERSION_MM6));

    EXPECT_EQ(pNPCTopics[0].pText, "Here, take this money.");  // index 1 -> slot 0.
    EXPECT_EQ(pNPCTopics[1].pText, "You got your gold!");      // index 2 -> slot 1.

    pNPCTopics[0] = saved0;
    pNPCTopics[1] = saved1;
}

// Guards the MM7 npctext parse path through the version-parameter refactor: single header row.
GAME_TEST(NPCTableTextMm7, DropsOneHeaderRow) {
    Blob blob = makeNpcDataBlob({
        {"#", "Text", "Notes", "Owner"},                 // Single header row.
        {"1", "Emerald Island is southeast of Erathia.", "", ""},
    });

    NPCTopic saved0 = pNPCTopics[0];

    auto stats = std::make_unique<NPCStats>();
    EXPECT_NO_THROW(stats->InitializeNPCText(blob, GAME_VERSION_MM7));

    EXPECT_EQ(pNPCTopics[0].pText, "Emerald Island is southeast of Erathia.");

    pNPCTopics[0] = saved0;
}

// MM6's npctopic.txt likewise has TWO header rows ("Text Number From NPC Events.Doc ... Notes" then
// "# Topic") where MM7 has ONE. Columns correspond (index | topic); InitializeNPCTopics writes
// pNPCTopics[index].pTopic (0-based, no -1). MM6 with .drop(1) throws on the "# Topic" row.
GAME_TEST(NPCTableTopicMm6, DropsTwoHeaderRows) {
    Blob blob = makeNpcDataBlob({
        {"Text Number From NPC Events.Doc", "", "Notes"},  // Header 1.
        {"#", "Topic", ""},                                // Header 2.
        {"1", "The Letter", ""},
        {"3", "Goblinwatch", ""},
    });

    NPCTopic saved1 = pNPCTopics[1];
    NPCTopic saved3 = pNPCTopics[3];

    auto stats = std::make_unique<NPCStats>();
    EXPECT_NO_THROW(stats->InitializeNPCTopics(blob, GAME_VERSION_MM6));

    EXPECT_EQ(pNPCTopics[1].pTopic, "The Letter");    // index 1 -> slot 1.
    EXPECT_EQ(pNPCTopics[3].pTopic, "Goblinwatch");   // index 3 -> slot 3.

    pNPCTopics[1] = saved1;
    pNPCTopics[3] = saved3;
}

// Guards the MM7 npctopic parse path through the version-parameter refactor: single header row.
GAME_TEST(NPCTableTopicMm7, DropsOneHeaderRow) {
    Blob blob = makeNpcDataBlob({
        {"#", "Topic", "Requires", "Notes", "Text #", "Owner(s)", "Owner #"},  // Single header row.
        {"1", "Castle Harmondale", "0", "Praise", "6", "Lord Markham", "1"},
    });

    NPCTopic saved1 = pNPCTopics[1];

    auto stats = std::make_unique<NPCStats>();
    EXPECT_NO_THROW(stats->InitializeNPCTopics(blob, GAME_VERSION_MM7));

    EXPECT_EQ(pNPCTopics[1].pTopic, "Castle Harmondale");

    pNPCTopics[1] = saved1;
}

// Guards the MM7 parse path through the version-parameter refactor: MM7 reads greetingIndex from col 8,
// the y/n join flag from col 9, and six event columns (A-F) into dialogue 1-6 from cols 10-15.
GAME_TEST(NPCTableMm7, ParsesMm7Layout) {
    Blob blob = makeNpcDataBlob({
        {"NPC Data (Special)"},                                                              // Header 1.
        {"#", "Name", "Pic", "A", "B", "C", "2D Location", " 1 - 76", "#", "Y / N", "# A",   // Header 2.
         "# B", "# C", "# D", "# E", "# F", "Notes"},
        // index | name | pic | group A/B/C | house | profession | greet | join | A | B | C | D | E | F |
        // notes.
        {"1", "Lord Markham", "709", "0", "0", "0", "186", "0", "5", "y", "1", "2", "3", "34", "187", "0",
         "An important fellow"},
    });

    auto stats = std::make_unique<NPCStats>();
    EXPECT_NO_THROW(stats->InitializeNPCData(blob, GAME_VERSION_MM7));

    const NPCData &npc = stats->pOriginalNPCData[1];
    EXPECT_EQ(npc.name, "Lord Markham");
    EXPECT_EQ(npc.portraitId, 709u);
    EXPECT_EQ(static_cast<int>(npc.house), 186);
    EXPECT_EQ(npc.greetingIndex, 5);           // MM7 greeting index from col 8.
    EXPECT_TRUE(npc.canJoin);                  // MM7 y/n join flag from col 9.
    EXPECT_EQ(npc.dialogue_1_evt_id, 1u);
    EXPECT_EQ(npc.dialogue_2_evt_id, 2u);
    EXPECT_EQ(npc.dialogue_3_evt_id, 3u);
    EXPECT_EQ(npc.dialogue_4_evt_id, 34u);     // Event #D, only present in MM7's layout.
    EXPECT_EQ(npc.dialogue_5_evt_id, 187u);
    EXPECT_EQ(npc.dialogue_6_evt_id, 0u);
}

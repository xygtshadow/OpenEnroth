#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Testing/Game/GameTest.h"

#include "Application/Paths/GameVersion.h"

#include "Engine/Localization.h"
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
// dialogue 1/2/3, and leaves greetingIndex / dialogue 4-6 at their defaults. MM6 profession ids are
// MM6's own 77-profession set (diverging from MM7's from id 23) and translate via npcProfessionFromMm6Id.
GAME_TEST(NPCTableMm6, ParsesMm6Layout) {
    Blob blob = makeNpcDataBlob({
        {"NPC Data (Special)"},                                                          // Header 1.
        {"#", "Name", "Pic", "State", "Fame", "Rep", "2D Location", " 1 - 76", "Y / N",  // Header 2.
         "Y / N", "# A", "# B", "# C", "Notes"},
        // index | name | pic | state | fame | rep | house | profession | join | news | A | B | C | notes.
        {"1", "Andover Potbello", "81", "0", "0", "0", "92", "74", "1", "0", "1", "296", "0",
         "gives money for the sixth letter and retrieve candelabra quest"},
        {"2", "Maria", "126", "0", "0", "0", "92", "4", "0", "1", "8", "0", "0",
         "gives clue for marketing promotion."},
    });

    auto stats = std::make_unique<NPCStats>();
    EXPECT_NO_THROW(stats->InitializeNPCData(blob, GAME_VERSION_MM6));

    const NPCData &npc1 = stats->pOriginalNPCData[1];
    EXPECT_EQ(npc1.name, "Andover Potbello");
    EXPECT_EQ(npc1.portraitId, 81u);
    EXPECT_EQ(static_cast<int>(npc1.house), 92);
    EXPECT_EQ(npc1.profession, FollowerOfBaa); // MM6 id 74, an MM6-only profession.
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
    EXPECT_EQ(npc2.profession, Scholar);       // MM6 id 4 is in the shared 1-22 prefix -> kept as is.
    EXPECT_FALSE(npc2.canJoin);                // col 8 = 0.
    EXPECT_EQ(npc2.dialogue_1_evt_id, 8u);
}

// MM6's npcnews.txt is a different feature from MM7's: it is per-map "Regional News" (rows of
// index | Map | Topic | News Text) preceded by TWO header rows ("Regional News" then "# Map Topic News
// Text"), whereas MM7 is a 52-entry NPC catch-phrase list (index | text | notes) with a single header.
// The Map column is offset by 25 from the mapstats.txt row for the 15 outdoor regions (26-40 -> rows
// 1-15, e.g. 40 = New Sorpigal), and Map 1 is a pool of kingdom-wide rumors. The MM6 parse fills
// pRegionalNews / pGeneralNews and leaves MM7's pCatchPhrases untouched.
GAME_TEST(NPCTableNewsMm6, ParsesRegionalNews) {
    Blob blob = makeNpcDataBlob({
        {"Regional News", "", "", ""},               // Header 1.
        {"#", "Map", "Topic", "News Text"},          // Header 2.
        {"1", "40", "Goblinwatch", "\"The keep on the hill is now home to a pack of goblins.\""},
        {"2", "40", "Baa Temple", "A new Temple dedicated to Baa lies west of here."},
        {"3", "1", "Obelisks", "Touch an obelisk and you'll get part of a message."},
    });

    auto stats = std::make_unique<NPCStats>();
    EXPECT_NO_THROW(stats->InitializeNPCNews(blob, GAME_VERSION_MM6));

    // Map 40 - 25 = mapstats row 15, New Sorpigal.
    const std::vector<RegionalNewsEntry> &sorpigal = stats->pRegionalNews[static_cast<MapId>(15)];
    ASSERT_EQ(sorpigal.size(), 2u);
    EXPECT_EQ(sorpigal[0].topic, "Goblinwatch");
    EXPECT_EQ(sorpigal[0].text, "The keep on the hill is now home to a pack of goblins."); // Unquoted.
    EXPECT_EQ(sorpigal[1].topic, "Baa Temple");

    // Map 1 = the kingdom-wide pool.
    ASSERT_EQ(stats->pGeneralNews.size(), 1u);
    EXPECT_EQ(stats->pGeneralNews[0].topic, "Obelisks");

    // And a news line can be picked for the map (from either pool).
    EXPECT_FALSE(stats->pickRandomNewsEntry(static_cast<MapId>(15)).text.empty());

    // MM7's catch-phrase array stays at its defaults.
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

// MM6's npcprof.txt parses natively. Its column layout is # | Professions | Random Chance | Cost/w |
// Personality | Action Text | In Party Benefit | Join Text (a "Random Chance" col 2 and "Personality"
// col 4 that MM7 doesn't have, no "Dismiss Text"), and its ids are MM6's own 77-profession set that
// diverges from MM7's from id 23 - translated via npcProfessionFromMm6Id (23 -> Counselor, not MM7's
// Herbalist; 69 -> Hunter2, not MM7's benefit-carrying Hunter; 77 -> Child). Col 1 feeds the
// localization profession-name table, and the random-chance column fans out into every map's
// pProfessionChance slot (MM6 has no per-map npcdist.txt).
GAME_TEST(NPCTableProfMm6, ParsesMm6Layout) {
    Blob blob = makeNpcDataBlob({
        {"", "", "", "", "", "", "", ""},                                                  // Header 1.
        {"", "NPC", "Random", "Join", "", "", "", ""},                                     // Header 2.
        {"#", "Professions", "Chance", "Cost/w", "Personality", "Action Text",             // Header 3.
         "In Party Benefit", "Join Text"},
        {"", "", "", "", "", "", "", ""},                                                  // Header 4.
        {"1", "Smith", "10", "200", "Merchant", "", "Unlimited weapon repair.", "I'll join for 200 gold."},
        {"23", "Counselor", "10", "200", "Official", "", "", "Need advice?"},
        {"69", "Hunter", "10", "5", "Adventurer", "", "", "I have no skills to offer."},
        {"77", "Child", "??", "1", "Peasant", "", "", "Can I come too?"},
    });

    // The parse feeds the global localization name table - snapshot & restore the touched slots.
    std::string savedSmith = localization->npcProfessionName(Smith);
    std::string savedCounselor = localization->npcProfessionName(Counselor);
    std::string savedHunter2 = localization->npcProfessionName(Hunter2);
    std::string savedChild = localization->npcProfessionName(Child);

    auto stats = std::make_unique<NPCStats>();
    EXPECT_NO_THROW(stats->InitializeNPCProfs(blob, GAME_VERSION_MM6));

    EXPECT_EQ(stats->pProfessions[Smith].uHirePrice, 200u);  // Cost/w from col 3, not the col-2 chance.
    EXPECT_EQ(stats->pProfessions[Smith].pBenefits, "Unlimited weapon repair.");
    EXPECT_EQ(stats->pProfessions[Smith].pJoinText, "I'll join for 200 gold.");
    EXPECT_TRUE(stats->pProfessions[Smith].pDismissText.empty());  // MM6 has no dismiss-text column.

    // Divergent / MM6-only ids land on their own slots, not MM7's professions at the same id.
    EXPECT_EQ(stats->pProfessions[Counselor].uHirePrice, 200u);
    EXPECT_EQ(stats->pProfessions[Herbalist].uHirePrice, 0u);   // MM7's id 23 stays untouched.
    EXPECT_EQ(stats->pProfessions[Hunter2].uHirePrice, 5u);
    EXPECT_EQ(stats->pProfessions[Hunter].uHirePrice, 0u);      // MM7's Hunter (58) stays untouched.
    EXPECT_EQ(stats->pProfessions[Child].uHirePrice, 1u);

    // Col 1 is MM6's localized profession-name source.
    EXPECT_EQ(localization->npcProfessionName(Counselor), "Counselor");
    EXPECT_EQ(localization->npcProfessionName(Child), "Child");

    // Random-chance weights fan out to every map; Child's literal "??" chance parses as 0 (never generated).
    MapId map = static_cast<MapId>(15);
    EXPECT_EQ(stats->pProfessionChance[map].chanceByProfession[Smith], 10);
    EXPECT_EQ(stats->pProfessionChance[map].chanceByProfession[Child], 0);
    EXPECT_EQ(stats->pProfessionChance[map].total, 30);  // 10 + 10 + 10 + 0.

    EXPECT_EQ(stats->uNumNPCProfessions, 78);

    localization->setNpcProfessionName(Smith, std::move(savedSmith));
    localization->setNpcProfessionName(Counselor, std::move(savedCounselor));
    localization->setNpcProfessionName(Hunter2, std::move(savedHunter2));
    localization->setNpcProfessionName(Child, std::move(savedChild));
}

// Guards the MM7 npcprof parse path through the version-parameter refactor: MM7 columns are
// # | Professions | Cost/w | Action Text | In Party Benefit | Join Text | Dismiss Text, 4 header rows.
GAME_TEST(NPCTableProfMm7, ParsesProfessions) {
    Blob blob = makeNpcDataBlob({
        {"", "", "", "", "", "", ""},                                                      // Header 1.
        {"", "NPC", "Join", "", "", "", ""},                                               // Header 2.
        {"#", "Professions", "Cost/w", "Action Text", "In Party Benefit", "Join Text",     // Header 3.
         "Dismiss Text"},
        {"", "", "", "", "", "", ""},                                                      // Header 4.
        {"1", "Smith", "200", "", "Unlimited weapon repair.", "I'll join for 200 gold.", "Don't dismiss me!"},
    });

    auto stats = std::make_unique<NPCStats>();
    EXPECT_NO_THROW(stats->InitializeNPCProfs(blob, GAME_VERSION_MM7));

    const NPCProfession &smith = stats->pProfessions[Smith];
    EXPECT_EQ(smith.uHirePrice, 200u);                         // MM7 Cost/w from col 2.
    EXPECT_EQ(smith.pBenefits, "Unlimited weapon repair.");    // col 4.
    EXPECT_EQ(smith.pJoinText, "I'll join for 200 gold.");     // col 5.
    EXPECT_EQ(smith.pDismissText, "Don't dismiss me!");        // col 6.
    EXPECT_EQ(stats->uNumNPCProfessions, 59);
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

#include "NPCTable.h"

#include <array>
#include <string>
#include <utility>
#include <vector>

#include "Engine/Localization.h"
#include "Engine/MapEnumFunctions.h"
#include "Engine/Objects/NPC.h"
#include "Engine/Objects/MonsterEnumFunctions.h"
#include "Engine/Party.h"
#include "Engine/Objects/NPCEnumFunctions.h"
#include "Engine/Resources/ResourceManager.h"
#include "Engine/Random/Random.h"

#include "Library/Logger/Logger.h"
#include "Library/Serialization/Serialization.h"

#include "Utility/Memory/Blob.h"
#include "Utility/String/Split.h"
#include "Utility/String/Transformations.h"

std::array<NPCTopic, 789> pNPCTopics;
NPCStats *pNPCStats = nullptr;

// Personality names as MM6's npcprof.txt "Personality" column spells them (the parser in MM6.EXE
// string-matches against the same set - the string block right before "npcprof.txt" @0x4C1794).
static NpcPersonality mm6PersonalityByName(std::string_view name) {
    if (name == "Adventurer") return PERSONALITY_ADVENTURER;
    if (name == "Fanatic") return PERSONALITY_EVIL_FANATIC;
    if (name == "Guard") return PERSONALITY_GUARD;
    if (name == "Merchant") return PERSONALITY_MERCHANT;
    if (name == "Noble") return PERSONALITY_NOBLE;
    if (name == "Official") return PERSONALITY_OFFICIAL;
    if (name == "Paladin") return PERSONALITY_PALADIN;
    if (name == "Peasant") return PERSONALITY_PEASANT;
    if (name == "Priest") return PERSONALITY_PRIEST;
    if (name == "Scholar") return PERSONALITY_SCHOLAR;
    if (name == "Sorcerer") return PERSONALITY_SORCERER;
    if (name == "Thief") return PERSONALITY_THIEF;
    if (name == "Monster") return PERSONALITY_MONSTER;
    logger->warning("npcprof.txt: unknown personality '{}', defaulting to Adventurer.", name);
    return PERSONALITY_ADVENTURER; // MM6.EXE's zeroed-row default.
}

int NPCStats::dword_AE336C_LastMispronouncedNameFirstLetter = -1;
int NPCStats::dword_AE3370_LastMispronouncedNameResult = -1;
int NPCStats::mm6LastAddressingAwardPick = -1;

//----- (00476977) --------------------------------------------------------
void NPCStats::InitializeNPCText(const Blob &npcText, GameVersion version) {
    // npctext.txt table structure: index | text (localized) | dev notes | npc name (localized, not used).
    // MM6 has two header rows ("Text Number From NPC Events.Doc" then "# Text Notes") where MM7 has one;
    // the columns otherwise correspond, so only the header count differs.
    int headerRows = version == GAME_VERSION_MM6 ? 2 : 1;
    for (std::string_view line : split(npcText.str()).by("\r\n").drop(headerRows).skip("")) {
        std::array<std::string_view, 2> tokens = split(line).by('\t');
        int i = fromString<int>(tokens[0]) - 1; // File indices are 1-based, array is 0-based.
        pNPCTopics[i].pText = removeQuotes(tokens[1]);
    }
}

void NPCStats::InitializeNPCTopics(const Blob &npcTopics, GameVersion version) {
    // npctopic.txt table structure: index | topic (localized) | ??? (not used) | dev notes | text index (not used) |
    //                               npc name (not localized, not used) | npc index (not used).
    // MM6 has two header rows ("Text Number From NPC Events.Doc ... Notes" then "# Topic") where MM7 has one.
    int headerRows = version == GAME_VERSION_MM6 ? 2 : 1;
    for (std::string_view line : split(npcTopics.str()).by("\r\n").drop(headerRows).skip("")) {
        std::array<std::string_view, 2> tokens = split(line).by('\t');
        int i = fromString<int>(tokens[0]);
        pNPCTopics[i].pTopic = removeQuotes(tokens[1]);
    }
}

void NPCStats::InitializeNPCDist(const Blob &npcDist) {
    // npcdist.txt table structure: profession (localized, not used) | area profession chance values...
    // The file is MM7-only (58 profession rows, then trailing non-numeric rows) - zip against MM7's
    // profession range, NOT allNpcProfessions(), which also spans the MM6-only professions and would
    // pull the trailing rows in as data.
    for (auto [line, prof] : split(npcDist.str()).by("\r\n").drop(2).skip("").zip(Segment(Smith, Hunter)))
        for (auto [token, map] : split(line).by('\t').drop(1).zip(allMaps()))
            pProfessionChance[map].chanceByProfession[prof] = fromString<int>(token);

    for (MapId map : allMaps())
        for (NpcProfession prof : allNpcProfessions())
            pProfessionChance[map].total += pProfessionChance[map].chanceByProfession[prof];
}

// TODO(Nik-RE-dev): move out of table back to Engine/Objects/NPC.cpp
void NPCStats::setNPCNamesOnLoad() {
    for (unsigned int i = 1; i < uNumNewNPCs; ++i)
        pNPCData[i].name = pNPCUnicNames[i - 1];

    if (!pParty->pHirelings[0].name.empty())
        pParty->pHirelings[0].name = pParty->pHireling1Name;
    if (!pParty->pHirelings[1].name.empty())
        pParty->pHirelings[1].name = pParty->pHireling2Name;
}

//----- (00476CB5) --------------------------------------------------------
void NPCStats::InitializeNPCData(const Blob &npcData, GameVersion version) {
    // npcdata.txt table layout differs between MM7 and MM6:
    //
    //  MM7 (17 cols): index | name | portrait | group A/B/C (3-5) | 2D location (6) | profession (7) |
    //                 greeting index (8) | can join y/n (9) | event ids A-F (10-15) | notes (16).
    //  MM6 (14 cols): index | name | portrait | state/fame/rep (3-5) | 2D location (6) | profession (7) |
    //                 join 0/1 (8) | news 0/1 (9) | event ids A-C (10-12) | notes (13).
    //
    // 2D location (6) and profession (7) line up in both, as do event ids A/B/C -> dialogue 1/2/3 (10-12).
    // MM6 has no greeting-index column, its join flag is the numeric col 8 (MM7's join is the y/n col 9),
    // and from col 13 on it is free-form text (the "Notes" column), so MM7's numeric reads of cols 13-15
    // would throw on MM6 data. Branch the version-specific columns below.
    bool isMm6 = version == GAME_VERSION_MM6;
    for (std::string_view line : split(npcData.str()).by("\r\n").drop(2).skip("").take(500)) {
        std::array<std::string_view, 16> tokens = split(line).by('\t');
        int i = fromString<int>(tokens[0]); // File indices are 1-based.
        pNPCUnicNames[i - 1] = removeQuotes(tokens[1]);
        pOriginalNPCData[i].name = pNPCUnicNames[i - 1]; // TODO(captainurist): just make this 1-based too?
        pOriginalNPCData[i].portraitId = fromString<int>(tokens[2]);
        pOriginalNPCData[i].house = static_cast<HouseId>(fromString<int>(tokens[6]));
        pOriginalNPCData[i].dialogue_1_evt_id = fromString<int>(tokens[10]);
        pOriginalNPCData[i].dialogue_2_evt_id = fromString<int>(tokens[11]);
        pOriginalNPCData[i].dialogue_3_evt_id = fromString<int>(tokens[12]);
        if (isMm6) {
            // MM6 profession ids are MM6's own 77-profession set (diverges from MM7's from id 23).
            pOriginalNPCData[i].profession = npcProfessionFromMm6Id(fromString<int>(tokens[7]));
            // No greeting-index column; join is the numeric col 8 (0/1); no event D/E/F columns.
            pOriginalNPCData[i].canJoin = fromString<int>(tokens[8]) != 0;
            // Col 9 is MM6's "News Y/N" - whether this NPC tells regional news (a dialogue option).
            pOriginalNPCData[i].mm6HasNews = fromString<int>(tokens[9]) != 0;
            // Cols 3-5 gate street dialogue: State (initial greet state, all zeros in shipped data,
            // skipped) | Fame requirement | Rep requirement (signed, sign = the NPC's alignment).
            pOriginalNPCData[i].fame = fromString<int>(tokens[4]);
            pOriginalNPCData[i].rep = fromString<int>(tokens[5]);
        } else {
            pOriginalNPCData[i].profession = static_cast<NpcProfession>(fromString<int>(tokens[7]));
            pOriginalNPCData[i].greetingIndex = fromString<int>(tokens[8]);
            pOriginalNPCData[i].canJoin = tokens[9][0] == 'y' ? 1 : 0;
            pOriginalNPCData[i].dialogue_4_evt_id = fromString<int>(tokens[13]);
            pOriginalNPCData[i].dialogue_5_evt_id = fromString<int>(tokens[14]);
            pOriginalNPCData[i].dialogue_6_evt_id = fromString<int>(tokens[15]);
        }
    }
    uNumNewNPCs = 501;
}

void NPCStats::InitializeNPCGreets(const Blob &npcGreets) {
    // npcgreet.txt table structure: index | greeting 1 (localized) | greeting 2 (localized) |
    //                               notes (not localized, not used) | owner (not localized, not used).
    for (std::string_view line : split(npcGreets.str()).by("\r\n").drop(1).skip("")) {
        std::array<std::string_view, 3> tokens = split(line).by('\t');
        if (tokens[0].empty())
            continue; // Trailing orphan row with no index column.

        int i = fromString<int>(tokens[0]); // File indices are 1-based.
        pNPCGreetings[i].pGreeting1 = removeQuotes(tokens[1]);
        pNPCGreetings[i].pGreeting2 = removeQuotes(tokens[2]);
    }
}

void NPCStats::InitializeNPCGroups(const Blob &npcGroups) {
    // npcgroup.txt table structure: group index | news index | dev notes.
    for (std::string_view line : split(npcGroups.str()).by("\r\n").drop(1).skip("")) {
        std::array<std::string_view, 2> tokens = split(line).by('\t');
        int i = fromString<int>(tokens[0]); // File indices are 0-based.
        pOriginalGroups[i] = fromString<int>(tokens[1]);
    }
}

void NPCStats::InitializeNPCNews(const Blob &npcNews, GameVersion version) {
    if (version == GAME_VERSION_MM6) {
        // MM6's npcnews.txt is a different feature from MM7's: it is "Regional News" - the gossip street
        // townsfolk tell when talked to - under two header rows, with rows of index | Map | Topic | News Text.
        // The Map column is offset by 25 from the mapstats.txt row for the 15 outdoor regions (values 26-40 ->
        // rows 1-15, e.g. 40 = New Sorpigal), and Map 1 is a pool of kingdom-wide rumors.
        for (std::string_view line : split(npcNews.str()).by("\r\n").drop(2).skip("")) {
            std::array<std::string_view, 4> tokens = split(line).by('\t');
            if (tokens[0].empty())
                continue; // Trailing orphan row with no index column.

            int newsMapId = fromString<int>(tokens[1]);
            RegionalNewsEntry entry;
            entry.topic = removeQuotes(tokens[2]);
            entry.text = removeQuotes(tokens[3]);
            if (newsMapId == 1) {
                pGeneralNews.push_back(std::move(entry));
            } else if (newsMapId >= 26 && newsMapId <= 40) {
                pRegionalNews[static_cast<MapId>(newsMapId - 25)].push_back(std::move(entry));
            } else {
                logger->warning("npcnews.txt: unexpected map id {} for news topic '{}', skipping.",
                                newsMapId, entry.topic);
            }
        }
        return;
    }

    // npcnews.txt table structure: index | text (localized) | dev notes.
    for (std::string_view line : split(npcNews.str()).by("\r\n").drop(1).skip("")) {
        std::array<std::string_view, 2> tokens = split(line).by('\t');
        int i = fromString<int>(tokens[0]); // File indices are 0-based.
        pCatchPhrases[i] = removeQuotes(tokens[1]);
    }
}

RegionalNewsEntry NPCStats::pickRandomNewsEntry(MapId map) const {
    // MM6.EXE 0x43BC20: pick from the current map's own news; a map with no news of its own falls
    // back to the kingdom-wide pool (the file's "Map 1" rows) - the pools are not mixed.
    const std::vector<RegionalNewsEntry> &pool = !pRegionalNews[map].empty() ? pRegionalNews[map] : pGeneralNews;
    if (pool.empty())
        return {};

    return pool[grng->random(pool.size())];
}

//----- (0047702F) --------------------------------------------------------
void NPCStats::Initialize(ResourceManager *resourceManager, GameVersion version) {
    pOriginalNPCData.fill(NPCData());
    InitializeNPCData(resourceManager->eventsData("npcdata.txt"), version);
    // npcgreet.txt, npcgroup.txt and npcdist.txt are absent from MM6's icons.lod. eventsDataIfPresent
    // yields an empty blob when missing, and each Initialize below no-ops on empty input.
    InitializeNPCGreets(resourceManager->eventsDataIfPresent("npcgreet.txt"));
    InitializeNPCGroups(resourceManager->eventsDataIfPresent("npcgroup.txt"));
    InitializeNPCNews(resourceManager->eventsData("npcnews.txt"), version);
    InitializeNPCText(resourceManager->eventsData("npctext.txt"), version);
    InitializeNPCTopics(resourceManager->eventsData("npctopic.txt"), version);
    InitializeNPCDist(resourceManager->eventsDataIfPresent("npcdist.txt"));
    InitializeNPCNames(resourceManager->eventsData("npcnames.txt"));
    InitializeNPCProfs(resourceManager->eventsData("npcprof.txt"), version);
    if (version == GAME_VERSION_MM6) {
        // MM6-only street-dialogue tables: npcbtb.txt (beg/threaten/bribe & reaction texts) and
        // proftext.txt (per-profession weekday small talk). Both absent from MM7's events.lod.
        InitializeNPCBtb(resourceManager->eventsData("npcbtb.txt"));
        InitializeMm6ProfText(resourceManager->eventsData("proftext.txt"));
    }
}

void NPCStats::InitializeNPCNames(const Blob &npcNames) {
    // npcnames.txt table structure: male name (localized) | female name (localized).
    // Female column runs out before the male column.
    uNewlNPCBufPos = 0;
    pNPCNames.fill({});
    for (std::string_view line : split(npcNames.str()).by("\r\n").drop(1).skip("")) {
        std::array<std::string_view, 2> tokens = split(line).by('\t');
        if (!tokens[0].empty())
            pNPCNames[SEX_MALE].emplace_back(removeQuotes(tokens[0]));
        if (!tokens[1].empty())
            pNPCNames[SEX_FEMALE].emplace_back(removeQuotes(tokens[1]));
    }
}

void NPCStats::InitializeNPCProfs(const Blob &npcProfs, GameVersion version) {
    if (version == GAME_VERSION_MM6) {
        // MM6 npcprof.txt table structure: profession id | profession name (localized) | random chance |
        //                                  hire price | personality (not used) | action text (localized) |
        //                                  benefit description (localized) | join text (localized).
        // Compared to MM7 it inserts "Random Chance" (col 2) and "Personality" (col 4) and has no
        // "Dismiss Text"; its 77-profession set diverges from MM7's from id 23, so ids go through
        // npcProfessionFromMm6Id. Col 1 is MM6's only localized source of profession names (MM7 reads
        // them from fixed global.txt rows, which don't line up with MM6's global.txt), so it feeds the
        // localization table. The random-chance column drives street-citizen generation; MM6 has no
        // per-map npcdist.txt, the same weights apply everywhere, so they fan out into every
        // pProfessionChance slot.
        IndexedArray<int, NPC_PROFESSION_FIRST, NPC_PROFESSION_LAST> chanceByProfession = {{}};
        for (std::string_view line : split(npcProfs.str()).by("\r\n").drop(4)) {
            std::array<std::string_view, 8> tokens = split(line).by('\t');
            if (tokens[0].empty())
                continue; // Trailing pure-tab orphan rows past the last entry.

            NpcProfession prof = npcProfessionFromMm6Id(fromString<int>(tokens[0]));
            if (prof == NoProfession) {
                logger->warning("npcprof.txt: unexpected MM6 profession id '{}', skipping.", tokens[0]);
                continue;
            }

            localization->setNpcProfessionName(prof, std::string(removeQuotes(tokens[1])));
            // Child's random chance is a literal "??" in the file - not a number, so it never generates.
            if (!tokens[2].empty() && tokens[2].find_first_not_of("0123456789") == std::string_view::npos)
                chanceByProfession[prof] = fromString<int>(tokens[2]);
            mm6PersonalityByProfession[prof] = mm6PersonalityByName(trim(tokens[4]));
            pProfessions[prof].uHirePrice = fromString<int>(tokens[3]);
            pProfessions[prof].pActionText = removeQuotes(tokens[5]);
            pProfessions[prof].pBenefits = removeQuotes(tokens[6]);
            pProfessions[prof].pJoinText = removeQuotes(tokens[7]);
        }

        int total = 0;
        for (NpcProfession prof : allNpcProfessions())
            total += chanceByProfession[prof];
        for (MapId map : allMaps()) {
            pProfessionChance[map].chanceByProfession = chanceByProfession;
            pProfessionChance[map].total = total;
        }
        uNumNPCProfessions = 78; // Number of professions + 1, mirroring MM7's 59.
        return;
    }

    // npcprof.txt table structure: profession id | profession name (localized, not used) | hire price |
    //                              action text (localized) | benefit description (localized) |
    //                              join text (localized) | dismiss text (localized).
    for (std::string_view line : split(npcProfs.str()).by("\r\n").drop(4)) {
        std::array<std::string_view, 7> tokens = split(line).by('\t');
        if (tokens[0].empty())
            continue; // Trailing pure-tab orphan rows past the last entry.

        NpcProfession prof = static_cast<NpcProfession>(fromString<int>(tokens[0]));
        pProfessions[prof].uHirePrice = fromString<int>(tokens[2]);
        pProfessions[prof].pActionText = removeQuotes(tokens[3]);
        pProfessions[prof].pBenefits = removeQuotes(tokens[4]);
        pProfessions[prof].pJoinText = removeQuotes(tokens[5]);
        pProfessions[prof].pDismissText = removeQuotes(tokens[6]);
    }
    uNumNPCProfessions = 59;
}

// npcbtb.txt's 13 personality columns in file order. The in-memory personality order differs;
// MM6.EXE remaps the columns with the table @0x4C1036 while parsing, and this is that table.
static constexpr std::array<NpcPersonality, 13> mm6BtbFileColumnOrder = {{
    PERSONALITY_PEASANT, PERSONALITY_MONSTER, PERSONALITY_THIEF, PERSONALITY_MERCHANT,
    PERSONALITY_SORCERER, PERSONALITY_SCHOLAR, PERSONALITY_ADVENTURER, PERSONALITY_PRIEST,
    PERSONALITY_OFFICIAL, PERSONALITY_GUARD, PERSONALITY_EVIL_FANATIC, PERSONALITY_PALADIN,
    PERSONALITY_NOBLE,
}};

void NPCStats::InitializeNPCBtb(const Blob &npcBtb) {
    // npcbtb.txt table structure: one header row, then rows of label | notes | 13 personality cells.
    // The label is "Beg" / "Bribe" / "Threat" for the three accept-flag rows (cells are 0/1), or a
    // Msg# 1-24 for the reaction-text rows. Cells that can never show (e.g. a "Beg return" line for
    // a personality that refuses begging) are literal "n/a" in the file and are kept verbatim.
    for (std::string_view line : split(npcBtb.str()).by("\r\n").drop(1).skip("")) {
        std::array<std::string_view, 15> tokens = split(line).by('\t');
        std::string_view label = trim(tokens[0]);
        if (label.empty())
            continue;

        if (label == "Beg" || label == "Bribe" || label == "Threat") {
            auto &flags = label == "Beg" ? mm6PersonalityAcceptsBeg
                        : label == "Bribe" ? mm6PersonalityAcceptsBribe
                        : mm6PersonalityAcceptsThreat;
            for (int column = 0; column < 13; column++)
                flags[mm6BtbFileColumnOrder[column]] = trim(tokens[column + 2]) == "1";
            continue;
        }

        int msg = fromString<int>(label);
        if (msg < 1 || msg >= static_cast<int>(mm6BtbTexts.size())) {
            logger->warning("npcbtb.txt: unexpected row label '{}', skipping.", label);
            continue;
        }
        for (int column = 0; column < 13; column++)
            mm6BtbTexts[msg][mm6BtbFileColumnOrder[column]] = removeQuotes(tokens[column + 2]);
    }
}

void NPCStats::InitializeMm6ProfText(const Blob &profText) {
    // proftext.txt table structure: two header rows, then rows of profession id | name (not used) |
    // 7 x (topic, text) - one pair per weekday, Sunday first.
    for (std::string_view line : split(profText.str()).by("\r\n").drop(2).skip("")) {
        std::array<std::string_view, 16> tokens = split(line).by('\t');
        if (tokens[0].empty())
            continue; // Trailing pure-tab orphan rows past the last entry.

        NpcProfession prof = npcProfessionFromMm6Id(fromString<int>(tokens[0]));
        if (prof == NoProfession) {
            logger->warning("proftext.txt: unexpected MM6 profession id '{}', skipping.", tokens[0]);
            continue;
        }
        for (int day = 0; day < 7; day++) {
            mm6ProfText[prof][day].topic = removeQuotes(tokens[2 + 2 * day]);
            mm6ProfText[prof][day].text = removeQuotes(tokens[3 + 2 * day]);
        }
    }
}

NpcProfession NPCStats::rollProfession(MapId mapId) const {
    if (pProfessionChance[mapId].total <= 0) // Zero when profession chances are not loaded.
        return Hunter; // MM7's fallback profession (was NPC_PROFESSION_LAST before the enum grew MM6's professions).

    int cap = grng->random(pProfessionChance[mapId].total);
    int sum = 0;
    for (NpcProfession prof : allNpcProfessions()) {
        sum += pProfessionChance[mapId].chanceByProfession[prof];
        if (sum > cap)
            return prof;
    }
    return Hunter;
}

// MM6.EXE 0x4C13F0 / 0x4C15D0: the street-citizen portrait pools, per sex, over the regular npcXXX
// portrait space (they are sex-consistent subsets of the npcdata portraits - the npc501..npc554
// "commoner block" is not used for citizens at all).
static constexpr std::array<int, 120> mm6MaleCitizenPortraits = {{
    1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27,
    28, 29, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53,
    54, 55, 56, 57, 58, 60, 62, 64, 65, 66, 68, 70, 71, 74, 75, 76, 77, 78, 81, 89, 96, 101, 102,
    103, 106, 119, 123, 128, 129, 130, 145, 152, 157, 158, 159, 161, 162, 163, 166, 168, 169, 170,
    173, 177, 181, 182, 183, 184, 187, 188, 189, 191, 192, 195, 196, 197, 199, 213, 214, 223, 289,
    357, 367, 369, 370, 372, 373, 374, 377, 389,
}};
static constexpr std::array<int, 66> mm6FemaleCitizenPortraits = {{
    30, 63, 82, 83, 84, 86, 87, 88, 99, 100, 104, 105, 107, 108, 109, 110, 113, 114, 115, 118, 124,
    125, 126, 132, 136, 139, 144, 146, 148, 149, 150, 155, 160, 164, 165, 167, 171, 185, 186, 190,
    194, 240, 250, 251, 252, 253, 254, 255, 256, 257, 258, 259, 260, 296, 297, 299, 300, 301, 302,
    303, 304, 305, 306, 316, 326, 348,
}};

std::span<const int> NPCStats::mm6CitizenPortraitPool(Sex sex) {
    if (sex == SEX_FEMALE)
        return mm6FemaleCitizenPortraits;
    return mm6MaleCitizenPortraits;
}

// The street-NPC reputation-requirement roll, shared verbatim between MM6 (citizen generation
// @0x469210) and MM7 (InitializeAdditionalNPCs): d100 -> 59% no requirement, 30% "display
// reputation must exceed +200", 5% "below -300", 3% "above +400", 3% "below -600".
static int rollStreetNpcRepRequirement() {
    int roll = grng->random(100) + 1;
    if (roll < 60)
        return 0;
    if (roll < 90)
        return 200;
    if (roll < 95)
        return -300;
    if (roll < 98)
        return 400;
    return -600;
}

void NPCStats::initializeMm6StreetCitizen(NPCData *npc, Sex sex, MapId mapId) {
    *npc = NPCData();
    npc->sex = sex;
    npc->name = grng->randomSample(pNPCNames[sex]);
    npc->portraitId = grng->randomSample(mm6CitizenPortraitPool(sex));
    npc->rep = rollStreetNpcRepRequirement();
    npc->profession = rollProfession(mapId);
    npc->house = HOUSE_INVALID;
    npc->field_24 = 1;
    npc->canJoin = 1;
    npc->mm6HasNews = true; // Street citizens always carry the news option (MM6.EXE street page 0x419600).
}

//----- (0047732C) --------------------------------------------------------
void NPCStats::InitializeAdditionalNPCs(NPCData *pNPCDataBuff, MonsterId npc_uid,
                                        HouseId uLocation2D, MapId uMapId) {
    int uGeneratedPortret;    // ecx@23
                              // signed int result; // eax@39
    Race uRace;                // [sp+Ch] [bp-Ch]@1
    bool break_gen;           // [sp+10h] [bp-8h]@1
    signed int gen_attempts;  // [sp+14h] [bp-4h]@1
    int uPortretMin;          // [sp+24h] [bp+Ch]@1
    int uPortretMax;

    MonsterType monsterType = monsterTypeForMonsterId(npc_uid);
    Sex uNPCSex = sexForMonsterType(monsterType);
    uRace = raceForMonsterType(monsterType);
    pNPCDataBuff->sex = uNPCSex;
    pNPCDataBuff->name = grng->randomSample(pNPCNames[uNPCSex]);

    gen_attempts = 0;
    break_gen = false;

    do {
        switch (uRace) {
            case RACE_HUMAN:
                if (uNPCSex == SEX_MALE) {
                    uPortretMin = 2;
                    uPortretMax = 100;
                } else {
                    uPortretMin = 201;
                    uPortretMax = 250;
                }
            case RACE_ELF:
                if (uNPCSex == SEX_MALE) {
                    uPortretMin = 400;
                    uPortretMax = 430;
                } else {
                    uPortretMin = 460;
                    uPortretMax = 490;
                }
                break;
            case RACE_GOBLIN:
                if (uNPCSex == SEX_MALE) {
                    uPortretMin = 500;
                    uPortretMax = 520;
                } else {
                    uPortretMin = 530;
                    uPortretMax = 550;
                }
                break;
            case RACE_DWARF:
                if (uNPCSex == SEX_MALE) {
                    uPortretMin = 300;
                    uPortretMax = 330;
                } else {
                    uPortretMin = 360;
                    uPortretMax = 387;
                }

                break;
        }

        uGeneratedPortret =
            uPortretMin + grng->random(uPortretMax - uPortretMin + 1);
        if (CheckPortretAgainstSex(uGeneratedPortret, uNPCSex))
            break_gen = true;
        ++gen_attempts;
        if (gen_attempts >= 4) {
            uGeneratedPortret = uPortretMin;
            break_gen = true;
        }
    } while (!break_gen);

    pNPCDataBuff->portraitId = uGeneratedPortret;
    pNPCDataBuff->flags = 0;
    pNPCDataBuff->fame = 0;
    pNPCDataBuff->rep = rollStreetNpcRepRequirement();
    pNPCDataBuff->profession = rollProfession(uMapId);
    pNPCDataBuff->house = uLocation2D;
    pNPCDataBuff->field_24 = 1;
    pNPCDataBuff->canJoin = 1;
    pNPCDataBuff->dialogue_1_evt_id = 0;
    pNPCDataBuff->dialogue_2_evt_id = 0;
    pNPCDataBuff->dialogue_3_evt_id = 0;
    pNPCDataBuff->dialogue_4_evt_id = 0;
    pNPCDataBuff->dialogue_5_evt_id = 0;
    pNPCDataBuff->dialogue_6_evt_id = 0;
}

//----- (00495366) --------------------------------------------------------
const std::string &NPCStats::sub_495366_MispronounceName(char firstLetter, Sex gender) {
    int pickedName;

    // TODO(captainurist): Caching in kinda wrong? Revisit when working on #mm6.
    //                     See "O Ho! %13! Er, %13. I think. Whatever..."

    if (firstLetter == dword_AE336C_LastMispronouncedNameFirstLetter) {
        pickedName = dword_AE3370_LastMispronouncedNameResult;
    } else {
        dword_AE336C_LastMispronouncedNameFirstLetter = firstLetter;
        const std::vector<std::string> &names = this->pNPCNames[gender];

        std::vector<int> matches;
        for (int i = 0; i < static_cast<int>(names.size()); ++i)
            if (tolower(names[i][0]) == tolower(firstLetter))
                matches.push_back(i);

        if (!matches.empty())
            pickedName = vrng->randomSample(matches);
        else
            pickedName = vrng->random(names.size()); // No name with this letter - pick any.
    }
    dword_AE3370_LastMispronouncedNameResult = pickedName;
    return this->pNPCNames[gender][pickedName];
}

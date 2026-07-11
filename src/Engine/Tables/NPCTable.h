#pragma once

#include <string>
#include <array>
#include <span>
#include <vector>

#include "Application/Paths/GameVersion.h"

#include "Engine/Data/HouseEnums.h"
#include "Engine/Objects/NPCEnums.h"
#include "Engine/Objects/CharacterEnums.h"
#include "Engine/Objects/MonsterEnums.h"
#include "Engine/Time/Duration.h"
#include "Engine/MapEnums.h"

#include "Utility/IndexedArray.h"
#include "Utility/Flags.h"

class Blob;
class ResourceManager;

// TODO(Nik-RE-dev): It seems that two greet flags are used purely because it's modification is performed
//                   before greeting string is constructed. It is also ensures that NPC in multi-NPC houses
//                   always greet you with first line until you leave the house.
//                   Ideally there should be only one flag.
enum class NpcFlag : uint32_t {
    NPC_GREETED_FIRST = 0x01, // NPC has been greeted first time
    NPC_GREETED_SECOND = 0x02, // NPC has been greeted second time
    NPC_HIRED = 0x80 // NPC is hired
};
using enum NpcFlag;
MM_DECLARE_FLAGS(NpcFlags, NpcFlag)
MM_DECLARE_OPERATORS_FOR_FLAGS(NpcFlags)

struct NPCTopic {
    std::string pTopic;
    std::string pText;
};

// One row of MM6's npcnews.txt - the "Regional News" gossip that street townsfolk tell when talked to.
struct RegionalNewsEntry {
    std::string topic; // Dev-facing topic name, e.g. "Goblinwatch". Labels the "News" dialogue option.
    std::string text; // The news line itself.
};

struct NPCData {  // 4Ch
    inline bool Hired() { return flags & NPC_HIRED; }

    std::string name; // Actual NPC name as displayed in-game.
    unsigned int portraitId = 0; // Portrait texture is "npcXXX" in icons.lod.
    NpcFlags flags = 0;
    // Fame requirement for the NPC to talk to the party. Live in MM6 street dialogue: party fame
    // (total experience / 1000) must EXCEED it or the NPC refuses to talk (npcbtb.txt row 6).
    // Unused in MM7 (the gate survives only as #if 0 code).
    int fame = 0;
    // Reputation requirement, signed: positive = a good NPC that demands display reputation > rep,
    // negative = an evil NPC that demands display reputation < rep. Live in MM6 street dialogue -
    // failing it gets a refusal greeting and only the Beg/Threaten/Bribe options. Generated street
    // citizens roll it (see initializeMm6StreetCitizen), fixed NPCs carry it in npcdata.txt.
    int rep = 0;
    HouseId house = HOUSE_INVALID; // House where this NPC is in.
    NpcProfession profession = NoProfession;
    int greetingIndex = 0; // Index into "npcgreet.txt" for this NPC's greeting.
    bool canJoin = false;
    int field_24 = 0;
    unsigned int dialogue_1_evt_id = 0;  // dialogue options that are defined by script
    unsigned int dialogue_2_evt_id = 0;  // = 0  == unused
    unsigned int dialogue_3_evt_id = 0;  // can also be idx in pNPCTopics
    unsigned int dialogue_4_evt_id = 0;  // and absolutely crazy stuff when it's in party hierlings (npc2)
    unsigned int dialogue_5_evt_id = 0;
    unsigned int dialogue_6_evt_id = 0;
    Sex sex = SEX_MALE;
    int hasUsedAbility = 0;
    int newsTopic = 0;
    // MM6: the regional news line behind this NPC's "News" dialogue option, picked once at first
    // dialogue (MM6.EXE 0x43BC20 stores an npcnews.txt index in the NPC's NewsTopic field, so the
    // NPC repeats the same line forever). Transient - street citizens live per map session.
    RegionalNewsEntry mm6News;
};

struct NPCSacrificeStatus {
    bool inProgress = false; // Dark sacrifice is in progress for this hired NPC?
    Duration elapsedTime; // Time elapsed since the spell was cast.
    Duration endTime; // Total time of the animation - NPC will be removed once this time is reached.
};

struct NPCProfession {
    unsigned int uHirePrice{};
    std::string pBenefits{};
    std::string pActionText{};
    std::string pJoinText{};
    std::string pDismissText{};
};

struct NPCProfessionChance {
    IndexedArray<int, NPC_PROFESSION_FIRST, NPC_PROFESSION_LAST> chanceByProfession = {{}};
    int total = 0;
};

struct NPCGreeting {
    std::string pGreeting1;  // at first meet
    std::string pGreeting2;  // at latest meets
};

// One weekday's profession small talk from MM6's proftext.txt - the topic labels the profession
// dialogue option, the text is the reply.
struct Mm6ProfDayText {
    std::string topic;
    std::string text;
};

struct NPCStats {
    void Initialize(ResourceManager *resourceManager, GameVersion version);
    void InitializeNPCNames(const Blob &npcNames);
    void InitializeNPCProfs(const Blob &npcProfs, GameVersion version);
    void InitializeNPCText(const Blob &npcText, GameVersion version);
    void InitializeNPCTopics(const Blob &npcTopics, GameVersion version);
    void InitializeNPCDist(const Blob &npcDist);
    void InitializeNPCData(const Blob &npcData, GameVersion version);
    void InitializeNPCGreets(const Blob &npcGreets);
    void InitializeNPCGroups(const Blob &npcGroups);
    void InitializeNPCNews(const Blob &npcNews, GameVersion version);

    /**
     * Parses MM6's npcbtb.txt ("beg/threaten/bribe") - per-personality flags for which of the three
     * work at all, plus the whole street-dialogue reaction text matrix (greetings by reputation
     * band, refusals, beg/bribe/threat accept and refuse lines). MM6 only.
     */
    void InitializeNPCBtb(const Blob &npcBtb);

    /**
     * Parses MM6's proftext.txt - per-profession weekday small talk. The current weekday's topic
     * labels a street citizen's profession dialogue option, the text is the reply. MM6 only.
     */
    void InitializeMm6ProfText(const Blob &profText);

    void InitializeAdditionalNPCs(NPCData *pNPCDataBuff, MonsterId npc_uid,
                                  HouseId uLocation2D, MapId uMapId);

    /**
     * Generates an MM6 street citizen into `npc`. MM6 street townsfolk aren't npcdata NPCs - the
     * original generates a random citizen when the party first talks to a peasant actor
     * (MM6.EXE 0x469210): name by sex from npcnames.txt, portrait from per-sex pools over the
     * regular npcdata portrait space (tables @0x4C13F0/0x4C15D0), profession weighted by
     * npcprof.txt's "Random Chance" column (same weights on every map), and a rolled reputation
     * requirement (d100: 59% none, 30% > +200, 5% < -300, 3% > +400, 3% < -600) that gates whether
     * the citizen talks to the party at all.
     *
     * @param npc                       Slot in `pAdditionalNPC` to fill.
     * @param sex                       Citizen sex, from the peasant's monster row (the PeasantF / PeasantM models).
     * @param mapId                     Map the citizen lives on.
     */
    void initializeMm6StreetCitizen(NPCData *npc, Sex sex, MapId mapId);

    /**
     * @param sex                       Citizen sex.
     * @return                          MM6's street-citizen portrait pool for that sex - portrait ids into the
     *                                  regular npcXXX space (MM6.EXE tables @0x4C13F0 male / @0x4C15D0 female).
     */
    static std::span<const int> mm6CitizenPortraitPool(Sex sex);

    /**
     * Rolls a random profession weighted by `pProfessionChance` for the given map (MM7: npcdist.txt
     * per-map chances; MM6: npcprof.txt's "Random Chance" column, same on every map).
     *
     * @param mapId                     Map to roll for.
     * @return                          Rolled profession, or `Hunter` (MM7's legacy fallback) when no
     *                                  chances are loaded.
     */
    NpcProfession rollProfession(MapId mapId) const;

    /**
     * @offset 0x476C60
     */
    void setNPCNamesOnLoad();

    /**
     * Returns a random NPC name of the given gender starting with the same letter as `firstLetter`. Backs the
     * `%13` placeholder in `BuildDialogueString` - NPC dialogue templates that address the player by a similar-
     * sounding (mispronounced) name, e.g. "O Ho! %13! Er, %13. I think. Whatever...".
     *
     * The picked name is cached per `firstLetter`, so within a session an NPC mispronounces the player's name
     * the same way every time, and consecutive `%13` substitutions in the same template are consistent.
     *
     * Note: `%13` is only used by MM6's `npcbtb.txt`. MM7 has no templates that invoke this function.
     *
     * @param firstLetter               First letter of the player's name (case-insensitive).
     * @param gender                    Player's gender. Determines which name pool to draw from.
     * @return                          A name from `pNPCNames[gender]` that starts with `firstLetter`, or any
     *                                  random name from that pool if no name with that letter exists.
     * @offset 0x00495366
     */
    const std::string &sub_495366_MispronounceName(char firstLetter, Sex gender);

    /**
     * Picks a random news entry for a street townsfolk chat / tavern rumor: a regional entry for the
     * given map if it has any, else a kingdom-wide rumor (MM6.EXE 0x43BC20 falls back to the map-1
     * pool when the current map has no news of its own). Only populated for MM6 (from npcnews.txt).
     *
     * @param map                       Map the party is on.
     * @return                          The picked entry, or an empty one if no news is loaded.
     */
    RegionalNewsEntry pickRandomNewsEntry(MapId map) const;

    std::array<NPCData, 501> pOriginalNPCData; // NPC data as read from npcdata.txt.
    std::array<NPCData, 501> pNPCData; // NPC data used during the game.
    IndexedArray<std::vector<std::string>, SEX_FIRST, SEX_LAST> pNPCNames = {};
    IndexedArray<NPCProfession, NPC_PROFESSION_FIRST, NPC_PROFESSION_LAST> pProfessions = {};
    // MM6: npcprof.txt's "Personality" column - keys all npcbtb.txt lookups below.
    IndexedArray<NpcPersonality, NPC_PROFESSION_FIRST, NPC_PROFESSION_LAST> mm6PersonalityByProfession = {{}};
    // MM6 npcbtb.txt: which personalities accept begging / bribes / threats at all.
    IndexedArray<bool, PERSONALITY_FIRST, PERSONALITY_LAST> mm6PersonalityAcceptsBeg = {{}};
    IndexedArray<bool, PERSONALITY_FIRST, PERSONALITY_LAST> mm6PersonalityAcceptsBribe = {{}};
    IndexedArray<bool, PERSONALITY_FIRST, PERSONALITY_LAST> mm6PersonalityAcceptsThreat = {{}};
    // MM6 npcbtb.txt reaction texts, indexed by the file's Msg# rows 1..24 (row 0 unused, like
    // MM6.EXE's own Text[25][13] @0x6B99A8): 1/2 greetings, 3/4/5 begged/bribed/threatened-before
    // returns, 6 fame too low, 7-18 reputation greetings and refusals, 19-24 beg/bribe/threat
    // accept and refuse lines.
    std::array<IndexedArray<std::string, PERSONALITY_FIRST, PERSONALITY_LAST>, 25> mm6BtbTexts;
    // MM6 proftext.txt: per-profession weekday small talk, [profession][day of week 0-6, Sunday first].
    IndexedArray<std::array<Mm6ProfDayText, 7>, NPC_PROFESSION_FIRST, NPC_PROFESSION_LAST> mm6ProfText = {{}};
    std::array<NPCData, 100> pAdditionalNPC = {{}};
    std::array<std::string, 52> pCatchPhrases{};   // 15CA4h
    IndexedArray<std::vector<RegionalNewsEntry>, MAP_FIRST, MAP_LAST> pRegionalNews = {{}}; // MM6 npcnews.txt, keyed by map.
    std::vector<RegionalNewsEntry> pGeneralNews; // MM6 npcnews.txt kingdom-wide rumors (its "Map" column = 1).
    std::array<std::string, 500> pNPCUnicNames{};  // from first batch
    IndexedArray<NPCProfessionChance, MAP_FIRST, MAP_LAST> pProfessionChance;
    int field_17884 = 0;
    int field_17888 = 0;
    std::array<NPCGreeting, 206> pNPCGreetings;
    std::array<uint16_t, 51> pOriginalGroups = {{}}; // NPC groups as read from npcgroup.txt.
    std::array<uint16_t, 51> pGroups = {{}}; // NPC groups used during the game.
    int uNewlNPCBufPos = 0;
    int uNumNewNPCs = 0;
    int field_17FC8 = 0;
    int uNumNPCProfessions = 0;

    static int dword_AE336C_LastMispronouncedNameFirstLetter;
    static int dword_AE3370_LastMispronouncedNameResult;
    // MM6: which of the speaker's fame-worthy awards the %08 token names - rolled once per
    // dialogue so the line doesn't change between frames (MM6.EXE cache @0x944C60, reset at
    // dialogue open). -1 = not rolled yet.
    static int mm6LastAddressingAwardPick;
};

extern std::array<NPCTopic, 789> pNPCTopics;
extern NPCStats *pNPCStats;

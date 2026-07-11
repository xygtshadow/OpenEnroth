#pragma once

#include <cstdint>

/**
 * Phrase IDs for phrases displayed in shops when hovering over items.
 *
 * IDs work for selling, buying, repairing and identifying items.
 */
enum class MerchantPhrase {
    MERCHANT_PHRASE_NOT_ENOUGH_GOLD = 0,    // Not used at the moment.
    MERCHANT_PHRASE_PRICE = 1,              // When hovering over an item w/o a merchant skill.
    MERCHANT_PHRASE_PRICE_HAGGLE = 2,       // When hovering over an item while having a merchant skill.
    MERCHANT_PHRASE_PRICE_HAGGLE_TO_ACTUAL_PRICE = 3,   // When hovering over an item while having a merchant skill
                                                        // that reduces the price of an item to its actual price.
    MERCHANT_PHRASE_INCOMPATIBLE_ITEM = 4,  // When hovering over an incompatible item, e.g. a weapon at an armor shop.
    MERCHANT_PHRASE_INVALID_ACTION = 5,     // When hovering over an item that you cannot perform any action on,
                                            // e.g. repairing a non-broken item, or selling a quest item.
    MERCAHNT_PHRASE_STOLEN_ITEM = 6,        // When hovering over a stolen item.

    MERCHANT_PHRASE_FIRST = MERCHANT_PHRASE_NOT_ENOUGH_GOLD,
    MERCHANT_PHRASE_LAST = MERCAHNT_PHRASE_STOLEN_ITEM
};
using enum MerchantPhrase;

// TODO(captainurist): #enum renamings needed
enum class NpcProfession : int32_t {
    NoProfession = 0,
    Smith = 1,       // GM Weapon Repair;
    Armorer = 2,     // GM Armor Repair;
    Alchemist = 3,   // GM Potion Repair;
    Scholar = 4,     // GM Item ID;               Learning: +5
    Guide = 5,       // Travel by foot: -1 day;
    Tracker = 6,     // Travel by foot: -2 days;
    Pathfinder = 7,  // Travel by foot: -3 days;
    Sailor = 8,      // Travel by sea: -2 days;
    Navigator = 9,   // Travel by sea: -3 days;
    Healer = 10,
    ExpertHealer = 11,
    MasterHealer = 12,
    Teacher = 13,        // Learning: +10;
    Instructor = 14,     // Learning: +15;
    Armsmaster = 15,     // Armsmaster: +2;
    Weaponsmaster = 16,  // Armsmaster: +3;
    Apprentice = 17,    // Fire: +2;         Air: +2;    Water: +2;   Earth: +2;
    Mystic = 18,        // Fire: +3;         Air: +3;    Water: +3;   Earth: +3;
    Spellmaster = 19,   // Fire: +4;         Air: +4;    Water: +4;   Earth: +4;
    Trader = 20,        // Merchant: +4;
    Merchant = 21,      // Merchant: +6;
    Scout = 22,         // Perception: +6;
    Herbalist = 23,     // Alchemy: +4;
    Apothecary = 24,    // Alchemy: +8;
    Tinker = 25,        // Traps: +4;
    Locksmith = 26,     // Traps: +6;
    Fool = 27,          // Luck: +5;
    ChimneySweep = 28,  // Luck: +20;
    Porter = 29,        // Food for rest: -1;
    QuarterMaster = 30,  // Food for rest: -2;
    Factor = 31,         // Gold finds: +10%;
    Banker = 32,         // Gold finds: +20%;
    Cook = 33,
    Chef = 34,
    Horseman = 35,  // Travel by foot: -2 days;
    Bard = 36,
    Enchanter = 37,     // Resist All: +20;
    Cartographer = 38,  // Wizard Eye level 2;
    WindMaster = 39,
    WaterMaster = 40,
    GateMaster = 41,
    Acolyte = 42,
    Piper = 43,
    Explorer = 44,  // Travel by foot -1 day;     Travel by sea: -1 day;
    Pirate = 45,    // Travel by sea: -2 days;    Gold finds: +10%; Reputation: +5;
    Squire = 46,
    Psychic = 47,  // Perception: +5;            Luck: +10;
    Gypsy = 48,    // Food for rest: -1;         Merchant: +3; Reputation: +5;
    Diplomat = 49,
    Duper = 50,    // Merchant: +8;              Reputation: +5;
    Burglar = 51,  // Traps: +8;                 Stealing: +8; Reputation: +5;
    FallenWizard = 52,  // Reputation: +5;
    Acolyte2 = 53,  // Spirit: +2;                Mind: +2;              Body: +2;
    Initiate = 54,  // Spirit: +3;                Mind: +3;              Body: +3;
    Prelate = 55,      // Spirit: +4;                Mind: +4;              Body: +4;
    Monk = 56,   // Unarmed: +2;               Dodge: +2;
    Sage = 57,   // Monster ID: +6
    Hunter = 58,  // Monster ID: +6

    // MM6-only professions. MM6's npcprof.txt lists 77 professions whose ids 1-22, 25-48 and 50-51
    // coincide with MM7's; the ids below exist only in MM6 (or, like Hunter2, share a name but not
    // MM7's in-party benefit). Their numeric values are engine-internal - MM6 file ids are translated
    // via npcProfessionFromMm6Id(). None of them have an in-party benefit (commoner trades).
    Counselor = 59,      // MM6 id 23 (MM7 23 = Herbalist).
    Barrister = 60,      // MM6 id 24 (MM7 24 = Apothecary).
    Negotiator = 61,     // MM6 id 49 (MM7 49 = Diplomat).
    Peasant = 62,        // MM6 id 52 (MM7 52 = Fallen Wizard).
    Serf = 63,           // MM6 id 53.
    Tailor = 64,         // MM6 id 54.
    Laborer = 65,        // MM6 id 55.
    Farmer = 66,         // MM6 id 56.
    Cooper = 67,         // MM6 id 57.
    Potter = 68,         // MM6 id 58.
    Weaver = 69,         // MM6 id 59.
    Cobbler = 70,        // MM6 id 60.
    DitchDigger = 71,    // MM6 id 61.
    Miller = 72,         // MM6 id 62.
    Carpenter = 73,      // MM6 id 63.
    StoneCutter = 74,    // MM6 id 64.
    Jester = 75,         // MM6 id 65.
    Trapper = 76,        // MM6 id 66.
    Beggar = 77,         // MM6 id 67.
    Rustler = 78,        // MM6 id 68.
    Hunter2 = 79,        // MM6 id 69 - MM6's Hunter is a benefit-less commoner, unlike MM7's (Monster ID +6).
    Scribe = 80,         // MM6 id 70.
    Missionary = 81,     // MM6 id 71.
    Clerk = 82,          // MM6 id 72.
    Guard = 83,          // MM6 id 73.
    FollowerOfBaa = 84,  // MM6 id 74.
    Noble = 85,          // MM6 id 75.
    Gambler = 86,        // MM6 id 76.
    Child = 87,          // MM6 id 77.

    NPC_PROFESSION_FIRST = Smith,
    NPC_PROFESSION_LAST = Child
};
using enum NpcProfession;

// MM6 NPC personality - the "Personality" column of MM6's npcprof.txt, one per profession. Keys
// npcbtb.txt: whether an NPC accepts begging / bribes / threats, and every one of its greeting and
// reaction lines. Numeric values are the in-memory order of MM6.EXE's BTB tables @0x6B9980 (the
// file's column order differs - see the parser); MM6.EXE remaps columns via the table @0x4C1036.
enum class NpcPersonality {
    PERSONALITY_ADVENTURER = 0,
    PERSONALITY_EVIL_FANATIC = 1, // npcprof.txt spells it "Fanatic".
    PERSONALITY_GUARD = 2,
    PERSONALITY_MERCHANT = 3,
    PERSONALITY_NOBLE = 4,
    PERSONALITY_OFFICIAL = 5,
    PERSONALITY_PALADIN = 6,
    PERSONALITY_PEASANT = 7,
    PERSONALITY_PRIEST = 8,
    PERSONALITY_SCHOLAR = 9,
    PERSONALITY_SORCERER = 10,
    PERSONALITY_THIEF = 11,
    PERSONALITY_MONSTER = 12, // Not used by any npcprof.txt row; npcbtb.txt still has lines for it.

    PERSONALITY_FIRST = PERSONALITY_ADVENTURER,
    PERSONALITY_LAST = PERSONALITY_MONSTER
};
using enum NpcPersonality;

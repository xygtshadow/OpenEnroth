#include "NPCTopics.h"

#include <array>
#include <utility>
#include <string>
#include <vector>

#include "Engine/ArenaEnumFunctions.h"
#include "Engine/AssetsManager.h"
#include "Engine/Engine.h"
#include "Engine/Graphics/Sprites.h"
#include "Engine/Graphics/Outdoor.h"
#include "Engine/Graphics/Indoor.h"
#include "Engine/Graphics/Viewport.h"
#include "Engine/Graphics/Renderer/Renderer.h"
#include "Engine/Objects/Decoration.h"
#include "Engine/Localization.h"
#include "Engine/Objects/Actor.h"
#include "Engine/Objects/NPC.h"
#include "Engine/Objects/CharacterEnumFunctions.h"
#include "Engine/Objects/MonsterEnumFunctions.h"
#include "Engine/Party.h"
#include "Engine/Data/HouseEnumFunctions.h"
#include "Engine/Graphics/LocationFunctions.h"
#include "Engine/Tables/ItemTable.h"
#include "Engine/Evt/Processor.h"
#include "Engine/Random/Random.h"

#include "GUI/GUIWindow.h"
#include "GUI/GUIMessageQueue.h"
#include "GUI/UI/UIDialogue.h"
#include "GUI/UI/UIHouses.h"
#include "GUI/UI/UIStatusBar.h"
#include "GUI/UI/Houses/TownHall.h"

#include "Media/Audio/AudioPlayer.h"

#include "Utility/String/Ascii.h"

// MM6's Beg / Threat / Bribe option labels sit at global.txt rows MM7 reuses for class names and
// settings text, so they have no shared LSTR_* ids - same situation as Tavern.cpp's Tip Barkeep.
static constexpr LstrId MM6_LSTR_BEG = static_cast<LstrId>(27);
static constexpr LstrId MM6_LSTR_BRIBE = static_cast<LstrId>(31);
static constexpr LstrId MM6_LSTR_THREAT = static_cast<LstrId>(226);

int membershipOrTrainingApproved;
int topicEventId; // event id of currently viewed scripted NPC event
DialogueId guildMembershipNPCTopicId;

int gold_transaction_amount;

static constexpr std::array<Vec2f, 20> pMonsterArenaPlacements = {{
    Vec2f(1524, 8332),    Vec2f(2186, 8844),
    Vec2f(3219, 9339),    Vec2f(4500, 9339),
    Vec2f(5323, 9004),    Vec2f(0x177D, 0x2098),
    Vec2f(0x50B, 0x1E15), Vec2f(0x18FF, 0x1E15),
    Vec2f(0x50B, 0xD69),  Vec2f(0x18FF, 0x1B15),
    Vec2f(0x50B, 0x1021), Vec2f(0x18FF, 0x1848),
    Vec2f(0x50B, 0x12D7), Vec2f(0x18FF, 0x15A3),
    Vec2f(0x50B, 0x14DB), Vec2f(0x18FF, 0x12D7),
    Vec2f(0x50B, 0x1848), Vec2f(0x18FF, 0x1021),
    Vec2f(0x50B, 0x1B15), Vec2f(0x18FF, 0xD69),
}};

static constexpr IndexedArray<int, GUILD_FIRST, GUILD_LAST> priceForMembership = {{
    {GUILD_OF_ELEMENTS, 100},
    {GUILD_OF_SELF,     100},
    {GUILD_OF_AIR,      50},
    {GUILD_OF_EARTH,    50},
    {GUILD_OF_FIRE,     50},
    {GUILD_OF_WATER,    50},
    {GUILD_OF_BODY,     50},
    {GUILD_OF_MIND,     50},
    {GUILD_OF_SPIRIT,   50},
    {GUILD_OF_LIGHT,    1000},
    {GUILD_OF_DARK,     1000}
}};

static constexpr IndexedArray<int, SKILL_FIRST, SKILL_LAST> expertSkillMasteryCost = {{
    {SKILL_STAFF,        2000},
    {SKILL_SWORD,        2000},
    {SKILL_DAGGER,       2000},
    {SKILL_AXE,          2000},
    {SKILL_SPEAR,        2000},
    {SKILL_BOW,          2000},
    {SKILL_MACE,         2000},
    {SKILL_BLASTER,      0},
    {SKILL_SHIELD,       1000},
    {SKILL_LEATHER,      1000},
    {SKILL_CHAIN,        1000},
    {SKILL_PLATE,        1000},
    {SKILL_FIRE,         1000},
    {SKILL_AIR,          1000},
    {SKILL_WATER,        1000},
    {SKILL_EARTH,        1000},
    {SKILL_SPIRIT,       1000},
    {SKILL_MIND,         1000},
    {SKILL_BODY,         1000},
    {SKILL_LIGHT,        2000},
    {SKILL_DARK,         2000},
    {SKILL_ITEM_ID,      500},
    {SKILL_MERCHANT,     2000},
    {SKILL_REPAIR,       500},
    {SKILL_BODYBUILDING, 500},
    {SKILL_MEDITATION,   500},
    {SKILL_PERCEPTION,   500},
    {SKILL_DIPLOMACY,    0}, // not used
    {SKILL_THIEVERY,     0}, // not used
    {SKILL_TRAP_DISARM,  500},
    {SKILL_DODGE,        2000},
    {SKILL_UNARMED,      2000},
    {SKILL_MONSTER_ID,   500},
    {SKILL_ARMSMASTER,   2000},
    {SKILL_STEALING,     500},
    {SKILL_ALCHEMY,      500},
    {SKILL_LEARNING,     2000},
    {SKILL_CLUB,         500},
    {SKILL_MISC,         0} // hidden, not used
}};

static constexpr IndexedArray<int, SKILL_FIRST, SKILL_LAST> masterSkillMasteryCost = {{
    {SKILL_STAFF,        5000},
    {SKILL_SWORD,        5000},
    {SKILL_DAGGER,       5000},
    {SKILL_AXE,          5000},
    {SKILL_SPEAR,        5000},
    {SKILL_BOW,          5000},
    {SKILL_MACE,         5000},
    {SKILL_BLASTER,      0},
    {SKILL_SHIELD,       3000},
    {SKILL_LEATHER,      3000},
    {SKILL_CHAIN,        3000},
    {SKILL_PLATE,        3000},
    {SKILL_FIRE,         4000},
    {SKILL_AIR,          4000},
    {SKILL_WATER,        4000},
    {SKILL_EARTH,        4000},
    {SKILL_SPIRIT,       4000},
    {SKILL_MIND,         4000},
    {SKILL_BODY,         4000},
    {SKILL_LIGHT,        5000},
    {SKILL_DARK,         5000},
    {SKILL_ITEM_ID,      2500},
    {SKILL_MERCHANT,     5000},
    {SKILL_REPAIR,       2500},
    {SKILL_BODYBUILDING, 2500},
    {SKILL_MEDITATION,   2500},
    {SKILL_PERCEPTION,   2500},
    {SKILL_DIPLOMACY,    0}, // not used
    {SKILL_THIEVERY,     0}, // not used
    {SKILL_TRAP_DISARM,  2500},
    {SKILL_DODGE,        5000},
    {SKILL_UNARMED,      5000},
    {SKILL_MONSTER_ID,   2500},
    {SKILL_ARMSMASTER,   5000},
    {SKILL_STEALING,     2500},
    {SKILL_ALCHEMY,      2500},
    {SKILL_LEARNING,     5000},
    {SKILL_CLUB,         2500},
    {SKILL_MISC,         0} // hidden, not used
}};

static constexpr IndexedArray<int, SKILL_FIRST, SKILL_LAST> grandmasterSkillMasteryCost = {{
    {SKILL_STAFF,        8000},
    {SKILL_SWORD,        8000},
    {SKILL_DAGGER,       8000},
    {SKILL_AXE,          8000},
    {SKILL_SPEAR,        8000},
    {SKILL_BOW,          8000},
    {SKILL_MACE,         8000},
    {SKILL_BLASTER,      0},
    {SKILL_SHIELD,       7000},
    {SKILL_LEATHER,      7000},
    {SKILL_CHAIN,        7000},
    {SKILL_PLATE,        7000},
    {SKILL_FIRE,         8000},
    {SKILL_AIR,          8000},
    {SKILL_WATER,        8000},
    {SKILL_EARTH,        8000},
    {SKILL_SPIRIT,       8000},
    {SKILL_MIND,         8000},
    {SKILL_BODY,         8000},
    {SKILL_LIGHT,        8000},
    {SKILL_DARK,         8000},
    {SKILL_ITEM_ID,      6000},
    {SKILL_MERCHANT,     8000},
    {SKILL_REPAIR,       6000},
    {SKILL_BODYBUILDING, 6000},
    {SKILL_MEDITATION,   6000},
    {SKILL_PERCEPTION,   6000},
    {SKILL_DIPLOMACY,    0}, // not used
    {SKILL_THIEVERY,     0}, // not used
    {SKILL_TRAP_DISARM,  6000},
    {SKILL_DODGE,        8000},
    {SKILL_UNARMED,      8000},
    {SKILL_MONSTER_ID,   6000},
    {SKILL_ARMSMASTER,   8000},
    {SKILL_STEALING,     6000},
    {SKILL_ALCHEMY,      6000},
    {SKILL_LEARNING,     8000},
    {SKILL_CLUB,         6000},
    {SKILL_MISC,         0} // hidden, not used
}};

static constexpr std::array<std::pair<QuestBit, ItemId>, 27> _4F0882_evt_VAR_PlayerItemInHands_vals = {{
    {QBIT_212, ITEM_QUEST_VASE},
    {QBIT_213, ITEM_SPECIAL_LADY_CARMINES_DAGGER},
    {QBIT_214, ITEM_MESSAGE_SCROLL_OF_WAVES},
    {QBIT_215, ITEM_MESSAGE_CIPHER},
    {QBIT_216, ITEM_QUEST_WORN_BELT},
    {QBIT_217, ITEM_QUEST_HEART_OF_THE_WOOD},
    {QBIT_218, ITEM_MESSAGE_MAP_TO_EVENMORN_ISLAND},
    {QBIT_219, ITEM_QUEST_GOLEM_HEAD},
    {QBIT_220, ITEM_QUEST_ABBEY_NORMAL_GOLEM_HEAD},
    {QBIT_221, ITEM_QUEST_GOLEM_RIGHT_ARM},
    {QBIT_222, ITEM_QUEST_GOLEM_LEFT_ARM},
    {QBIT_223, ITEM_QUEST_GOLEM_RIGHT_LEG},
    {QBIT_224, ITEM_QUEST_GOLEM_LEFT_LEG},
    {QBIT_225, ITEM_QUEST_GOLEM_CHEST},
    {QBIT_226, ITEM_SPELLBOOK_DIVINE_INTERVENTION},
    {QBIT_227, ITEM_QUEST_DRAGON_EGG},
    {QBIT_228, ITEM_QUEST_ZOKARR_IVS_SKULL},
    {QBIT_229, ITEM_QUEST_LICH_JAR_EMPTY},
    {QBIT_230, ITEM_QUEST_ELIXIR},
    {QBIT_231, ITEM_QUEST_CASE_OF_SOUL_JARS},
    {QBIT_232, ITEM_QUEST_ALTAR_PIECE_1},
    {QBIT_233, ITEM_QUEST_ALTAR_PIECE_2},
    {QBIT_234, ITEM_QUEST_CONTROL_CUBE},
    {QBIT_235, ITEM_QUEST_WETSUIT},
    {QBIT_236, ITEM_QUEST_OSCILLATION_OVERTHRUSTER},
    {QBIT_237, ITEM_QUEST_LICH_JAR_FULL},
    {QBIT_241, ITEM_SPECIAL_THE_PERFECT_BOW}
}};

DialogueId arenaMainDialogue() {
    if (pParty->arenaState == ARENA_STATE_INITIAL)
        return DIALOGUE_ARENA_WELCOME;

    if (pParty->arenaState == ARENA_STATE_WON)
        return DIALOGUE_ARENA_ALREADY_WON;

    assert(pParty->arenaState == ARENA_STATE_FIGHTING);

    int killedMonsters = 0;
    for (Actor &actor : pActors) {
        if (actor.aiState == Dead ||
            actor.aiState == Removed ||
            actor.aiState == Disabled ||
            (actor.summonerId && actor.summonerId.type() == OBJECT_Character)) {
            killedMonsters++;
        }
    }

    if (killedMonsters >= pActors.size() || pActors.size() <= 0) {
        pParty->uNumArenaWins[pParty->arenaLevel]++;
        // MM6's arena-victor awards are its own awards.txt rows 84-87 ("%u Page Arena Victories"...,
        // MM6.EXE 0x4A342A grants state byte + 4); MM7's live at rows 88-91 behind the AwardId enum.
        int awardId = engine->gameVersion() == GAME_VERSION_MM6
            ? 83 + std::to_underlying(pParty->arenaLevel)
            : std::to_underlying(awardForArenaLevel(pParty->arenaLevel));
        for (Character &player : pParty->pCharacters) {
            player.SetVariable(VAR_Award, awardId);
        }
        pParty->partyFindsGold(gold_transaction_amount, GOLD_RECEIVE_SHARE);
        pAudioPlayer->playUISound(SOUND_51heroism03);
        pParty->arenaState = ARENA_STATE_WON;
        pParty->arenaLevel = ARENA_LEVEL_INVALID;
        return DIALOGUE_ARENA_REWARD;
    } else {
        pParty->pos = Vec3f(3849, 5770, 1);
        pParty->velocity = Vec3f();
        pParty->uFallStartZ = 1;
        pParty->_viewYaw = 512;
        pParty->_viewPitch = 0;
        pAudioPlayer->playUISound(SOUND_51heroism03);
        engine->_messageQueue->addMessageCurrentFrame(UIMSG_Escape, 1, 0);
        return DIALOGUE_NULL;
    }
}

/**
 * @offset 0x4BC109
 */
void prepareArenaFight(ArenaLevel level) {
    pParty->arenaState = ARENA_STATE_FIGHTING;
    pParty->arenaLevel = level;

    // TODO(pskelton): This doesnt work properly and we dont want draw calls here
    render->BeginScene3D();
    if (uCurrentlyLoadedLevelType == LEVEL_INDOOR) {
        pIndoor->Draw();
    } else if (uCurrentlyLoadedLevelType == LEVEL_OUTDOOR) {
        pOutdoor->Draw();
    }
    render->DrawBillboards_And_MaybeRenderSpecialEffects_And_EndScene();
    render->BeginScene2D();
    pDialogueWindow->DrawDialoguePanel(localization->str(LSTR_PLEASE_WAIT_WHILE_I_SUMMON_THE_MONSTERS));
    render->Present();

    pParty->pos = Vec3f(3849, 5770, 1); // TODO(pskelton) :: extract this common teleport to func
    pParty->velocity = Vec3f();
    pParty->uFallStartZ = 1;
    pParty->_viewYaw = 512;
    pParty->_viewPitch = 0;
    engine->_messageQueue->addMessageCurrentFrame(UIMSG_Escape, 1, 0);

    int characterMaxLevel = 0;
    for (Character &character : pParty->pCharacters) {
        if (characterMaxLevel < character.GetActualLevel()) {
            characterMaxLevel = character.GetActualLevel();
        }
    }

    bool isMm6 = engine->gameVersion() == GAME_VERSION_MM6;

    int monsterMaxLevel = characterMaxLevel;
    int monsterMinLevel = characterMaxLevel / 2;

    switch(level) {
    case ARENA_LEVEL_PAGE:
        monsterMaxLevel = characterMaxLevel;
        break;
    case ARENA_LEVEL_SQUIRE:
        monsterMaxLevel = characterMaxLevel * 1.5;
        break;
    case ARENA_LEVEL_KNIGHT:
        monsterMaxLevel = characterMaxLevel * 2;
        break;
    case ARENA_LEVEL_LORD:
        // MM6's Lord tier doesn't halve the lower bound (MM6.EXE 0x4A39B0: min = max party level).
        if (isMm6)
            monsterMinLevel = characterMaxLevel;
        monsterMaxLevel = characterMaxLevel * 2;
        break;
    default:
        assert(false);
    }

    // MM6.EXE 0x4A39C0 clamps to [1, 100]; MM7 raised the floor to 2.
    int minLevelFloor = isMm6 ? 1 : 2;
    if (monsterMinLevel < minLevelFloor)
        monsterMinLevel = minLevelFloor;
    if (monsterMinLevel > 100)
        monsterMinLevel = 100;

    if (monsterMaxLevel > 100)
        monsterMaxLevel = 100;
    if (monsterMaxLevel < minLevelFloor)
        monsterMaxLevel = minLevelFloor;

    std::vector<MonsterId> candidateIds;
    if (isMm6) {
        // MM6.EXE 0x4A39DC: every monsters.txt row 1-171 whose level fits the window is a candidate -
        // no arena-suitability flag, only the special endgame rows past 171 are excluded.
        for (int i = 1; i <= 171; i++) {
            MonsterId id = static_cast<MonsterId>(i);
            if (pMonsterStats->infos[id].level >= monsterMinLevel && pMonsterStats->infos[id].level <= monsterMaxLevel)
                candidateIds.push_back(id);
        }
    } else {
        for (MonsterId i : allArenaMonsters()) {
            if (pMonsterStats->infos[i].level >= monsterMinLevel && pMonsterStats->infos[i].level <= monsterMaxLevel) {
                candidateIds.push_back(i);
            }
        }
    }
    assert(!candidateIds.empty());

    int maxIdsNum = 6;
    if (candidateIds.size() < 6) {
        maxIdsNum = candidateIds.size();
    }

    std::vector<MonsterId> monsterIds;
    for (int i = 0; i < maxIdsNum; i++) {
        monsterIds.push_back(grng->randomSample(candidateIds));
    }

    int baseReward = 0, monstersNum = 0;

    if (level == ARENA_LEVEL_PAGE) {
        baseReward = 50;
        monstersNum = grng->random(3) + 6; // [6:8] monsters
    } else if (level == ARENA_LEVEL_SQUIRE) {
        baseReward = 100;
        monstersNum = grng->random(7) + 6; // [6:12] monsters
    } else if (level == ARENA_LEVEL_KNIGHT) {
        baseReward = 200;
        monstersNum = grng->random(11) + 10; // [10:19] monsters
    } else if (level == ARENA_LEVEL_LORD) {
        baseReward = 500;
        monstersNum = 20;
    }

    gold_transaction_amount = characterMaxLevel * baseReward;

    for (int i = 0; i < monstersNum; ++i) {
        Vec2f pos = pMonsterArenaPlacements[i];
        Actor::Arena_summon_actor(grng->randomSample(monsterIds), Vec3f(pos.x, pos.y, 1));
    }
    pAudioPlayer->playUISound(SOUND_51heroism03);
}

/**
 * @offset 0x004B1ECE.
 *
 * @brief Oracle's 'I lost it!' dialog option
 */
void oracleDialogue() {
    ItemId item_id = ITEM_NULL;

    // display "You never had it" if nothing missing will be found
    current_npc_text = pNPCTopics[667].pText;

    // only items with special subquest in range 212-237 and also 241 are recoverable
    for (auto pair : _4F0882_evt_VAR_PlayerItemInHands_vals) {
        QuestBit quest_id = pair.first;
        if (pParty->_questBits[quest_id]) {
            ItemId search_item_id = pair.second;
            if (!pParty->hasItem(search_item_id) && pParty->pPickedItem.itemId != search_item_id) {
                item_id = search_item_id;
                break;
            }
        }
    }

    // missing item found
    if (item_id != ITEM_NULL) {
        pParty->pCharacters[0].AddVariable(VAR_PlayerItemInHands, std::to_underlying(item_id));
        // TODO(captainurist): what if fmt throws?
        current_npc_text = fmt::sprintf(pNPCTopics[666].pText, // "Here's %s that you lost. Be careful" // NOLINT: this is not ::sprintf.
                                        fmt::format("{::}{}\f00000", colorTable.Sunflower.tag(),
                                                    pItemTable->items[item_id].unidentifiedName));
    }

    // missing item is lich jar and we need to bind soul vessel to lich class character
    // TODO(Nik-RE-dev): this code is walking only through inventory, but item was added to hand, so it will not bind new item if it was acquired
    //                   rather this code will bind jars that already present in inventory to liches that currently do not have binded jars
    if (item_id == ITEM_QUEST_LICH_JAR_FULL) {
        for (int i = 0; i < pParty->pCharacters.size(); i++) {
            if (pParty->pCharacters[i].classType == CLASS_LICH) {
                bool have_vessels_soul = false;
                InventoryEntry jar;
                for (Character &player : pParty->pCharacters) {
                    for (InventoryEntry entry : player.inventory.entries(ITEM_QUEST_LICH_JAR_FULL)) {
                        if (entry->lichJarCharacterIndex == -1)
                            jar = entry;
                        if (entry->lichJarCharacterIndex == i)
                            have_vessels_soul = true;
                    }
                }

                if (jar && !have_vessels_soul) {
                    jar->lichJarCharacterIndex = i;
                    break;
                }
            }
        }
    }
}

// The Seer's lost-quest-item table (MM6.EXE 0x4C3DAC, 25 (quest bit, item) word pairs read by the
// "I lost it" handler @0x496570): while the quest bit is set and no character carries the item,
// the Seer hands out a replacement. Quest bit 181 is set in the new-game template - it arms the
// replacement of The Letter (item 505), the party's starting quest item.
static constexpr std::array<std::pair<QuestBit, ItemId>, 25> mm6SeerLostItemPairs = {{
    {QuestBit(181), ItemId(505)}, {QuestBit(182), ItemId(499)}, {QuestBit(183), ItemId(433)},
    {QuestBit(184), ItemId(506)}, {QuestBit(185), ItemId(455)}, {QuestBit(186), ItemId(457)},
    {QuestBit(187), ItemId(508)}, {QuestBit(188), ItemId(434)}, {QuestBit(189), ItemId(486)},
    {QuestBit(190), ItemId(502)}, {QuestBit(191), ItemId(550)}, {QuestBit(192), ItemId(551)},
    {QuestBit(193), ItemId(552)}, {QuestBit(194), ItemId(553)}, {QuestBit(195), ItemId(456)},
    {QuestBit(196), ItemId(446)}, {QuestBit(197), ItemId(461)}, {QuestBit(198), ItemId(544)},
    {QuestBit(199), ItemId(487)}, {QuestBit(229), ItemId(538)}, {QuestBit(230), ItemId(542)},
    {QuestBit(231), ItemId(537)}, {QuestBit(232), ItemId(539)}, {QuestBit(233), ItemId(541)},
    {QuestBit(234), ItemId(540)},
}};

// MM6's Seer "I lost it" topic (npc topic 45, MM6.EXE handler @0x496570) - the ancestor of MM7's
// Oracle "I lost it!" dialogue above. The Ritual of the Void (item 544) is checked first with its
// own gate: re-given while the endgame is armed (quest bit 177) and not yet performed (bit 237).
static void mm6SeerLostItemDialogue() {
    current_npc_text = pNPCTopics[174].pText; // npctext row 175: "You never found it."

    auto missing = [](ItemId itemId) {
        return !pParty->hasItem(itemId) && pParty->pPickedItem.itemId != itemId;
    };

    ItemId itemId = ITEM_NULL;
    if (!pParty->_questBits[QuestBit(237)] && pParty->_questBits[QuestBit(177)] && missing(ItemId(544))) {
        itemId = ItemId(544); // Ritual of the Void.
    } else {
        for (auto [questBit, pairItemId] : mm6SeerLostItemPairs) {
            if (pParty->_questBits[questBit] && missing(pairItemId)) {
                itemId = pairItemId;
                break;
            }
        }
    }

    if (itemId != ITEM_NULL) {
        pParty->pCharacters[0].AddVariable(VAR_PlayerItemInHands, std::to_underlying(itemId));
        // npctext row 176: "Here is the %s you have misplaced.  You must be more careful in the future."
        current_npc_text = fmt::sprintf(pNPCTopics[175].pText, // NOLINT: this is not ::sprintf.
                                        fmt::format("{::}{}\f00000", colorTable.Sunflower.tag(),
                                                    pItemTable->items[itemId].unidentifiedName));
    }
}

// The shrine a pilgrimage visits is keyed to the CALENDAR month (the shrine events compare
// MonthIs): months 0-6 are the seven stat shrines in display order, 7-11 the five resistance
// shrines. Name sources per MM6.EXE 0x49786f: the stat-name table for 0-6, global.txt rows
// 87/71/43/166/138 for 7-11.
static std::string mm6ShrineNameForMonth(int month) {
    if (month <= 6)
        return localization->attributeName(static_cast<Attribute>(month));
    static constexpr std::array<LstrId, 5> resistanceRows = {
        LstrId(87), LstrId(71), LstrId(43), LstrId(166), LstrId(138)}; // Fire/Electricity/Cold/Poison/Magic.
    return localization->str(resistanceRows[month - 7]);
}

// MM6's Seer "Pilgrimage" topic (npc topic 41, MM6.EXE handler @0x4A2F20): the Seer names the
// current month's shrine, and - once a month, on the first visit in a new month - resets the
// pilgrimage quest bits so the shrines' blessing can be collected again (the shrine events gate on
// bit 206; bit 205 is cleared alongside but nothing in the shipped game sets or reads it). The
// reset timestamp lives in MM6's own party struct, not the MM7 save format OE serializes, so like
// the tavern state it does not survive save/load.
static void mm6SeerPilgrimageDialogue() {
    if (pParty->_mm6SeerNextPilgrimageReset < pParty->GetPlayingTime()) {
        pParty->_questBits[QuestBit(205)] = false;
        pParty->_questBits[QuestBit(206)] = false;
        pParty->_mm6SeerNextPilgrimageReset = Time::fromMonths(pParty->GetPlayingTime().toMonths() + 1);
    }

    if (pParty->_questBits[QuestBit(206)]) {
        current_npc_text = pNPCTopics[54].pText; // npctext row 55: "You must wait until the new month..."
    } else {
        // npctext row 54: "This is %s, the month of %s.  Journey to the Shrine of %s and pray there
        // to be rewarded." - the EXE passes (month name, shrine name, shrine name).
        std::string shrine = mm6ShrineNameForMonth(pParty->uCurrentMonth);
        current_npc_text = fmt::sprintf(pNPCTopics[53].pText, // NOLINT: this is not ::sprintf.
                                        localization->monthName(pParty->uCurrentMonth), shrine, shrine);
    }
}

/**
 * @offset 0x4B29F2
 */
// MM6 guild memberships, indexed by join topic id - 381 (MM6.EXE join prices @0x4C3E10, award
// bit = 64 + index, set for the whole party @0x496a96). The 17 organizations: Elemental & Self
// guilds (the tier-2 magic guilds), the three thief and three fighter guilds, the nine
// school-of-magic guilds, and the Light & Dark guilds.
static constexpr std::array<int, 17> mm6GuildJoinPrices = {
    100, 100, 25, 50, 50, 25, 50, 50, 50, 50, 50, 50, 50, 50, 50, 1000, 1000};

const std::string &joinGuildOptionString() {
    if (engine->gameVersion() == GAME_VERSION_MM6) {
        int guildIndex = topicEventId - 381;
        AwardId guildMembershipAwardBit = static_cast<AwardId>(64 + guildIndex);

        membershipOrTrainingApproved = false;
        gold_transaction_amount = mm6GuildJoinPrices[guildIndex];

        if (!pParty->hasActiveCharacter())
            pParty->setActiveToFirstCanAct();  // avoid nzi

        if (pParty->activeCharacter().CanAct()) {
            if (pParty->activeCharacter()._achievedAwardsBits[guildMembershipAwardBit]) {
                return pNPCTopics[119].pText; // The already-a-member brush-off (npctext row 120).
            } else if (gold_transaction_amount <= pParty->GetGold()) {
                membershipOrTrainingApproved = true;
                return pNPCTopics[154 + guildIndex].pText; // "Join <guild> for <N> gold" (npctext rows 155-171).
            } else {
                return localization->str(LSTR_YOU_DONT_HAVE_ENOUGH_GOLD);
            }
        }
        return pNPCTopics[119].pText;
    }

    GuildId guild_id = static_cast<GuildId>(topicEventId - 400);
    static const int dialogue_base = 110;
    AwardId guildMembershipAwardBit = membershipAwardForGuild(guild_id);

    membershipOrTrainingApproved = false;
    gold_transaction_amount = priceForMembership[guild_id];

    // TODO(pskelton): check this behaviour
    if (!pParty->hasActiveCharacter())
        pParty->setActiveToFirstCanAct();  // avoid nzi

    if (pParty->activeCharacter().CanAct()) {
        if (pParty->activeCharacter()._achievedAwardsBits[guildMembershipAwardBit]) {
            return pNPCTopics[dialogue_base + 13].pText;
        } else {
            if (gold_transaction_amount <= pParty->GetGold()) {
                membershipOrTrainingApproved = true;
                return pNPCTopics[dialogue_base + std::to_underlying(guild_id)].pText;
            } else {
                return pNPCTopics[dialogue_base + 14].pText;
            }
        }
    } else {
        return pNPCTopics[dialogue_base + 12].pText;
    }
}

// The MM6 skill taught by NPC-teacher topic 200-259: 200 + 2 x skill slot (+1 for the master
// teacher), with slot 28 = Thievery skipped - a slot above 27 is incremented, so topics 256-259
// teach Disarm Traps and Learning (MM6.EXE 0x496c90). MM6 skill slots 0-29 coincide with the
// engine's Skill enum; slot 30 is Learning.
static Skill mm6TeacherSkill(int topicId) {
    int slot = (topicId - 200) / 2;
    if (slot > 27)
        slot++;
    return slot == 30 ? SKILL_LEARNING : static_cast<Skill>(slot);
}

static bool mm6IsMasterTeacherTopic(int topicId) {
    return (topicId - 200) % 2 != 0;
}

// MM6.EXE 0x496c90, the NPC skill-teacher gate: computes the price, checks every prerequisite on
// the ACTIVE character, and returns the string shown as the clickable option - one of the npctext
// 260-266 refusals, or "Learn" (global.txt 535) with membershipOrTrainingApproved set. Unlike the
// guild learn dialogues there is no merchant discount, and the class-can-learn table plays no
// part - the only class gates are the per-skill promotion checks below.
static std::string mm6MasteryTeacherOptionString() {
    Skill skill = mm6TeacherSkill(topicEventId);
    bool master = mm6IsMasterTeacherTopic(topicEventId);

    membershipOrTrainingApproved = false;

    if (!pParty->hasActiveCharacter())
        pParty->setActiveToFirstCanAct();
    Character *student = &pParty->activeCharacter();

    // Not in your condition!
    if (!student->CanAct())
        return pNPCTopics[263].pText; // npctext row 264.

    int level = student->getSkillValue(skill).level();
    Mastery mastery = student->getSkillValue(skill).mastery();

    // You must know the skill before you can become an expert in it!
    if (!level)
        return pNPCTopics[261].pText; // npctext row 262.

    // You are already an expert / a master in this skill.
    if (mastery >= (master ? MASTERY_MASTER : MASTERY_EXPERT))
        return pNPCTopics[master ? 265 : 264].pText; // npctext rows 266 / 265.

    // You must first be an expert in the skill before you can become a master.
    if (master && mastery < MASTERY_EXPERT)
        return pNPCTopics[262].pText; // npctext row 263.

    auto isMm6Class = [&](int mm6ClassByte) {
        return student->classType == classFromMm6ClassByte(mm6ClassByte);
    };
    auto hasAward = [&](int awardId) {
        return static_cast<bool>(student->_achievedAwardsBits[static_cast<AwardId>(awardId)]);
    };
    auto carriesBlaster = [&] {
        for (InventoryConstEntry entry : student->inventory.entries())
            if (pItemTable->items[entry->itemId].skill == SKILL_BLASTER)
                return true;
        return false;
    };

    bool canLearn;
    int price;
    if (!master) {
        // Expert: rank 4 everywhere; the price is per skill group (jump table @0x497214).
        canLearn = level >= 4;
        switch (skill) {
          case SKILL_LEATHER: case SKILL_CHAIN: case SKILL_PLATE:
          case SKILL_FIRE: case SKILL_AIR: case SKILL_WATER: case SKILL_EARTH:
          case SKILL_SPIRIT: case SKILL_MIND: case SKILL_BODY:
            price = 1000;
            break;
          case SKILL_ITEM_ID: case SKILL_REPAIR: case SKILL_BODYBUILDING:
          case SKILL_MEDITATION: case SKILL_PERCEPTION: case SKILL_DIPLOMACY:
          case SKILL_TRAP_DISARM:
            price = 500;
            break;
          default: // Weapons, Shield, Light, Dark, Merchant, Learning.
            price = 2000;
            break;
        }
    } else {
        // Master: a per-skill price and prerequisite (switch @0x497198). The MM6 class bytes in
        // the promotion gates: 1/2 Cavalier/Champion, 5 High Priest, 8 Arch Mage, 10/11
        // Crusader/Hero, 13/14 Battle Mage/Warrior Mage; the award bits are those promotions'
        // awards.txt rows.
        canLearn = true;
        price = 5000;
        switch (skill) {
          case SKILL_STAFF:
            canLearn = level >= 8;
            break;
          case SKILL_SWORD:
            price = 0;
            canLearn = level >= 8 && (isMm6Class(1) || isMm6Class(2) || hasAward(17) || hasAward(19));
            break;
          case SKILL_DAGGER:
            canLearn = student->GetActualSpeed() >= 40 && level >= 8;
            break;
          case SKILL_AXE:
            price = 0; // Unconditional (0x496e0b).
            break;
          case SKILL_SPEAR:
            canLearn = level >= 8 && (isMm6Class(1) || isMm6Class(2) || hasAward(17) || hasAward(19));
            break;
          case SKILL_BOW:
            price = 0;
            canLearn = level >= 8 && (isMm6Class(13) || isMm6Class(14) || hasAward(29) || hasAward(31));
            break;
          case SKILL_MACE:
            canLearn = level >= 8 && student->GetActualMight() >= 40;
            break;
          case SKILL_BLASTER:
            canLearn = carriesBlaster();
            break;
          case SKILL_SHIELD:
            canLearn = level >= 10;
            break;
          case SKILL_LEATHER:
            price = 3000;
            canLearn = level >= 10;
            break;
          case SKILL_CHAIN:
            price = 0;
            canLearn = level >= 10 && (isMm6Class(10) || isMm6Class(11) || hasAward(9) || hasAward(11));
            break;
          case SKILL_PLATE:
            price = 0;
            canLearn = isMm6Class(11) || hasAward(11);
            break;
          case SKILL_FIRE: case SKILL_WATER: case SKILL_EARTH: case SKILL_MIND: case SKILL_BODY:
            price = 4000;
            canLearn = level >= 12;
            break;
          case SKILL_AIR:
            price = 4000;
            canLearn = isMm6Class(8) || hasAward(15);
            break;
          case SKILL_SPIRIT:
            price = 0;
            canLearn = isMm6Class(5) || hasAward(23);
            break;
          case SKILL_LIGHT:
            // MM6 reputation is positive = good: Saintly at +1000 (titles @0x489c60). The EXE
            // gates on the hireling-adjusted display value (getter 0x47D600 @0x496faf).
            price = 0;
            canLearn = pParty->GetPartyReputation() >= 1000;
            break;
          case SKILL_DARK:
            price = 0;
            canLearn = pParty->GetPartyReputation() <= -1000;
            break;
          case SKILL_ITEM_ID:
            price = 2500;
            canLearn = level >= 7 && student->GetActualIntelligence() >= 30;
            break;
          case SKILL_MERCHANT:
            price = 4000;
            canLearn = level >= 7 && student->GetActualPersonality() >= 30;
            break;
          case SKILL_REPAIR:
          case SKILL_TRAP_DISARM:
            price = 2500;
            canLearn = level >= 7 && student->GetActualAccuracy() >= 30;
            break;
          case SKILL_BODYBUILDING:
            price = 2500;
            canLearn = level >= 7 && student->GetActualEndurance() >= 30;
            break;
          case SKILL_MEDITATION:
            price = 2500;
            canLearn = level >= 7 && student->GetActualPersonality() >= 30;
            break;
          case SKILL_PERCEPTION:
            price = 2500;
            canLearn = level >= 7 && student->GetActualLuck() >= 30;
            break;
          case SKILL_DIPLOMACY:
            price = 2500;
            canLearn = pParty->getPartyFame() >= 200;
            break;
          case SKILL_LEARNING:
            canLearn = student->GetActualIntelligence() >= 30 && level >= 7;
            break;
          default:
            canLearn = false; // Thievery has no master (0x4970f9).
            break;
        }
    }
    gold_transaction_amount = price;

    // You don't meet the requirements, and cannot be taught until you do.
    if (!canLearn)
        return pNPCTopics[260].pText; // npctext row 261.

    // You don't have enough gold!
    if (gold_transaction_amount > pParty->GetGold())
        return pNPCTopics[259].pText; // npctext row 260.

    membershipOrTrainingApproved = true;
    return localization->str(LSTR_LEARN); // The offer prose already names the price.
}

/**
 * @offset 0x4B254D
 */
std::string masteryTeacherOptionString() {
    if (engine->gameVersion() == GAME_VERSION_MM6)
        return mm6MasteryTeacherOptionString();

    int teacherLevel = (topicEventId - 200) % 3;
    Skill skillBeingTaught = static_cast<Skill>((topicEventId - 200) / 3);
    Character *activePlayer = &pParty->activeCharacter();
    Class pClassType = activePlayer->classType;
    Mastery currClassMaxMastery = skillMaxMasteryPerClass[pClassType][skillBeingTaught];
    Mastery masteryLevelBeingTaught = static_cast<Mastery>(teacherLevel + 2);

    membershipOrTrainingApproved = false;

    if (currClassMaxMastery < masteryLevelBeingTaught) {
        if (skillMaxMasteryPerClass[getTier2Class(pClassType)][skillBeingTaught] >= masteryLevelBeingTaught) {
            return localization->format(LSTR_YOU_HAVE_TO_BE_PROMOTED_TO_S_TO_LEARN, localization->className(getTier2Class(pClassType)));
        } else if (skillMaxMasteryPerClass[getTier3LightClass(pClassType)][skillBeingTaught] >= masteryLevelBeingTaught &&
                skillMaxMasteryPerClass[getTier3DarkClass(pClassType)][skillBeingTaught] >= masteryLevelBeingTaught) {
            return localization->format(LSTR_YOU_HAVE_TO_BE_PROMOTED_TO_S_OR_S_TO,
                    localization->className(getTier3LightClass(pClassType)),
                    localization->className(getTier3DarkClass(pClassType)));
        } else if (skillMaxMasteryPerClass[getTier3LightClass(pClassType)][skillBeingTaught] >= masteryLevelBeingTaught) {
            return localization->format(LSTR_YOU_HAVE_TO_BE_PROMOTED_TO_S_TO_LEARN, localization->className(getTier3LightClass(pClassType)));
        } else if (skillMaxMasteryPerClass[getTier3DarkClass(pClassType)][skillBeingTaught] >= masteryLevelBeingTaught) {
            return localization->format(LSTR_YOU_HAVE_TO_BE_PROMOTED_TO_S_TO_LEARN, localization->className(getTier3DarkClass(pClassType)));
        } else {
            return localization->format(LSTR_THIS_SKILL_LEVEL_CAN_NOT_BE_LEARNED_BY, localization->className(pClassType));
        }
    }

    // Not in your condition!
    if (!activePlayer->CanAct()) {
        return std::string(pNPCTopics[122].pText);
    }

    // You must know the skill before you can become an expert in it!
    int skillLevel = activePlayer->getSkillValue(skillBeingTaught).level();
    if (!skillLevel) {
        return std::string(pNPCTopics[131].pText);
    }

    // You are already have this mastery in this skill.
    Mastery skillMastery = activePlayer->getSkillValue(skillBeingTaught).mastery();
    if (std::to_underlying(skillMastery) > teacherLevel + 1) {
        return std::string(pNPCTopics[teacherLevel + 128].pText);
    }

    bool canLearn = true;

    if (masteryLevelBeingTaught == MASTERY_EXPERT) {
        canLearn = skillLevel >= 4;
        gold_transaction_amount = expertSkillMasteryCost[skillBeingTaught];
    }

    if (masteryLevelBeingTaught == MASTERY_MASTER) {
        switch (skillBeingTaught) {
          case SKILL_LIGHT:
            canLearn = pParty->_questBits[QBIT_114];
            break;
          case SKILL_DARK:
            canLearn = pParty->_questBits[QBIT_110];
            break;
          case SKILL_MERCHANT:
            canLearn = activePlayer->GetBasePersonality() >= 50;
            break;
          case SKILL_BODYBUILDING:
            canLearn = activePlayer->GetBaseEndurance() >= 50;
            break;
          case SKILL_LEARNING:
            canLearn = activePlayer->GetBaseIntelligence() >= 50;
            break;
          default:
            break;
        }
        canLearn = canLearn && (skillLevel >= 7) && (skillMastery == MASTERY_EXPERT);
        gold_transaction_amount = masterSkillMasteryCost[skillBeingTaught];
    }

    if (masteryLevelBeingTaught == MASTERY_GRANDMASTER) {
        switch (skillBeingTaught) {
          case SKILL_LIGHT:
            canLearn = activePlayer->isClass(CLASS_ARCHAMGE) || activePlayer->isClass(CLASS_PRIEST_OF_SUN);
            break;
          case SKILL_DARK:
            canLearn = activePlayer->isClass(CLASS_LICH) || activePlayer->isClass(CLASS_PRIEST_OF_MOON);
            break;
          case SKILL_DODGE:
            canLearn = activePlayer->pActiveSkills[SKILL_UNARMED].level() >= 10;
            break;
          case SKILL_UNARMED:
            canLearn = activePlayer->pActiveSkills[SKILL_DODGE].level() >= 10;
            break;
          default:
            break;
        }
        canLearn = canLearn && (skillLevel >= 10) && (skillMastery == MASTERY_MASTER);
        gold_transaction_amount = grandmasterSkillMasteryCost[skillBeingTaught];
    }

    // You don't meet the requirements, and cannot be taught until you do.
    if (!canLearn) {
        return std::string(pNPCTopics[127].pText);
    }

    // You don't have enough gold!
    if (gold_transaction_amount > pParty->GetGold()) {
        return std::string(pNPCTopics[124].pText);
    }

    membershipOrTrainingApproved = true;

    return localization->format(LSTR_BECOME_S_IN_S_FOR_LU_GOLD, localization->masteryNameLong(masteryLevelBeingTaught),
                                      localization->skillName(skillBeingTaught), gold_transaction_amount);
}

std::string npcDialogueOptionString(DialogueId topic, NPCData *npcData) {
    switch (topic) {
      case DIALOGUE_SCRIPTED_LINE_1:
        return pNPCTopics[npcData->dialogue_1_evt_id].pTopic;
      case DIALOGUE_SCRIPTED_LINE_2:
        return pNPCTopics[npcData->dialogue_2_evt_id].pTopic;
      case DIALOGUE_SCRIPTED_LINE_3:
        return pNPCTopics[npcData->dialogue_3_evt_id].pTopic;
      case DIALOGUE_SCRIPTED_LINE_4:
        return pNPCTopics[npcData->dialogue_4_evt_id].pTopic;
      case DIALOGUE_SCRIPTED_LINE_5:
        return pNPCTopics[npcData->dialogue_5_evt_id].pTopic;
      case DIALOGUE_SCRIPTED_LINE_6:
        return pNPCTopics[npcData->dialogue_6_evt_id].pTopic;
      case DIALOGUE_HIRE_FIRE:
        if (npcData->Hired()) {
            return localization->format(LSTR_DISMISS_S, npcData->name);
        } else if (engine->gameVersion() == GAME_VERSION_MM6) {
            return localization->str(LSTR_JOIN); // MM6's one-click hire is labeled "Join" (global.txt 122).
        } else {
            return localization->str(LSTR_HIRE);
        }
      case DIALOGUE_STREET_MM6_PROF_TOPIC:
        // Labeled with the profession's small-talk topic for the current weekday (MM6.EXE 0x4974E7).
        return pNPCStats->mm6ProfText[npcData->profession][pParty->uCurrentDayOfMonth % 7].topic;
      case DIALOGUE_STREET_MM6_NEWS:
        return npcData->mm6News.topic; // The npcnews.txt topic column, e.g. "Goblinwatch".
      case DIALOGUE_STREET_MM6_BEG:
        return localization->str(MM6_LSTR_BEG);
      case DIALOGUE_STREET_MM6_THREATEN:
        return localization->str(MM6_LSTR_THREAT);
      case DIALOGUE_STREET_MM6_BRIBE:
        // "Bribe 50 Gold" - the live price, recomputed as bribes accumulate (MM6.EXE 0x43B75B).
        return fmt::format("{} {} {}", localization->str(MM6_LSTR_BRIBE), mm6BribeCost(), localization->str(LSTR_GOLD));
      case DIALOGUE_13_hiring_related:
        if (npcData->Hired()) {
            return localization->format(LSTR_DISMISS_S, npcData->name);
        } else {
            return localization->str(LSTR_JOIN);
        }
      case DIALOGUE_PROFESSION_DETAILS:
        return localization->str(LSTR_MORE_INFORMATION);
      case DIALOGUE_MASTERY_TEACHER_LEARN:
        return masteryTeacherOptionString();
      case DIALOGUE_MAGIC_GUILD_JOIN:
        return joinGuildOptionString();
      case DIALOGUE_ARENA_SELECT_LORD:
        return localization->str(LSTR_ARENA_DIFFICULTY_LORD);
      case DIALOGUE_ARENA_SELECT_KNIGHT:
        return localization->str(LSTR_ARENA_DIFFICULTY_KNIGHT);
      case DIALOGUE_ARENA_SELECT_SQUIRE:
        return localization->str(LSTR_ARENA_DIFFICULTY_SQUIRE);
      case DIALOGUE_ARENA_SELECT_PAGE:
        return localization->str(LSTR_ARENA_DIFFICULTY_PAGE);
      case DIALOGUE_USE_HIRED_NPC_ABILITY:
        return GetProfessionActionText(npcData->profession);
      default:
        return "";
    }
}

// MM6's guild-membership topics (381-397) and skill-teacher topics (200-259) have no global.evt
// script behind them - they are intercepted in handleScriptedNPCTopicSelection - so they are
// always listed instead of being dry-run through the event interpreter. (Some ORDINARY global
// events share ids with the teacher range; the MM6.EXE topic dispatch @0x496b42 shadows them
// unconditionally for NPC topics, so listing and intercepting these ids is faithful.)
static bool isMm6ReservedNPCTopic(unsigned int eventId) {
    return engine->gameVersion() == GAME_VERSION_MM6 &&
           ((eventId >= 381 && eventId <= 397) || (eventId >= 200 && eventId <= 259) ||
            eventId == 41 || eventId == 45 || eventId == 399 || eventId == 400);
}

static void addScriptedNPCDialogueTopics(NPCData *npcData, std::vector<DialogueId> &optionList) {
    // TODO(Nik-RE-dev): place NPC events in array
#define ADD_NPC_SCRIPTED_DIALOGUE(EVENT_ID, MSG_PARAM) \
    if (EVENT_ID) { \
        if (optionList.size() < 4) { \
            if (isMm6ReservedNPCTopic(EVENT_ID)) { \
                optionList.push_back(MSG_PARAM); \
            } else { \
                int res = npcDialogueEventProcessor(EVENT_ID); \
                if (res == 1 || res == 2) { \
                    optionList.push_back(MSG_PARAM); \
                } \
            } \
        } \
    }

    ADD_NPC_SCRIPTED_DIALOGUE(npcData->dialogue_1_evt_id, DIALOGUE_SCRIPTED_LINE_1);
    ADD_NPC_SCRIPTED_DIALOGUE(npcData->dialogue_2_evt_id, DIALOGUE_SCRIPTED_LINE_2);
    ADD_NPC_SCRIPTED_DIALOGUE(npcData->dialogue_3_evt_id, DIALOGUE_SCRIPTED_LINE_3);
    ADD_NPC_SCRIPTED_DIALOGUE(npcData->dialogue_4_evt_id, DIALOGUE_SCRIPTED_LINE_4);
    ADD_NPC_SCRIPTED_DIALOGUE(npcData->dialogue_5_evt_id, DIALOGUE_SCRIPTED_LINE_5);
    ADD_NPC_SCRIPTED_DIALOGUE(npcData->dialogue_6_evt_id, DIALOGUE_SCRIPTED_LINE_6);

#undef ADD_NPC_SCRIPTED_DIALOGUE
}

std::vector<DialogueId> prepareScriptedNPCDialogueTopics(NPCData *npcData) {
    std::vector<DialogueId> optionList;

    if (npcData->canJoin) {
        optionList.push_back(DIALOGUE_13_hiring_related);
    }

    addScriptedNPCDialogueTopics(npcData, optionList);

    return optionList;
}

std::vector<DialogueId> prepareHouseNPCDialogueTopics(NPCData *npcData) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        return prepareScriptedNPCDialogueTopics(npcData);

    // MM6's house occupants lead with the same profession small talk / Join / News trio that street
    // dialogue offers, ahead of their scripted topics. The option factory (MM6.EXE 0x499b3a) creates
    // each one only when the matching npcdata column is non-zero, in this order: profession (@+0x18)
    // -> kind 0xc, join (@+0x1c) -> 0xd, news (@+0x20) -> 0xe, then events A/B/C -> 0x13/0x14/0x15.
    // No MM6 house occupant has more than three of these, so the scripted helper's option cap - an
    // MM7 rule with no counterpart in the MM6 factory - can never bite here.
    std::vector<DialogueId> optionList;

    if (npcData->profession != NoProfession)
        optionList.push_back(DIALOGUE_STREET_MM6_PROF_TOPIC);

    if (npcData->canJoin)
        optionList.push_back(DIALOGUE_13_hiring_related);

    if (npcData->mm6HasNews)
        optionList.push_back(DIALOGUE_STREET_MM6_NEWS);

    addScriptedNPCDialogueTopics(npcData, optionList);

    return optionList;
}

DialogueId handleScriptedNPCTopicSelection(DialogueId topic, NPCData *npcData) {
    int eventId;

    if (topic == DIALOGUE_SCRIPTED_LINE_1) {
        eventId = npcData->dialogue_1_evt_id;
    } else if (topic == DIALOGUE_SCRIPTED_LINE_2) {
        eventId = npcData->dialogue_2_evt_id;
    } else if (topic == DIALOGUE_SCRIPTED_LINE_3) {
        eventId = npcData->dialogue_3_evt_id;
    } else if (topic == DIALOGUE_SCRIPTED_LINE_4) {
        eventId = npcData->dialogue_4_evt_id;
    } else if (topic == DIALOGUE_SCRIPTED_LINE_5) {
        eventId = npcData->dialogue_5_evt_id;
    } else {
        assert(topic == DIALOGUE_SCRIPTED_LINE_6);
        eventId = npcData->dialogue_6_evt_id;
    }


    // The special event ids below (Oracle 139, Arena 399, guild membership 400-410, mastery
    // teachers 200-310) are MM7 global.evt conventions. MM6's own reserved topic ranges differ
    // (MM6.EXE topic dispatch @0x496b42): 381-397 are guild-membership offers and 200-259 are
    // expert/master skill teachers; everything else - e.g. New Sorpigal's candelabra quest,
    // event 296 - is an ordinary global.evt script on the generic interpreter path.
    if (engine->gameVersion() == GAME_VERSION_MM6) {
        if (eventId >= 381 && eventId <= 397) {
            // A guild-membership offer: the same offer/join flow as MM7's 400-410 (which descend
            // from these), with MM6's own texts. The long offer texts are npctext rows 138-154,
            // in join-topic order (row 140 = Buccaneers' Lair, topic 383).
            guildMembershipNPCTopicId = topic;
            current_npc_text = pNPCTopics[137 + eventId - 381].pText;
            topicEventId = eventId;
            return DIALOGUE_MAGIC_GUILD_OFFER;
        }
        if (eventId >= 200 && eventId <= 259) {
            // An expert/master skill teacher: the offer prose is npctext row [topic id]
            // (the Merchant teachers' rows 244/245 even name the decoded prices and ranks).
            current_npc_text = pNPCTopics[eventId - 1].pText;
            topicEventId = eventId;
            return DIALOGUE_MASTERY_TEACHER_OFFER;
        }
        if (eventId == 41) { // The Seer's "Pilgrimage" (MM6.EXE @0x4A2F20).
            mm6SeerPilgrimageDialogue();
            return DIALOGUE_MAIN;
        }
        if (eventId == 45) { // The Seer's "I lost it" (MM6.EXE @0x496570).
            mm6SeerLostItemDialogue();
            return DIALOGUE_MAIN;
        }
        if (eventId == 399) {
            // The town-hall bounty hunt as an NPC topic (Janice/Earnest/Jake carry it; MM6.EXE
            // @0x4A30B0) - the same interaction the town-hall house dialogue offers.
            auto [text, monsterId] = bountyHuntInteraction(npcData->house);
            current_npc_text = bountyHuntReplyText(text, monsterId);
            return DIALOGUE_MAIN;
        }
        if (eventId == 400) { // The Arena Master (MM6.EXE @0x4A3350) - MM7's arena descends from this.
            return arenaMainDialogue();
        }
    }
    if (engine->gameVersion() == GAME_VERSION_MM7) {
        if (eventId == 311) {
            // Original code also listed this event which presumably opened bounty dialogue but MM7
            // use event 311 for some teleport in Bracada
            assert(false);
            return DIALOGUE_MAIN;
        }

        if (eventId == 139) {
            oracleDialogue();
            return DIALOGUE_MAIN;
        }
        if (eventId == 399) {
            return arenaMainDialogue();
        }
        if (eventId >= 400 && eventId <= 410) {
            guildMembershipNPCTopicId = topic;
            current_npc_text = pNPCTopics[eventId - 301].pText;
            topicEventId = eventId;
            return DIALOGUE_MAGIC_GUILD_OFFER;
        }
        if (eventId >= 200 && eventId <= 310) {
            current_npc_text = pNPCTopics[eventId + 168].pText;
            topicEventId = eventId;
            return DIALOGUE_MASTERY_TEACHER_OFFER;
        }
    }

    activeLevelDecoration = (LevelDecoration *)1;
    current_npc_text.clear();
    eventProcessor(eventId, Pid(), 1);
    activeLevelDecoration = nullptr;

    return DIALOGUE_MAIN;
}

std::vector<DialogueId> listNPCDialogueOptions(DialogueId topic) {
    switch (topic) {
      case DIALOGUE_MAGIC_GUILD_OFFER:
        return {DIALOGUE_MAGIC_GUILD_JOIN};
      case DIALOGUE_MASTERY_TEACHER_OFFER:
        return {DIALOGUE_MASTERY_TEACHER_LEARN};
      case DIALOGUE_ARENA_WELCOME:
        return {DIALOGUE_ARENA_SELECT_PAGE, DIALOGUE_ARENA_SELECT_SQUIRE, DIALOGUE_ARENA_SELECT_KNIGHT, DIALOGUE_ARENA_SELECT_LORD};
      default:
        return {};
    }
}


void selectSpecialNPCTopicSelection(DialogueId topic, NPCData* npcData) {
    if (topic == DIALOGUE_MASTERY_TEACHER_LEARN) {
        if (membershipOrTrainingApproved) {
            if (pParty->hasActiveCharacter()) {
                Skill skillBeingTaught;
                Mastery newMastery;
                if (engine->gameVersion() == GAME_VERSION_MM6) {
                    // MM6 teacher topics are 200 + 2 x skill slot (+1 for master); MM7's are
                    // 200 + 3 x skill (+0/1/2 for expert/master/grandmaster).
                    skillBeingTaught = mm6TeacherSkill(topicEventId);
                    newMastery = mm6IsMasterTeacherTopic(topicEventId) ? MASTERY_MASTER : MASTERY_EXPERT;
                } else {
                    skillBeingTaught = static_cast<Skill>((topicEventId - 200) / 3);
                    newMastery = static_cast<Mastery>((topicEventId - 200) % 3 + 2);
                }
                CombinedSkillValue skillValue = CombinedSkillValue::increaseMastery(pParty->activeCharacter().getSkillValue(skillBeingTaught), newMastery);
                pParty->activeCharacter().setSkillValue(skillBeingTaught, skillValue);
                pParty->activeCharacter().playReaction(SPEECH_SKILL_MASTERY_INC);
                pParty->TakeGold(gold_transaction_amount);
                engine->_messageQueue->addMessageCurrentFrame(UIMSG_Escape, 1, 0);
            }
        }
    } else if (topic == DIALOGUE_MAGIC_GUILD_JOIN) {
        if (membershipOrTrainingApproved) {
            // MM6 join topics are 381-397 with award bit 64 + index; MM7's are 400-410/416.
            bool isMm6 = engine->gameVersion() == GAME_VERSION_MM6;
            AwardId guildMembershipAwardBit = isMm6
                ? static_cast<AwardId>(64 + topicEventId - 381)
                : membershipAwardForGuild(static_cast<GuildId>(topicEventId - 400));
            unsigned int topicFirst = isMm6 ? 381 : 400;
            unsigned int topicLast = isMm6 ? 397 : 416;
            pParty->TakeGold(gold_transaction_amount, true);
            for (Character &player : pParty->pCharacters) {
                player.SetVariable(VAR_Award, std::to_underlying(guildMembershipAwardBit));
            }

            switch (guildMembershipNPCTopicId) {
              case DIALOGUE_SCRIPTED_LINE_1:
                if (npcData->dialogue_1_evt_id >= topicFirst && npcData->dialogue_1_evt_id <= topicLast)
                    npcData->dialogue_1_evt_id = 0;
                break;
              case DIALOGUE_SCRIPTED_LINE_2:
                if (npcData->dialogue_2_evt_id >= topicFirst && npcData->dialogue_2_evt_id <= topicLast)
                    npcData->dialogue_2_evt_id = 0;
                break;
              case DIALOGUE_SCRIPTED_LINE_3:
                if (npcData->dialogue_3_evt_id >= topicFirst && npcData->dialogue_3_evt_id <= topicLast)
                    npcData->dialogue_3_evt_id = 0;
                break;
              case DIALOGUE_SCRIPTED_LINE_4:
                if (npcData->dialogue_4_evt_id >= topicFirst && npcData->dialogue_4_evt_id <= topicLast)
                    npcData->dialogue_4_evt_id = 0;
                break;
              case DIALOGUE_SCRIPTED_LINE_5:
                if (npcData->dialogue_5_evt_id >= topicFirst && npcData->dialogue_5_evt_id <= topicLast)
                    npcData->dialogue_5_evt_id = 0;
                break;
              case DIALOGUE_SCRIPTED_LINE_6:
                if (npcData->dialogue_6_evt_id >= topicFirst && npcData->dialogue_6_evt_id <= topicLast)
                    npcData->dialogue_6_evt_id = 0;
                break;
              default:
                break;
            }

            engine->_messageQueue->addMessageCurrentFrame(UIMSG_Escape, 1, 0);
            if (pParty->hasActiveCharacter()) {
                pParty->activeCharacter().playReaction(SPEECH_JOINED_GUILD);
            }
        }
    } else if (topic == DIALOGUE_PROFESSION_DETAILS) {
        dialogue_show_profession_details = ~dialogue_show_profession_details;
    } else if (topic >= DIALOGUE_ARENA_SELECT_PAGE && topic <= DIALOGUE_ARENA_SELECT_LORD) {
        prepareArenaFight(arenaLevelForDialogue(topic));
    } else if (topic == DIALOGUE_USE_HIRED_NPC_ABILITY) {
        int hirelingId;
        for (hirelingId = 0; hirelingId < pParty->pHirelings.size(); hirelingId++) {
            if (ascii::noCaseEquals(pParty->pHirelings[hirelingId].name, npcData->name)) { // TODO(captainurist): #unicode
                break;
            }
        }
        assert(hirelingId < pParty->pHirelings.size());
        if (UseNPCSkill(npcData->profession, hirelingId) == 0) {
            if (npcData->profession != GateMaster) {
                npcData->hasUsedAbility = 1;
            }
            engine->_messageQueue->addMessageCurrentFrame(UIMSG_Escape, 1, 0);
        } else {
            engine->_statusBar->setEvent(LSTR_YOUR_PACKS_ARE_ALREADY_FULL);
        }
    } else if (topic == DIALOGUE_HIRE_FIRE) {
        if (npcData->Hired()) {
            if (pNPCStats->uNumNewNPCs > 0) {
                for (int i = 0; i < pNPCStats->uNumNewNPCs; ++i) {
                    if (pNPCStats->pNPCData[i].Hired() && npcData->name == pNPCStats->pNPCData[i].name) {
                        pNPCStats->pNPCData[i].flags &= ~NPC_HIRED;
                    }
                }
            }
            if (ascii::noCaseEquals(pParty->pHirelings[0].name, npcData->name)) { // TODO(captainurist): #unicode
                pParty->pHirelings[0] = NPCData();
            } else if (ascii::noCaseEquals(pParty->pHirelings[1].name, npcData->name)) { // TODO(captainurist): #unicode
                pParty->pHirelings[1] = NPCData();
            }
            pParty->hirelingScrollPosition = 0;
            pParty->CountHirelings();
            engine->_messageQueue->addMessageCurrentFrame(UIMSG_Escape, 1, 0);
            return;
        }
        if (!pParty->pHirelings[0].name.empty() && !pParty->pHirelings[1].name.empty()) {
            engine->_statusBar->setEvent(LSTR_I_CANNOT_JOIN_YOU_YOURE_PARTY_IS_FULL);
        } else {
            if (npcData->profession != Burglar) {
                // burglars have no hiring price
                if (pParty->GetGold() < pNPCStats->pProfessions[npcData->profession].uHirePrice) {
                    engine->_statusBar->setEvent(LSTR_YOU_DONT_HAVE_ENOUGH_GOLD);
                    dialogue_show_profession_details = false;
                    //uDialogueType = DIALOGUE_13_hiring_related;
                    if (pParty->hasActiveCharacter()) {
                        pParty->activeCharacter().playReaction(SPEECH_NOT_ENOUGH_GOLD);
                    }
                    return;
                }
                pParty->TakeGold(pNPCStats->pProfessions[npcData->profession].uHirePrice);
            }
            npcData->flags |= NPC_HIRED;
            if (!pParty->pHirelings[0].name.empty()) {
                pParty->pHirelings[1] = *npcData;
                pParty->pHireling2Name = npcData->name;
            } else {
                pParty->pHirelings[0] = *npcData;
                pParty->pHireling1Name = npcData->name;
            }
            pParty->hirelingScrollPosition = 0;
            pParty->CountHirelings();
            engine->_messageQueue->addMessageCurrentFrame(UIMSG_Escape, 1, 0);
            if (pParty->hasActiveCharacter()) {
                pParty->activeCharacter().playReaction(SPEECH_HIRE_NPC);
            }
        }
    }
}

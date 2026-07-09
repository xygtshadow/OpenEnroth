#include "MagicGuild.h"

#include <array>
#include <cassert>
#include <string>
#include <vector>

#include "Engine/Engine.h"
#include "Engine/EngineIocContainer.h"
#include "Engine/Tables/HouseTable.h"
#include "Engine/Graphics/Renderer/Renderer.h"
#include "Engine/Graphics/Image.h"
#include "Engine/Localization.h"
#include "Engine/Objects/Item.h"
#include "Engine/Tables/ItemTable.h"
#include "Engine/Tables/MerchantTable.h"
#include "Engine/Party.h"
#include "Engine/PriceCalculator.h"
#include "Engine/AssetsManager.h"
#include "Engine/Data/HouseEnumFunctions.h"
#include "Engine/Objects/CombinedSkillValue.h"
#include "Engine/Tables/NPCTable.h"

#include "GUI/GUIWindow.h"
#include "GUI/GUIButton.h"
#include "GUI/GUIMessageQueue.h"
#include "GUI/UI/UIStatusBar.h"
#include "GUI/UI/Houses/MercenaryGuild.h"
#include "GUI/UI/Houses/Shops.h"

#include "Media/Audio/AudioPlayer.h"

#include "Io/Mouse.h"

#include "Engine/Random/Random.h"

static constexpr IndexedArray<MagicSchool, HOUSE_TYPE_FIRE_GUILD, HOUSE_TYPE_DARK_GUILD> guildSpellsSchool = {
    {HOUSE_TYPE_FIRE_GUILD,   MAGIC_SCHOOL_FIRE},
    {HOUSE_TYPE_AIR_GUILD,    MAGIC_SCHOOL_AIR},
    {HOUSE_TYPE_WATER_GUILD,  MAGIC_SCHOOL_WATER},
    {HOUSE_TYPE_EARTH_GUILD,  MAGIC_SCHOOL_EARTH},
    {HOUSE_TYPE_SPIRIT_GUILD, MAGIC_SCHOOL_SPIRIT},
    {HOUSE_TYPE_MIND_GUILD,   MAGIC_SCHOOL_MIND},
    {HOUSE_TYPE_BODY_GUILD,   MAGIC_SCHOOL_BODY},
    {HOUSE_TYPE_LIGHT_GUILD,  MAGIC_SCHOOL_LIGHT},
    {HOUSE_TYPE_DARK_GUILD,   MAGIC_SCHOOL_DARK}
};

static constexpr IndexedArray<DialogueId, HOUSE_TYPE_FIRE_GUILD, HOUSE_TYPE_DARK_GUILD> learnableMagicSkillDialogue = {
    {HOUSE_TYPE_FIRE_GUILD,   DIALOGUE_LEARN_FIRE},
    {HOUSE_TYPE_AIR_GUILD,    DIALOGUE_LEARN_AIR},
    {HOUSE_TYPE_WATER_GUILD,  DIALOGUE_LEARN_WATER},
    {HOUSE_TYPE_EARTH_GUILD,  DIALOGUE_LEARN_EARTH},
    {HOUSE_TYPE_SPIRIT_GUILD, DIALOGUE_LEARN_SPIRIT},
    {HOUSE_TYPE_MIND_GUILD,   DIALOGUE_LEARN_MIND},
    {HOUSE_TYPE_BODY_GUILD,   DIALOGUE_LEARN_BODY},
    {HOUSE_TYPE_LIGHT_GUILD,  DIALOGUE_LEARN_LIGHT},
    {HOUSE_TYPE_DARK_GUILD,   DIALOGUE_LEARN_DARK}
};

static constexpr IndexedArray<DialogueId, HOUSE_TYPE_FIRE_GUILD, HOUSE_TYPE_DARK_GUILD> learnableAdditionalSkillDialogue = {
    {HOUSE_TYPE_FIRE_GUILD,   DIALOGUE_LEARN_LEARNING},
    {HOUSE_TYPE_AIR_GUILD,    DIALOGUE_LEARN_LEARNING},
    {HOUSE_TYPE_WATER_GUILD,  DIALOGUE_LEARN_LEARNING},
    {HOUSE_TYPE_EARTH_GUILD,  DIALOGUE_LEARN_LEARNING},
    {HOUSE_TYPE_SPIRIT_GUILD, DIALOGUE_LEARN_MEDITATION},
    {HOUSE_TYPE_MIND_GUILD,   DIALOGUE_LEARN_MEDITATION},
    {HOUSE_TYPE_BODY_GUILD,   DIALOGUE_LEARN_MEDITATION},
    {HOUSE_TYPE_LIGHT_GUILD,  DIALOGUE_NULL},
    {HOUSE_TYPE_DARK_GUILD,   DIALOGUE_NULL}
};

// The MM6 magic-guild house model, from MM6.EXE (main-dialog handler @0x49bf3d, option factory
// cases @0x498a15..0x498ec4, shelf generator @0x4a4320 with the per-house spell ranges @0x4C48B0,
// learn price @0x49b854, membership table @0x4C3CB8):
//
// - Houses 119-140 come in Initiate/Adept pairs, one organization per pair, gated by a membership
//   award bit on the ACTIVE character (joining happens at recruiter NPC topics, see NPCTopics.cpp).
//   Non-members are turned away with npctext row 172 and zero options.
// - Members are offered Buy Spells plus the guild's skills: the school skill + Learning for the
//   fire/air/water/earth guilds, + Meditation for spirit/mind/body, the school skill alone for
//   light/dark; the MM6-only Element guild teaches all four elemental schools and the Self guild
//   all three self schools. Learning costs trunc(500 * 2dEvents multiplier), merchant-discounted
//   with a floor of a third, and is filtered by the class-can-learn table @0x4C2694.
// - Buy Spells restocks 12 identified spellbooks once the 2dEvents interval elapses:
//   item = 300 + school * 11 + rand % N, N below (the 2dEvents "Spells = 1-N" annotations agree);
//   the combined guilds roll the school per slot.
struct Mm6MagicGuild {
    AwardId membershipAward; // MM6 award bit, verbatim ("Joined the ..." in MM6's awards.txt).
    std::array<MagicSchool, 4> schools;
    int schoolCount;
    bool teachesLearning;
    bool teachesMeditation;
    int initiateSpellRange; // Books roll over the schools' first N spells at the Initiate house...
    int adeptSpellRange;    // ...and the first N at the Adept house.
};

static const Mm6MagicGuild *mm6MagicGuild(HouseId houseId) {
    static const std::array<Mm6MagicGuild, 11> guilds = {{
        {static_cast<AwardId>(74), {MAGIC_SCHOOL_FIRE},   1, true,  false, 7, 11}, // 119/120 Guild of Fire.
        {static_cast<AwardId>(72), {MAGIC_SCHOOL_AIR},    1, true,  false, 7, 11}, // 121/122 Guild of Air.
        {static_cast<AwardId>(75), {MAGIC_SCHOOL_WATER},  1, true,  false, 7, 11}, // 123/124 Guild of Water.
        {static_cast<AwardId>(73), {MAGIC_SCHOOL_EARTH},  1, true,  false, 7, 11}, // 125/126 Guild of Earth.
        {static_cast<AwardId>(78), {MAGIC_SCHOOL_SPIRIT}, 1, false, true,  7, 11}, // 127/128 Guild of Spirit.
        {static_cast<AwardId>(77), {MAGIC_SCHOOL_MIND},   1, false, true,  7, 11}, // 129/130 Guild of Mind.
        {static_cast<AwardId>(76), {MAGIC_SCHOOL_BODY},   1, false, true,  7, 11}, // 131/132 Guild of Body.
        {static_cast<AwardId>(79), {MAGIC_SCHOOL_LIGHT},  1, false, false, 6, 10}, // 133/134 Guild of Light.
        {static_cast<AwardId>(80), {MAGIC_SCHOOL_DARK},   1, false, false, 6, 10}, // 135/136 Guild of Dark.
        {static_cast<AwardId>(64), {MAGIC_SCHOOL_FIRE, MAGIC_SCHOOL_AIR, MAGIC_SCHOOL_WATER, MAGIC_SCHOOL_EARTH},
                                                          4, false, false, 4, 8},  // 137/138 Guild of the Elements.
        {static_cast<AwardId>(65), {MAGIC_SCHOOL_SPIRIT, MAGIC_SCHOOL_MIND, MAGIC_SCHOOL_BODY},
                                                          3, false, false, 4, 8},  // 139/140 Guild of the Self.
    }};
    int index = std::to_underlying(houseId) - 119;
    if (index < 0 || index >= 22)
        return nullptr;
    return &guilds[index / 2];
}

bool mm6IsMagicGuildHouse(HouseId houseId) {
    return mm6MagicGuild(houseId) != nullptr;
}

AwardId mm6MagicGuildMembershipAward(HouseId houseId) {
    assert(mm6IsMagicGuildHouse(houseId));
    return mm6MagicGuild(houseId)->membershipAward;
}

// MM6 skill slots 12-20 are the nine school skills in school order.
static Skill skillForMagicSchool(MagicSchool school) {
    return static_cast<Skill>(std::to_underlying(SKILL_FIRE) + std::to_underlying(school));
}

HouseId GUIWindow_MagicGuild::guildStorageId() const {
    if (engine->gameVersion() == GAME_VERSION_MM6)
        return static_cast<HouseId>(std::to_underlying(houseId()) + 20);
    return houseId();
}

static constexpr IndexedArray<Mastery, HOUSE_FIRST_MAGIC_GUILD, HOUSE_LAST_MAGIC_GUILD> guildSpellsMastery = {
    {HOUSE_FIRE_GUILD_EMERALD_ISLAND,   MASTERY_NOVICE},
    {HOUSE_FIRE_GUILD_HARMONDALE,       MASTERY_EXPERT},
    {HOUSE_FIRE_GUILD_TULAREAN_FOREST,  MASTERY_MASTER},
    {HOUSE_FIRE_GUILD_MOUNT_NIGHON,     MASTERY_GRANDMASTER},
    {HOUSE_AIR_GUILD_EMERALD_ISLAND,    MASTERY_NOVICE},
    {HOUSE_AIR_GUILD_HARMONDALE,        MASTERY_EXPERT},
    {HOUSE_AIR_GUILD_TULAREAN_FOREST,   MASTERY_MASTER},
    {HOUSE_AIR_GUILD_CELESTE,           MASTERY_GRANDMASTER},
    {HOUSE_WATER_GUILD_HARMONDALE,      MASTERY_NOVICE},
    {HOUSE_WATER_GUILD_TULAREAN_FOREST, MASTERY_EXPERT},
    {HOUSE_WATER_GUILD_BRACADA_DESERT,  MASTERY_MASTER},
    {HOUSE_WATER_GUILD_EVENMORN_ISLAND, MASTERY_GRANDMASTER},
    {HOUSE_EARTH_GUILD_HARMONDALE,      MASTERY_NOVICE},
    {HOUSE_EARTH_GUILD_TULAREAN_FOREST, MASTERY_EXPERT},
    {HOUSE_EARTH_GUILD_STONE_CITY,      MASTERY_MASTER},
    {HOUSE_EARTH_GUILD_PIT,             MASTERY_GRANDMASTER},
    {HOUSE_SPIRIT_GUILD_EMERALD_ISLAND, MASTERY_NOVICE},
    {HOUSE_SPIRIT_GUILD_HARMONDALE,     MASTERY_EXPERT},
    {HOUSE_SPIRIT_GUILD_DEYJA,          MASTERY_MASTER},
    {HOUSE_SPIRIT_GUILD_ERATHIA,        MASTERY_GRANDMASTER},
    {HOUSE_MIND_GUILD_HARMONDALE,       MASTERY_NOVICE},
    {HOUSE_MIND_GUILD_ERATHIA,          MASTERY_EXPERT},
    {HOUSE_MIND_GUILD_TATALIA,          MASTERY_MASTER},
    {HOUSE_MIND_GUILD_AVLEE,            MASTERY_GRANDMASTER},
    {HOUSE_BODY_GUILD_EMERALD_ISLAND,   MASTERY_NOVICE},
    {HOUSE_BODY_GUILD_HARMONDALE,       MASTERY_EXPERT},
    {HOUSE_BODY_GUILD_ERATHIA,          MASTERY_MASTER},
    {HOUSE_BODY_GUILD_AVLEE,            MASTERY_GRANDMASTER},
    {HOUSE_LIGHT_GUILD_BRACADA_DESERT,  MASTERY_EXPERT},
    {HOUSE_LIGHT_GUILD_CELESTE,         MASTERY_GRANDMASTER},
    {HOUSE_DARK_GUILD_DEYJA,            MASTERY_EXPERT},
    {HOUSE_DARK_GUILD_PIT,              MASTERY_GRANDMASTER}
};

void GUIWindow_MagicGuild::mm6MainDialogue() {
    Recti working_window = this->frameRect;
    working_window.x = SIDE_TEXT_BOX_POS_X;
    working_window.w = SIDE_TEXT_BOX_WIDTH;

    const Mm6MagicGuild *guild = mm6MagicGuild(houseId());
    if (!guild)
        return;

    if (!pParty->hasActiveCharacter()) // avoid nzi
        pParty->setActiveToFirstCanAct();

    if (!pParty->activeCharacter()._achievedAwardsBits[guild->membershipAward]) {
        // Non-members are turned away with npctext row 172, "You must be a member of this guild to
        // study here" (MM6.EXE 0x49c3b3), and the option count is zeroed.
        int textHeight = assets->pFontArrus->CalcTextHeight(pNPCTopics[171].pText, working_window.w, 0);
        DrawTitleText(assets->pFontArrus.get(), 0, (SIDE_TEXT_BOX_BODY_TEXT_HEIGHT - textHeight) / 2 + SIDE_TEXT_BOX_BODY_TEXT_OFFSET,
                      colorTable.PaleCanary, pNPCTopics[171].pText, 3, working_window);
        pDialogueWindow->pNumPresenceButton = 0;
        return;
    }

    if (!checkIfPlayerCanInteract())
        return;

    Character &player = pParty->activeCharacter();
    std::vector<std::string> optionsText;
    bool haveLearnableSkills = false;
    int buttonsLimit = pDialogueWindow->pStartingPosActiveItem + pDialogueWindow->pNumPresenceButton;
    for (int i = pDialogueWindow->pStartingPosActiveItem; i < buttonsLimit; ++i) {
        DialogueId option = static_cast<DialogueId>(pDialogueWindow->GetControl(i)->msg_param);
        if (option == DIALOGUE_GUILD_BUY_BOOKS) {
            optionsText.push_back(localization->str(LSTR_BUY_SPELLS));
        } else {
            Skill skill = GetLearningDialogueSkill(option);
            if (mm6ClassCanLearn(player.classType, skill) && !player.pActiveSkills[skill]) {
                optionsText.push_back(localization->skillName(skill));
                haveLearnableSkills = true;
            } else {
                optionsText.push_back("");
            }
        }
    }

    if (haveLearnableSkills) {
        int price = mm6SkillLearnPrice(&player, houseId(), 500);
        std::string priceLabel = localization->format(LSTR_SKILL_COST_LU, price);
        DrawTitleText(assets->pFontArrus.get(), 0, 146, colorTable.White, priceLabel, 3, working_window);
    }

    drawOptions(optionsText, colorTable.PaleCanary, 24);
}

void GUIWindow_MagicGuild::mm6LearnSelectedSkill(Skill skill) {
    const Mm6MagicGuild *guild = mm6MagicGuild(houseId());
    Character &player = pParty->activeCharacter();
    if (!guild || !player._achievedAwardsBits[guild->membershipAward])
        return;
    if (!mm6ClassCanLearn(player.classType, skill) || player.pActiveSkills[skill])
        return;

    int price = mm6SkillLearnPrice(&player, houseId(), 500);
    if (pParty->GetGold() < price) {
        engine->_statusBar->setEvent(LSTR_YOU_DONT_HAVE_ENOUGH_GOLD);
        playHouseSound(houseId(), HOUSE_SOUND_GENERAL_NOT_ENOUGH_GOLD);
        return;
    }
    pParty->TakeGold(price);
    _transactionPerformed = true;
    player.pActiveSkills[skill] = CombinedSkillValue::novice();
    player.playReaction(SPEECH_SKILL_LEARNED);
}

void GUIWindow_MagicGuild::mainDialogue() {
    if (engine->gameVersion() == GAME_VERSION_MM6) {
        mm6MainDialogue();
        return;
    }

    Recti working_window = this->frameRect;
    working_window.x = SIDE_TEXT_BOX_POS_X;
    working_window.w = SIDE_TEXT_BOX_WIDTH;

    if (!pParty->activeCharacter()._achievedAwardsBits[membershipAwardForGuild(houseId())]) {
        // you must be a member
        int textHeight = assets->pFontArrus->CalcTextHeight(pNPCTopics[121].pText, working_window.w, 0);
        DrawTitleText(assets->pFontArrus.get(), 0, (212 - textHeight) / 2 + 101, colorTable.PaleCanary, pNPCTopics[121].pText, 3, working_window);
        pDialogueWindow->pNumPresenceButton = 0;
        return;
    }

    if (!checkIfPlayerCanInteract()) {
        return;
    }

    std::vector<std::string> optionsText;

    bool haveLearnableSkills = false;
    int buttonsLimit = pDialogueWindow->pStartingPosActiveItem + pDialogueWindow->pNumPresenceButton;
    for (int i = pDialogueWindow->pStartingPosActiveItem; i < buttonsLimit; ++i) {
        if (pDialogueWindow->GetControl(i)->msg_param == std::to_underlying(DIALOGUE_GUILD_BUY_BOOKS)) {
            optionsText.push_back(localization->str(LSTR_BUY_SPELLS));
        } else {
            Skill skill = GetLearningDialogueSkill((DialogueId)pDialogueWindow->GetControl(i)->msg_param);
            if (skillMaxMasteryPerClass[pParty->activeCharacter().classType][skill] != MASTERY_NONE &&
                !pParty->activeCharacter().pActiveSkills[skill]) {
                optionsText.push_back(localization->skillName(skill));
                haveLearnableSkills = true;
            } else {
                optionsText.push_back("");
            }
        }
    }

    int pPrice = PriceCalculator::skillLearningCostForPlayer(&pParty->activeCharacter(), houseTable[houseId()]);

    if (haveLearnableSkills) {
        std::string skill_price_label = localization->format(LSTR_SKILL_COST_LU, pPrice);
        DrawTitleText(assets->pFontArrus.get(), 0, 146, colorTable.White, skill_price_label, 3, working_window);
    }

    drawOptions(optionsText, colorTable.PaleCanary, 24);
}

void GUIWindow_MagicGuild::buyBooksDialogue() {
    // TODO(pskelton): Extract common item picking code
    Recti working_window = this->frameRect;
    working_window.x = SIDE_TEXT_BOX_POS_X;
    working_window.w = SIDE_TEXT_BOX_WIDTH;

    // Every guild type stocks 12 books (the itemAmountInShop value for all of MM7's guild types;
    // MM6's Element/Self guild types sit outside that array's key range).
    constexpr int guildShelfSlots = 12;
    HouseId storageId = guildStorageId();

    render->DrawQuad2D(shop_ui_background, {8, 8});
    int itemxind = 0;

    for (int pX = 32; pX < 452; pX += 70) {  // top row
        if (pParty->spellBooksInGuilds[storageId][itemxind].itemId != ITEM_NULL) {
            render->DrawQuad2D(shop_ui_items_in_store[itemxind], {pX, 90});
        }
        if (pParty->spellBooksInGuilds[storageId][itemxind + 6].itemId != ITEM_NULL) {
            render->DrawQuad2D(shop_ui_items_in_store[itemxind + 6], {pX, 250});
        }

        ++itemxind;
    }

    if (checkIfPlayerCanInteract()) {
        int itemcount = 0;
        for (int i = 0; i < guildShelfSlots; ++i) {
            if (pParty->spellBooksInGuilds[storageId][i].itemId != ITEM_NULL)
                ++itemcount;
        }

        engine->_statusBar->drawForced(localization->str(LSTR_SELECT_THE_ITEM_TO_BUY), colorTable.White);

        if (!itemcount) {  // shop empty
            Time nextGenTime = pParty->PartyTimes.guildNextRefreshTime[storageId];
            DrawShops_next_generation_time_string(nextGenTime - pParty->GetPlayingTime(), working_window);
            return;
        }

        Pointi pt = EngineIocContainer::ResolveMouse()->position();
        int testx = (pt.x - 32) / 70;
        if (testx >= 0 && testx < 6) {
            if (pt.y >= 250) {
                testx += 6;
            }

            Item *item = &pParty->spellBooksInGuilds[storageId][testx];

            if (item->itemId != ITEM_NULL) {
                int testpos;
                if (pt.y >= 250) {
                    testpos = 32 + 70 * testx - 420;
                } else {
                    testpos = 32 + 70 * testx;
                }

                if (pt.x >= testpos && pt.x <= testpos + (shop_ui_items_in_store[testx]->width())) {
                    if ((pt.y >= 90 && pt.y <= (90 + (shop_ui_items_in_store[testx]->height()))) || (pt.y >= 250 && pt.y <= (250 + (shop_ui_items_in_store[testx]->height())))) {
                        MerchantPhrase phrase = pParty->activeCharacter().SelectPhrasesTransaction(item, HOUSE_TYPE_MAGIC_SHOP, houseId(), SHOP_SCREEN_BUY);
                        std::string str = BuildDialogueString(pMerchantsBuyPhrases[phrase], pParty->activeCharacterIndex() - 1, houseNpcs[currentHouseNpc].npc, item, houseId(), SHOP_SCREEN_BUY);
                        int textHeight = assets->pFontArrus->CalcTextHeight(str, working_window.w, 0);
                        DrawTitleText(assets->pFontArrus.get(), 0, (SIDE_TEXT_BOX_BODY_TEXT_HEIGHT - textHeight) / 2 + SIDE_TEXT_BOX_BODY_TEXT_OFFSET, colorTable.White, str, 3, working_window);
                        return;
                    }
                }
            }
        }
    }
}

void GUIWindow_MagicGuild::houseDialogueOptionSelected(DialogueId option) {
    if (engine->gameVersion() == GAME_VERSION_MM6 && IsSkillLearningDialogue(option)) {
        mm6LearnSelectedSkill(GetLearningDialogueSkill(option));
        // The EXE stays on the guild menu after a purchase; the learned skill drops out of the
        // draw filter when the option list is rebuilt.
        _currentDialogue = DIALOGUE_MAIN;
        return;
    }

    _currentDialogue = option;
    if (option == DIALOGUE_GUILD_BUY_BOOKS) {
        HouseId storageId = guildStorageId();
        if (pParty->PartyTimes.guildNextRefreshTime[storageId] >= pParty->GetPlayingTime()) {
            for (int i = 0; i < 12; ++i) {
                if (pParty->spellBooksInGuilds[storageId][i].itemId != ITEM_NULL)
                    shop_ui_items_in_store[i] = assets->getImage_ColorKey(pParty->spellBooksInGuilds[storageId][i].GetIconName());
            }
        } else {
            // Restock on demand once the 2dEvents interval has elapsed (MM6.EXE 0x4a4630 does the
            // same on selecting Buy Spells; MM7 inherited the flow).
            Time nextGenTime = pParty->GetPlayingTime() + Duration::fromDays(houseTable[houseId()].generation_interval_days);
            if (engine->gameVersion() == GAME_VERSION_MM6) {
                generateSpellBooksForGuildMm6();
            } else {
                generateSpellBooksForGuild();
            }
            pParty->PartyTimes.guildNextRefreshTime[storageId] = nextGenTime;
        }
    } else if (IsSkillLearningDialogue(option)) {
        learnSelectedSkill(GetLearningDialogueSkill(option));
    }
}

void GUIWindow_MagicGuild::houseSpecificDialogue() {
    switch (_currentDialogue) {
      case DIALOGUE_MAIN:
        mainDialogue();
        break;
      case DIALOGUE_GUILD_BUY_BOOKS:
        buyBooksDialogue();
        break;
      default:
        engine->_messageQueue->addMessageCurrentFrame(UIMSG_Escape, 1, 0);
        break;
    }
}

std::vector<DialogueId> GUIWindow_MagicGuild::listDialogueOptions() {
    if (engine->gameVersion() == GAME_VERSION_MM6) {
        if (_currentDialogue != DIALOGUE_MAIN)
            return {};
        const Mm6MagicGuild *guild = mm6MagicGuild(houseId());
        if (!guild || !pParty->hasActiveCharacter() || !pParty->activeCharacter()._achievedAwardsBits[guild->membershipAward])
            return {}; // Not a member: the guildmaster offers nothing; joining happens at recruiter NPCs.

        // The MM6.EXE option factory (@0x498a15..0x498ec4): Buy Spells, the guild's school
        // skill(s), and Learning (elemental schools) or Meditation (self schools).
        std::vector<DialogueId> options = {DIALOGUE_GUILD_BUY_BOOKS};
        for (int i = 0; i < guild->schoolCount; i++) // The inverse of GetLearningDialogueSkill.
            options.push_back(static_cast<DialogueId>(std::to_underlying(DIALOGUE_LEARN_STAFF) + std::to_underlying(skillForMagicSchool(guild->schools[i]))));
        if (guild->teachesLearning)
            options.push_back(DIALOGUE_LEARN_LEARNING);
        if (guild->teachesMeditation)
            options.push_back(DIALOGUE_LEARN_MEDITATION);
        return options;
    }

    HouseType guildType = buildingType();

    switch (_currentDialogue) {
      case DIALOGUE_MAIN:
        if (learnableAdditionalSkillDialogue[guildType] != DIALOGUE_NULL) {
            return {DIALOGUE_GUILD_BUY_BOOKS, learnableMagicSkillDialogue[guildType], learnableAdditionalSkillDialogue[guildType]};
        } else {
            return {DIALOGUE_GUILD_BUY_BOOKS, learnableMagicSkillDialogue[guildType]};
        }
      default:
        return {};
    }
}

void GUIWindow_MagicGuild::houseScreenClick() {
    if (!checkIfPlayerCanInteract()) {
        pAudioPlayer->playUISound(SOUND_error);
        return;
    }

    Pointi pt = EngineIocContainer::ResolveMouse()->position();

    int testx = (pt.x - 32) / 70;
    if (testx >= 0 && testx < 6) {
        if (pt.y >= 250) {
            testx += 6;
        }

        Item &boughtItem = pParty->spellBooksInGuilds[guildStorageId()][testx];
        if (boughtItem.itemId != ITEM_NULL) {
            int testpos;
            if (pt.y >= 250) {
                testpos = 32 + 70 * testx - 420;
            } else {
                testpos = 32 + 70 * testx;
            }

            if (pt.x >= testpos && pt.x <= testpos + (shop_ui_items_in_store[testx]->width())) {
                if ((pt.y >= 90 && pt.y <= (90 + (shop_ui_items_in_store[testx]->height()))) ||
                    (pt.y >= 250 && pt.y <= (250 + (shop_ui_items_in_store[testx]->height())))) {
                    float fPriceMultiplier = houseTable[houseId()].fPriceMultiplier;
                    int uPriceItemService = PriceCalculator::itemBuyingPriceForPlayer(&pParty->activeCharacter(), boughtItem.GetValue(), fPriceMultiplier);

                    if (pParty->GetGold() < uPriceItemService) {
                        playHouseSound(houseId(), HOUSE_SOUND_GENERAL_NOT_ENOUGH_GOLD);
                        engine->_statusBar->setEvent(LSTR_YOU_DONT_HAVE_ENOUGH_GOLD);
                        return;
                    }

                    std::optional<Pointi> pos = pParty->activeCharacter().inventory.findSpace(boughtItem);
                    if (pos) {
                        boughtItem.SetIdentified();
                        pParty->activeCharacter().inventory.add(*pos, boughtItem);
                        _transactionPerformed = true;
                        pParty->TakeGold(uPriceItemService);
                        boughtItem.Reset();
                        pParty->activeCharacter().playReaction(SPEECH_ITEM_BUY);
                        return;
                    }

                    pParty->activeCharacter().playReaction(SPEECH_NO_ROOM);
                    engine->_statusBar->setEvent(LSTR_PACK_IS_FULL);
                }
            }
        }
    }
}

void GUIWindow_MagicGuild::generateSpellBooksForGuildMm6() {
    const Mm6MagicGuild *guild = mm6MagicGuild(houseId());
    assert(guild);

    // Initiate houses (even offset from 119) stock the schools' first initiateSpellRange spells,
    // Adept houses (odd offset) the first adeptSpellRange (MM6.EXE ranges @0x4C48B0; the guilds'
    // 2dEvents "Spells = 1-N" annotations agree).
    int spellRange = (std::to_underlying(houseId()) - 119) % 2 ? guild->adeptSpellRange : guild->initiateSpellRange;
    HouseId storageId = guildStorageId();
    for (int i = 0; i < 12; ++i) {
        MagicSchool school = guild->schools[guild->schoolCount == 1 ? 0 : grng->random(guild->schoolCount)];
        ItemId itemId = static_cast<ItemId>(300 + std::to_underlying(school) * 11 + grng->random(spellRange));

        Item *itemSpellbook = &pParty->spellBooksInGuilds[storageId][i];
        itemSpellbook->Reset();
        itemSpellbook->itemId = itemId;
        itemSpellbook->SetIdentified();

        shop_ui_items_in_store[i] = assets->getImage_ColorKey(pItemTable->items[itemId].iconName);
    }
}

void GUIWindow_MagicGuild::generateSpellBooksForGuild() {
    HouseType guildType = buildingType();

    // Combined guilds exist only in MM6/MM8 and need to be processed separately
    assert(guildType >= HOUSE_TYPE_FIRE_GUILD && guildType <= HOUSE_TYPE_DARK_GUILD);

    MagicSchool schoolType = guildSpellsSchool[guildType];
    Mastery maxMastery = guildSpellsMastery[houseId()];
    Segment<ItemId> spellbooksForGuild = spellbooksForSchool(schoolType, maxMastery);

    for (int i = 0; i < itemAmountInShop[guildType]; ++i) {
        ItemId pItemNum = grng->randomSample(spellbooksForGuild);

        if (pItemNum == ITEM_SPELLBOOK_DIVINE_INTERVENTION) {
            if (!pParty->_questBits[QBIT_DIVINE_INTERVENTION_RETRIEVED]) {
                pItemNum = ITEM_SPELLBOOK_SUNRAY;
            }
        }

        Item *itemSpellbook = &pParty->spellBooksInGuilds[houseId()][i];
        itemSpellbook->Reset();
        itemSpellbook->itemId = pItemNum;
        itemSpellbook->SetIdentified();

        shop_ui_items_in_store[i] = assets->getImage_ColorKey(pItemTable->items[pItemNum].iconName);
    }
}

#include "GUI/UI/Houses/MercenaryGuild.h"

#include <array>
#include <string>
#include <vector>

#include "GUI/GUIButton.h"
#include "GUI/GUIDialogues.h"
#include "GUI/GUIFont.h"
#include "GUI/GUIMessageQueue.h"
#include "GUI/UI/UIStatusBar.h"

#include "Engine/AssetsManager.h"
#include "Engine/Engine.h"
#include "Engine/Localization.h"
#include "Engine/Party.h"
#include "Engine/PriceCalculator.h"
#include "Engine/Tables/HouseTable.h"

#include "Media/Audio/AudioPlayer.h"

// The MM6 guild-house model, from MM6.EXE (the type-17/18 house handler @0x49c420 and the
// option builder cases @0x498ec4/0x4991e3):
//
// - Every fighter/thief guild house maps to a membership award bit (word pair table @0x4C3CB8;
//   the same organization owns two or three houses). awards.txt rows 64-80 carry the
//   "Joined the ..." strings. Membership is granted through recruiter NPC topics 381-397
//   (see NPCTopics.cpp) and is checked on the ACTIVE character.
// - A member is offered the house's five taught skills, hardcoded per HOUSE ID in the EXE
//   (the two "Blades' End" houses even teach different fifth skills). Offered skills are
//   filtered by the class-can-learn table @0x4C2694 and by not already knowing the skill.
// - Learning costs trunc(base * 2dEvents price multiplier) where base is 100 for "Merc Guild"
//   rows and 250 for "Thieves Guild" rows (@0x49c4cd; the 2dEvents type picks the base),
//   merchant-discounted with a floor of a third of the undiscounted price, and sets the
//   skill to novice level 1 (@0x49c712).
struct Mm6GuildHouse {
    AwardId membershipAward; // MM6 award bit, verbatim ("Joined the ..." in MM6's awards.txt).
    int learnPriceBase;
    std::array<Skill, 5> taughtSkills;
};

static const Mm6GuildHouse *mm6GuildHouse(HouseId houseId) {
    static const Mm6GuildHouse blades141     = {static_cast<AwardId>(69), 100, {{SKILL_SWORD, SKILL_AXE, SKILL_SPEAR, SKILL_STAFF, SKILL_LEATHER}}};
    static const Mm6GuildHouse duelists      = {static_cast<AwardId>(70), 100, {{SKILL_MACE, SKILL_BOW, SKILL_CHAIN, SKILL_SHIELD, SKILL_BODYBUILDING}}};
    static const Mm6GuildHouse berserkers    = {static_cast<AwardId>(71), 100, {{SKILL_CHAIN, SKILL_BOW, SKILL_SHIELD, SKILL_PLATE, SKILL_REPAIR}}};
    static const Mm6GuildHouse blades145     = {static_cast<AwardId>(69), 100, {{SKILL_SWORD, SKILL_AXE, SKILL_SPEAR, SKILL_STAFF, SKILL_REPAIR}}};
    static const Mm6GuildHouse buccaneers    = {static_cast<AwardId>(66), 250, {{SKILL_DAGGER, SKILL_MERCHANT, SKILL_ITEM_ID, SKILL_PERCEPTION, SKILL_TRAP_DISARM}}};
    static const Mm6GuildHouse buccaneers2   = {static_cast<AwardId>(66), 250, {{SKILL_LEATHER, SKILL_DIPLOMACY, SKILL_ITEM_ID, SKILL_PERCEPTION, SKILL_TRAP_DISARM}}};
    static const Mm6GuildHouse protection    = {static_cast<AwardId>(67), 250, {{SKILL_DAGGER, SKILL_MERCHANT, SKILL_ITEM_ID, SKILL_PERCEPTION, SKILL_TRAP_DISARM}}};
    static const Mm6GuildHouse protection2   = {static_cast<AwardId>(67), 250, {{SKILL_LEATHER, SKILL_DIPLOMACY, SKILL_ITEM_ID, SKILL_PERCEPTION, SKILL_TRAP_DISARM}}};
    static const Mm6GuildHouse smugglers     = {static_cast<AwardId>(68), 250, {{SKILL_DAGGER, SKILL_MERCHANT, SKILL_ITEM_ID, SKILL_PERCEPTION, SKILL_TRAP_DISARM}}};
    static const Mm6GuildHouse smugglers2    = {static_cast<AwardId>(68), 250, {{SKILL_LEATHER, SKILL_DIPLOMACY, SKILL_ITEM_ID, SKILL_PERCEPTION, SKILL_TRAP_DISARM}}};

    switch (std::to_underlying(houseId)) {
      case 141: return &blades141;   // Blades' End, New Sorpigal.
      case 142: return &duelists;    // Duelists' Edge, Misty Islands.
      case 143: return &berserkers;  // Berserkers' Fury, White Cap.
      case 144: return &duelists;    // Duelists' Edge, Bootleg Bay.
      case 145: return &blades145;   // Blades' End, Blackshire.
      case 146: return &berserkers;  // Berserkers' Fury, Free Haven.
      case 147: return &buccaneers;  // Buccaneers' Lair, New Sorpigal.
      case 148: return &buccaneers2; // Buccaneers' Lair, Misty Islands.
      case 149: return &protection;  // Protection Services, Blackshire.
      case 150: return &protection2; // Protection Services, White Cap.
      case 151: return &smugglers;   // Smugglers' Guild, Bootleg Bay.
      case 152: return &smugglers2;  // Smugglers' Guild, Mist.
      default:  return nullptr;      // Not a guild: MM6's catch-all "plain house" type.
    }
}

// MM6.EXE class-can-learn table @0x4C2694: 6 base classes x 31 MM6 skill slots. Zero means the
// class can never learn the skill; the meaning of the nonzero grades 1/2/3 is not fully reversed
// (they likely cap the NPC-teacher promotion tier) - here only zero/nonzero matters.
static bool mm6ClassCanLearn(Class classType, Skill skill) {
    static constexpr std::array<std::array<uint8_t, 31>, 6> canLearn = {{
        {{3, 1, 2, 2, 2, 2, 3, 3, 2, 1, 2, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 3, 2, 0, 2, 3, 0, 2, 3}}, // Knight
        {{2, 0, 0, 0, 0, 3, 1, 3, 2, 2, 3, 0, 0, 0, 0, 0, 2, 2, 1, 3, 3, 2, 3, 2, 3, 2, 3, 2, 0, 3, 3}}, // Cleric
        {{2, 0, 1, 0, 0, 3, 0, 3, 0, 2, 0, 0, 1, 2, 2, 2, 0, 0, 0, 3, 3, 2, 3, 2, 3, 2, 3, 2, 0, 3, 3}}, // Sorcerer
        {{3, 1, 2, 3, 2, 3, 2, 3, 2, 2, 2, 3, 0, 0, 0, 0, 1, 3, 3, 0, 0, 3, 3, 3, 3, 3, 2, 2, 0, 2, 3}}, // Paladin
        {{3, 2, 2, 2, 3, 1, 3, 3, 0, 2, 3, 0, 2, 1, 3, 3, 0, 0, 0, 0, 0, 2, 3, 3, 3, 3, 2, 2, 0, 2, 3}}, // Archer
        {{1, 0, 3, 0, 0, 3, 2, 3, 3, 2, 0, 0, 3, 3, 2, 1, 2, 3, 2, 0, 0, 2, 3, 2, 3, 2, 3, 3, 0, 3, 2}}, // Druid
    }};
    // Map the engine's MM7-shaped Class lines (Knight, Thief, Monk, Paladin, Archer, Ranger,
    // Cleric, Druid, Sorcerer; promotion tiers don't change what a class can learn) onto MM6's
    // six base-class rows.
    static constexpr std::array<int, 9> mm6RowForClassLine = {{0, -1, -1, 3, 4, -1, 1, 5, 2}};
    int row = mm6RowForClassLine[std::to_underlying(classType) / 4];
    if (row < 0)
        return false; // A class that doesn't exist in MM6.
    // MM6 skill slots 0-29 coincide with the engine's Skill enum; slot 30 is Learning
    // (MM7 squeezed 6 more skills in before it). Everything else doesn't exist in MM6.
    int slot;
    if (skill == SKILL_LEARNING) {
        slot = 30;
    } else if (std::to_underlying(skill) < 30) {
        slot = std::to_underlying(skill);
    } else {
        return false;
    }
    return canLearn[row][slot] != 0;
}

// MM6.EXE 0x49c4cd: trunc(base * multiplier), merchant-discounted, floored at a third of the
// undiscounted price.
static int mm6SkillLearnPrice(const Character *player, HouseId houseId, int base) {
    int price = static_cast<int>(base * houseTable[houseId].fPriceMultiplier);
    int effectivePrice = PriceCalculator::applyMerchantDiscount(player, price);
    if (effectivePrice < price / 3)
        effectivePrice = price / 3;
    return effectivePrice;
}

std::vector<DialogueId> GUIWindow_MercenaryGuild::listDialogueOptions() {
    if (engine->gameVersion() != GAME_VERSION_MM6 || _currentDialogue != DIALOGUE_MAIN)
        return {};
    const Mm6GuildHouse *guild = mm6GuildHouse(houseId());
    if (!guild)
        return {};
    if (!pParty->hasActiveCharacter() || !pParty->activeCharacter()._achievedAwardsBits[guild->membershipAward])
        return {}; // Not a member: the guildmaster offers nothing; joining happens at recruiter NPCs.

    std::vector<DialogueId> options;
    for (Skill skill : guild->taughtSkills) // The inverse of GetLearningDialogueSkill.
        options.push_back(static_cast<DialogueId>(std::to_underlying(DIALOGUE_LEARN_STAFF) + std::to_underlying(skill)));
    return options;
}

void GUIWindow_MercenaryGuild::mm6LearnSkillsDialogue() {
    if (!checkIfPlayerCanInteract())
        return;

    const Mm6GuildHouse *guild = mm6GuildHouse(houseId());
    Character &player = pParty->activeCharacter();

    bool haveLearnableSkills = false;
    std::vector<std::string> optionsText;
    int buttonsLimit = pDialogueWindow->pStartingPosActiveItem + pDialogueWindow->pNumPresenceButton;
    for (int i = pDialogueWindow->pStartingPosActiveItem; i < buttonsLimit; i++) {
        DialogueId option = static_cast<DialogueId>(pDialogueWindow->GetControl(i)->msg_param);
        if (!IsSkillLearningDialogue(option)) {
            optionsText.push_back("");
            continue;
        }
        Skill skill = GetLearningDialogueSkill(option);
        if (mm6ClassCanLearn(player.classType, skill) && !player.pActiveSkills[skill]) {
            optionsText.push_back(localization->skillName(skill));
            haveLearnableSkills = true;
        } else {
            optionsText.push_back("");
        }
    }

    Recti dialogue = this->frameRect;
    dialogue.x = SIDE_TEXT_BOX_POS_X;
    dialogue.w = SIDE_TEXT_BOX_WIDTH;

    if (!haveLearnableSkills) {
        std::string str = localization->format(LSTR_SEEK_KNOWLEDGE_ELSEWHERE_S_THE_S, player.name, localization->className(player.classType));
        str = str + "\n \n" + localization->str(LSTR_I_CAN_OFFER_YOU_NOTHING_FURTHER);
        int textHeight = assets->pFontArrus->CalcTextHeight(str, dialogue.w, 0);
        DrawTitleText(assets->pFontArrus.get(), 0, (SIDE_TEXT_BOX_BODY_TEXT_HEIGHT - textHeight) / 2 + SIDE_TEXT_BOX_BODY_TEXT_OFFSET, colorTable.PaleCanary, str, 3, dialogue);
    } else {
        int cost = mm6SkillLearnPrice(&player, houseId(), guild->learnPriceBase);
        std::string priceLabel = localization->format(LSTR_SKILL_COST_LU, cost);
        DrawTitleText(assets->pFontArrus.get(), 0, 146, colorTable.White, priceLabel, 3, dialogue);
    }

    drawOptions(optionsText, colorTable.PaleCanary, 18);
}

void GUIWindow_MercenaryGuild::mm6LearnSelectedSkill(Skill skill) {
    const Mm6GuildHouse *guild = mm6GuildHouse(houseId());
    Character &player = pParty->activeCharacter();
    if (!guild || !player._achievedAwardsBits[guild->membershipAward])
        return;
    if (!mm6ClassCanLearn(player.classType, skill) || player.pActiveSkills[skill])
        return;

    int price = mm6SkillLearnPrice(&player, houseId(), guild->learnPriceBase);
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

void GUIWindow_MercenaryGuild::houseDialogueOptionSelected(DialogueId option) {
    if (engine->gameVersion() == GAME_VERSION_MM6 && IsSkillLearningDialogue(option)) {
        mm6LearnSelectedSkill(GetLearningDialogueSkill(option));
        // The EXE stays on the guild menu after a purchase; the option list is rebuilt by
        // selectProprietorDialogueOption and the learned skill drops out of the draw filter.
        _currentDialogue = DIALOGUE_MAIN;
        return;
    }
    _currentDialogue = option;
}

void GUIWindow_MercenaryGuild::houseSpecificDialogue() {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        return; // MM7 data maps every 2dEvents type explicitly; this window is MM6-only.

    // TODO(pskelton): check this behaviour
    if (!pParty->hasActiveCharacter())  // avoid nzi
        pParty->setActiveToFirstCanAct();

    if (_currentDialogue != DIALOGUE_MAIN) {
        engine->_messageQueue->addMessageCurrentFrame(UIMSG_Escape, 1, 0);
        return;
    }

    const Mm6GuildHouse *guild = mm6GuildHouse(houseId());
    if (!guild)
        return; // A plain house: nothing to offer (the proprietor name is already drawn).

    if (!pParty->activeCharacter()._achievedAwardsBits[guild->membershipAward]) {
        // Non-members get no options (MM6.EXE 0x49cc02 zeroes the option count).
        pDialogueWindow->pNumPresenceButton = 0;
        return;
    }

    mm6LearnSkillsDialogue();
}

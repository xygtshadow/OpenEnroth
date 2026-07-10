#include "Tavern.h"

#include <string>
#include <vector>

#include "GUI/UI/UIStatusBar.h"
#include "GUI/GUIFont.h"
#include "GUI/GUIButton.h"
#include "GUI/GUIMessageQueue.h"

#include "Engine/Data/HouseEnumFunctions.h"
#include "Engine/AssetsManager.h"
#include "Engine/Localization.h"
#include "Engine/PriceCalculator.h"
#include "Engine/Graphics/Renderer/Renderer.h"
#include "Engine/Party.h"
#include "Engine/Random/Random.h"
#include "Engine/Tables/NPCTable.h"
#include "Engine/mm7_data.h"
#include "Engine/Engine.h"

#include "Arcomage/Arcomage.h"
#include "Engine/Graphics/Viewport.h"

#include "Media/MediaPlayer.h"

#include "Utility/Segment.h"

// MM6 global.txt row 399 is "Tip Barkeep"; MM7 reuses that row for "Neutral", so there is no
// shared LSTR id for it.
static constexpr LstrId MM6_LSTR_TIP_BARKEEP = static_cast<LstrId>(399);

void GUIWindow_Tavern::mainDialogue() {
    if (!checkIfPlayerCanInteract()) {
        return;
    }

    int pPriceRoom = PriceCalculator::tavernRoomCostForPlayer(&pParty->activeCharacter(), houseTable[houseId()]);
    int pPriceFood = PriceCalculator::tavernFoodCostForPlayer(&pParty->activeCharacter(), houseTable[houseId()]);
    int foodNum = houseTable[houseId()].fPriceMultiplier;

    if (engine->gameVersion() == GAME_VERSION_MM6) {
        // Labels for the flat MM6 menu, parallel to listDialogueOptions().
        std::vector<std::string> optionsText = {localization->format(LSTR_RENT_ROOM_FOR_D_GOLD, pPriceRoom),
                                                localization->format(LSTR_FILL_PACKS_TO_D_DAYS_FOR_D_GOLD, foodNum, pPriceFood),
                                                localization->str(LSTR_HAVE_A_DRINK),
                                                localization->str(MM6_LSTR_TIP_BARKEEP)};
        drawOptions(optionsText, colorTable.PaleCanary);
        return;
    }

    std::vector<std::string> optionsText = {localization->format(LSTR_RENT_ROOM_FOR_D_GOLD, pPriceRoom),
                                            localization->format(LSTR_FILL_PACKS_TO_D_DAYS_FOR_D_GOLD, foodNum, pPriceFood),
                                            localization->str(LSTR_LEARN_SKILLS)};

    if (houseId() != HOUSE_TAVERN_EMERALD_ISLAND) {
        optionsText.push_back(localization->str(LSTR_PLAY_ARCOMAGE));
    }

    drawOptions(optionsText, colorTable.PaleCanary);
}

void GUIWindow_Tavern::arcomageMainDialogue() {
    if (!checkIfPlayerCanInteract()) {
        return;
    }

    std::vector<std::string> optionsText = {localization->str(LSTR_RULES), localization->str(LSTR_VICTORY_CONDITIONS)};
    if (pParty->hasItem(ITEM_QUEST_ARCOMAGE_DECK))
        optionsText.push_back(localization->str(LSTR_PLAY));

    drawOptions(optionsText, colorTable.PaleCanary);
}

void GUIWindow_Tavern::arcomageRulesDialogue() {
    DrawDialoguePanel(pNPCTopics[354].pText);
}

void GUIWindow_Tavern::arcomageVictoryCondDialogue() {
    DrawDialoguePanel(pNPCTopics[arcomageTopicForTavern(houseId())].pText);
}

void GUIWindow_Tavern::arcomageResultDialogue() {
    Recti dialog_window = this->frameRect;
    dialog_window.x = SIDE_TEXT_BOX_POS_X;
    dialog_window.w = SIDE_TEXT_BOX_WIDTH;

    if (!pParty->hasItem(ITEM_QUEST_ARCOMAGE_DECK)) {
        engine->_messageQueue->addMessageCurrentFrame(UIMSG_Escape, 1, 0);
        return;
    }

    if (pArcomageGame->bGameInProgress == 1) {
        return;
    }
    std::string pText;
    if (pArcomageGame->uGameWinner) {
        if (pArcomageGame->uGameWinner == 1)
            pText = localization->str(LSTR_YOU_WON);
        else
            pText = localization->str(LSTR_YOU_LOST);
    } else {
        pText = localization->str(LSTR_A_TIE);
    }
    int vertMargin = (SIDE_TEXT_BOX_BODY_TEXT_HEIGHT - assets->pFontArrus->CalcTextHeight(pText, dialog_window.w, 0)) / 2 + SIDE_TEXT_BOX_BODY_TEXT_OFFSET;
    DrawTitleText(assets->pFontArrus.get(), 0, vertMargin, colorTable.PaleCanary, pText, 3, dialog_window);
}

void GUIWindow_Tavern::restDialogue() {
    int pPriceRoom = PriceCalculator::tavernRoomCostForPlayer(&pParty->activeCharacter(), houseTable[houseId()]);

    if (pParty->GetGold() >= pPriceRoom) {
        pParty->TakeGold(pPriceRoom);
        playHouseSound(houseId(), HOUSE_SOUND_TAVERN_RENT_ROOM);
        _currentDialogue = DIALOGUE_NULL;
        houseDialogPressEscape();
        playHouseGoodbyeSpeech();
        pMediaPlayer->Unload();

        engine->_messageQueue->addMessageCurrentFrame(UIMSG_RentRoom, std::to_underlying(houseId()), 1);
        window_SpeakInHouse = nullptr;
        return;
    }
    engine->_statusBar->setEvent(LSTR_YOU_DONT_HAVE_ENOUGH_GOLD);
    playHouseSound(houseId(), HOUSE_SOUND_TAVERN_NOT_ENOUGH_GOLD);
    engine->_messageQueue->addMessageCurrentFrame(UIMSG_Escape, 1, 0);
}

void GUIWindow_Tavern::buyFoodDialogue() {
    int pPriceFood = PriceCalculator::tavernFoodCostForPlayer(&pParty->activeCharacter(), houseTable[houseId()]);

    if ((double)pParty->GetFood() >= houseTable[houseId()].fPriceMultiplier) {
        engine->_statusBar->setEvent(LSTR_YOUR_PACKS_ARE_ALREADY_FULL);
        if (pParty->hasActiveCharacter()) {
            pParty->activeCharacter().playReaction(SPEECH_PACKS_FULL);
        }
        engine->_messageQueue->addMessageCurrentFrame(UIMSG_Escape, 1, 0);
        return;
    }
    if (pParty->GetGold() >= pPriceFood) {
        pParty->TakeGold(pPriceFood);
        pParty->SetFood(houseTable[houseId()].fPriceMultiplier);
        playHouseSound(houseId(), HOUSE_SOUND_TAVERN_BUY_FOOD);
        engine->_messageQueue->addMessageCurrentFrame(UIMSG_Escape, 1, 0);
        return;
    }
    engine->_statusBar->setEvent(LSTR_YOU_DONT_HAVE_ENOUGH_GOLD);
    playHouseSound(houseId(), HOUSE_SOUND_TAVERN_NOT_ENOUGH_GOLD);
    engine->_messageQueue->addMessageCurrentFrame(UIMSG_Escape, 1, 0);
}

// MM6.EXE tavern handler, Drink case @0x49F45C: a drink costs a flat 1 gold and marks this tavern
// as drunk-in (which is what gates Tip). Half the time the drinker hiccups aloud, and 1-in-3 of
// those come down with the Drunk condition; on the sober half there is a 1-in-4 chance of a random
// stat gaining an until-rest +5..10 bonus. The original shows no status line when short on gold
// here - just the proprietor's refusal bark.
void GUIWindow_Tavern::mm6DrinksDialogue() {
    if (pParty->GetGold() < 1) {
        playHouseSound(houseId(), HOUSE_SOUND_TAVERN_NOT_ENOUGH_GOLD);
        engine->_messageQueue->addMessageCurrentFrame(UIMSG_Escape, 1, 0);
        return;
    }

    pParty->TakeGold(1);
    engine->_statusBar->setEvent(LSTR_HIC);
    playHouseSound(houseId(), HOUSE_SOUND_TAVERN_BUY_FOOD);
    pParty->_mm6TavernsDrunkIn.insert(houseId());
    _mm6RumorText.clear();

    Character &drinker = pParty->activeCharacter();
    if (grng->random(2) == 1) {
        drinker.playReaction(SPEECH_TAVERN_GOT_DRUNK);
        if (grng->random(3) == 1)
            drinker.conditions.set(CONDITION_DRUNK, pParty->GetPlayingTime());
    } else {
        if (grng->random(4) == 1) {
            Attribute stat = grng->randomSample(Segment(ATTRIBUTE_FIRST_STAT, ATTRIBUTE_LAST_STAT));
            drinker._statBonuses[stat] += grng->random(6) + 5;
        }
        drinker.playReaction(SPEECH_TAVERN_DRINK);
    }

    engine->_messageQueue->addMessageCurrentFrame(UIMSG_Escape, 1, 0);
}

// MM6.EXE tavern handler, Tip case @0x49F716: tipping needs a prior drink in THIS tavern and costs
// 1 gold; the barkeep then tells a rumor from the same regional-news pool the street townsfolk
// greet with. The rumor is rolled ONCE per tavern and cached in the party - later tips still cost
// gold but repeat the same line (and only the first roll gets the thank-you voice line).
void GUIWindow_Tavern::mm6TipDialogue() {
    if (!pParty->_mm6TavernsDrunkIn.contains(houseId())) {
        engine->_statusBar->setEvent(LSTR_HAVE_A_DRINK_FIRST);
        engine->_messageQueue->addMessageCurrentFrame(UIMSG_Escape, 1, 0);
        return;
    }
    if (pParty->GetGold() < 1) {
        engine->_statusBar->setEvent(LSTR_YOU_DONT_HAVE_ENOUGH_GOLD);
        playHouseSound(houseId(), HOUSE_SOUND_TAVERN_NOT_ENOUGH_GOLD);
        engine->_messageQueue->addMessageCurrentFrame(UIMSG_Escape, 1, 0);
        return;
    }

    pParty->TakeGold(1);
    std::string &rumor = pParty->_mm6TavernRumors[houseId()];
    if (rumor.empty()) {
        rumor = pNPCStats->pickRandomNewsLine(engine->_currentLoadedMapId);
        if (pParty->hasActiveCharacter())
            pParty->activeCharacter().playReaction(SPEECH_TAVERN_TIP);
    }
    _mm6RumorText = rumor;

    engine->_messageQueue->addMessageCurrentFrame(UIMSG_Escape, 1, 0);
}

void GUIWindow_Tavern::houseDialogueOptionSelected(DialogueId option) {
    _currentDialogue = option;
    if (option == DIALOGUE_TAVERN_ARCOMAGE_RESULT) {
        engine->_messageQueue->addMessageCurrentFrame(UIMSG_PlayArcomage, 0, 0);
    } else if (IsSkillLearningDialogue(option)) {
        learnSelectedSkill(GetLearningDialogueSkill(option));
    }
}

void GUIWindow_Tavern::houseSpecificDialogue() {
    // TODO(pskelton): check this behaviour
    if (!pParty->hasActiveCharacter()) {  // avoid nzi
        pParty->setActiveToFirstCanAct();
    }

    switch (_currentDialogue) {
      case DIALOGUE_MAIN:
        mainDialogue();
        break;
      case DIALOGUE_TAVERN_ARCOMAGE_MAIN:
        arcomageMainDialogue();
        break;
      case DIALOGUE_TAVERN_ARCOMAGE_RULES:
        arcomageRulesDialogue();
        break;
      case DIALOGUE_TAVERN_ARCOMAGE_VICTORY_CONDITIONS:
        arcomageVictoryCondDialogue();
        break;
      case DIALOGUE_TAVERN_ARCOMAGE_RESULT:
        arcomageResultDialogue();
        break;
      case DIALOGUE_TAVERN_REST:
        restDialogue();
        break;
      case DIALOGUE_TAVERN_BUY_FOOD:
        buyFoodDialogue();
        break;
      case DIALOGUE_TAVERN_MM6_DRINKS:
        mm6DrinksDialogue();
        break;
      case DIALOGUE_TAVERN_MM6_TIP:
        mm6TipDialogue();
        break;
      case DIALOGUE_LEARN_SKILLS:
        learnSkillsDialogue(colorTable.PaleCanary);
        break;
      default:
        engine->_messageQueue->addMessageCurrentFrame(UIMSG_Escape, 1, 0);
        break;
    }

    // MM6 draws the tipped rumor in the dialogue panel for as long as it is current - it stays up
    // in the main menu until the next drink (the EXE's [0x9DDEB8] check at its handler tail).
    if (!_mm6RumorText.empty())
        DrawDialoguePanel(_mm6RumorText);
}

std::vector<DialogueId> GUIWindow_Tavern::listDialogueOptions() {
    // MM6 taverns have a flat four-option menu (MM6.EXE option factory tavern case @0x498828):
    // Rest / Fill Packs / Have a Drink / Tip Barkeep. No Arcomage, and no skill teaching - the
    // tavern skills (Stealing/Disarm/Perception) are thieves-guild subjects in MM6.
    if (engine->gameVersion() == GAME_VERSION_MM6) {
        if (_currentDialogue == DIALOGUE_MAIN) {
            return {DIALOGUE_TAVERN_REST, DIALOGUE_TAVERN_BUY_FOOD,
                    DIALOGUE_TAVERN_MM6_DRINKS, DIALOGUE_TAVERN_MM6_TIP};
        }
        return {};
    }

    switch (_currentDialogue) {
      case DIALOGUE_MAIN:
        if (houseId() == HOUSE_TAVERN_EMERALD_ISLAND) {
            return {DIALOGUE_TAVERN_REST, DIALOGUE_TAVERN_BUY_FOOD, DIALOGUE_LEARN_SKILLS};
        } else {
            return {DIALOGUE_TAVERN_REST, DIALOGUE_TAVERN_BUY_FOOD, DIALOGUE_LEARN_SKILLS, DIALOGUE_TAVERN_ARCOMAGE_MAIN};
        }
      case DIALOGUE_LEARN_SKILLS:
        return {DIALOGUE_LEARN_STEALING, DIALOGUE_LEARN_TRAP_DISARM, DIALOGUE_LEARN_PERCEPTION};
      case DIALOGUE_TAVERN_ARCOMAGE_MAIN:
        if (pParty->hasItem(ITEM_QUEST_ARCOMAGE_DECK)) {
            return {DIALOGUE_TAVERN_ARCOMAGE_RULES, DIALOGUE_TAVERN_ARCOMAGE_VICTORY_CONDITIONS, DIALOGUE_TAVERN_ARCOMAGE_RESULT};
        } else {
            return {DIALOGUE_TAVERN_ARCOMAGE_RULES, DIALOGUE_TAVERN_ARCOMAGE_VICTORY_CONDITIONS};
        }
      default:
        return {};
    }
}

void GUIWindow_Tavern::updateDialogueOnEscape() {
    if (IsSkillLearningDialogue(_currentDialogue)) {
        _currentDialogue = DIALOGUE_LEARN_SKILLS;
        return;
    }
    if (_currentDialogue == DIALOGUE_TAVERN_ARCOMAGE_RULES ||
        _currentDialogue == DIALOGUE_TAVERN_ARCOMAGE_VICTORY_CONDITIONS ||
        _currentDialogue == DIALOGUE_TAVERN_ARCOMAGE_RESULT) {
        _currentDialogue = DIALOGUE_TAVERN_ARCOMAGE_MAIN;
        return;
    }
    if (_currentDialogue == DIALOGUE_MAIN) {
        _currentDialogue = DIALOGUE_NULL;
        return;
    }
    _currentDialogue = DIALOGUE_MAIN;
}

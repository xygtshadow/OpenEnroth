#include "TownHall.h"

#include <algorithm>
#include <cassert>
#include <string>
#include <utility>
#include <vector>

#include "Engine/Objects/Monsters.h"
#include "Engine/Objects/MonsterEnumFunctions.h"
#include "Engine/Graphics/LocationFunctions.h"
#include "Engine/Graphics/Renderer/Renderer.h"
#include "Engine/Localization.h"
#include "Engine/Data/AwardEnums.h"
#include "Engine/Party.h"
#include "Engine/mm7_data.h"
#include "Engine/Engine.h"

#include "GUI/GUIMessageQueue.h"
#include "GUI/GUIWindow.h"
#include "GUI/GUIFont.h"
#include "GUI/UI/UIHouses.h"

#include "Io/KeyboardActionMapping.h"

#include "Engine/Random/Random.h"

#include "Engine/AssetsManager.h"

using Io::TextInputType;

// MM6 has three town halls (2dEvents houses 89-91: New Sorpigal, Castle Ironfist, Silver Cove) and
// keys their bounty state as houseId - 89 (MM6.EXE 0x4A31AF). The engine's bounty arrays are keyed
// by MM7's five town-hall house ids, so the MM6 town halls map onto the first three slots.
static HouseId bountyHuntSlot(HouseId townHall) {
    if (engine->gameVersion() == GAME_VERSION_MM6)
        return static_cast<HouseId>(std::to_underlying(HOUSE_FIRST_TOWN_HALL) + std::to_underlying(townHall) - 89);
    return townHall;
}

void GUIWindow_TownHall::mainDialogue() {
    // MM6 has no fine mechanic (the original has no Party fine field), so its town hall offers just
    // the bounty hunt, labeled with MM6's own npctopic row 399 - MM7's town-hall strings don't exist
    // in MM6's global.txt. (Note that InitializeNPCTopics stores row N at pNPCTopics[N], unlike the
    // -1-shifted pText.)
    if (engine->gameVersion() == GAME_VERSION_MM6) {
        std::vector<std::string> optionsText = {pNPCTopics[399].pTopic};
        drawOptions(optionsText, colorTable.PaleCanary, 170, true);
        return;
    }

    Recti townHall_window = this->frameRect;
    townHall_window.x = SIDE_TEXT_BOX_POS_X;
    townHall_window.w = SIDE_TEXT_BOX_WIDTH;

    std::vector<std::string> optionsText = {localization->str(LSTR_BOUNTY_HUNT)};
    std::string fine_str = fmt::format("{}: {}", localization->str(LSTR_CURRENT_FINE), pParty->uFine);
    DrawTitleText(assets->pFontArrus.get(), 0, 260, colorTable.PaleCanary, fine_str, 3, townHall_window);
    if (pParty->uFine > 0) {
        optionsText.push_back(localization->str(LSTR_PAY_FINE));
    }

    drawOptions(optionsText, colorTable.PaleCanary, 170, true);
}

void GUIWindow_TownHall::bountyHuntDialogue() {
    if (engine->gameVersion() != GAME_VERSION_MM6) { // No fine mechanic in MM6.
        Recti townHall_window = this->frameRect;
        townHall_window.x = SIDE_TEXT_BOX_POS_X;
        townHall_window.w = SIDE_TEXT_BOX_WIDTH;

        std::string fine_str = fmt::format("{}: {}", localization->str(LSTR_CURRENT_FINE), pParty->uFine);
        DrawTitleText(assets->pFontArrus.get(), 0, 260, colorTable.PaleCanary, fine_str, 3, townHall_window);
    }

    current_npc_text = bountyHuntingText();
    DrawDialoguePanel(current_npc_text);
}

void GUIWindow_TownHall::payFineDialogue() {
    Recti townHall_window = this->frameRect;
    townHall_window.x = SIDE_TEXT_BOX_POS_X;
    townHall_window.w = SIDE_TEXT_BOX_WIDTH;

    std::string fine_str = fmt::format("{}: {}", localization->str(LSTR_CURRENT_FINE), pParty->uFine);
    DrawTitleText(assets->pFontArrus.get(), 0, 260, colorTable.PaleCanary, fine_str, 3, townHall_window);

    if (keyboard_input_status == WINDOW_INPUT_IN_PROGRESS) {
        DrawTitleText(assets->pFontArrus.get(), 0, 146, colorTable.PaleCanary,
                                      fmt::format("{}\n{}", localization->str(LSTR_PAY), localization->str(LSTR_HOW_MUCH)), 3, townHall_window);
        DrawTitleText(assets->pFontArrus.get(), 0, 186, colorTable.White, keyboardInputHandler->GetTextInput(), 3, townHall_window);
        DrawFlashingInputCursor(assets->pFontArrus->GetLineWidth(keyboardInputHandler->GetTextInput()) / 2 + 80, 185, assets->pFontArrus.get(), townHall_window);
        return;
    } else if (keyboard_input_status == WINDOW_INPUT_CONFIRMED) {
        int sum = atoi(keyboardInputHandler->GetTextInput().c_str());
        if (sum > 0) {
            int party_gold = pParty->GetGold();
            if (sum > party_gold) {
                // TODO(Nik-RE-dev): game resources does not contain such sounds for town halls
                playHouseSound(houseId(), HOUSE_SOUND_GENERAL_NOT_ENOUGH_GOLD);
                sum = party_gold;
            }

            if (sum > 0) {
                int required_sum = pParty->GetFine();
                if (sum > required_sum)
                    sum = required_sum;

                pParty->TakeGold(sum);
                pParty->TakeFine(sum);
                if (pParty->hasActiveCharacter())
                    pParty->activeCharacter().playReaction(SPEECH_BANK_DEPOSIT);
            }
        }
    }
    keyboard_input_status = WINDOW_INPUT_NONE;
    engine->_messageQueue->addMessageCurrentFrame(UIMSG_Escape, 1, 0);
}

void GUIWindow_TownHall::houseSpecificDialogue() {
    switch (_currentDialogue) {
      case DIALOGUE_MAIN:
        mainDialogue();
        break;
      case DIALOGUE_TOWNHALL_BOUNTY_HUNT:
        bountyHuntDialogue();
        break;
      case DIALOGUE_TOWNHALL_PAY_FINE:
        payFineDialogue();
        break;
      default:
        break;
    }
}

void GUIWindow_TownHall::houseDialogueOptionSelected(DialogueId option) {
    _currentDialogue = option;
    if (option == DIALOGUE_TOWNHALL_BOUNTY_HUNT) {
        bountyHuntingDialogueOptionClicked();
    } else if (option == DIALOGUE_TOWNHALL_PAY_FINE) {
        keyboardInputHandler->StartTextInput(TextInputType::Number, 10, this);
    }
}

std::vector<DialogueId> GUIWindow_TownHall::listDialogueOptions() {
    switch (_currentDialogue) {
      case DIALOGUE_MAIN:
        if (pParty->uFine && engine->gameVersion() != GAME_VERSION_MM6) { // No fine mechanic in MM6.
            return {DIALOGUE_TOWNHALL_BOUNTY_HUNT, DIALOGUE_TOWNHALL_PAY_FINE};
        } else {
            return {DIALOGUE_TOWNHALL_BOUNTY_HUNT};
        }
      default:
        return {};
    }
}

MonsterId GUIWindow_TownHall::randomMonsterForHunting(HouseId townhall) {
    if (engine->gameVersion() == GAME_VERSION_MM6) {
        // MM6.EXE 0x4A3238: a uniform roll over monsters.txt rows 1-171, re-rolled while it hits one
        // of the non-monster rows; the pool is the same for all three town halls.
        while (true) {
            MonsterId result = static_cast<MonsterId>(grng->random(171) + 1);
            if (isBountyHuntableMm6(result))
                return result;
        }
    }

    while (true) {
        MonsterId result = grng->randomSample(allMonsters());
        if (isBountyHuntable(monsterTypeForMonsterId(result), townhall))
            return result;
    }
}

void GUIWindow_TownHall::bountyHuntingDialogueOptionClicked() {
    bool mm6 = engine->gameVersion() == GAME_VERSION_MM6;
    HouseId house = bountyHuntSlot(houseId());

    // Generate new bounty
    if (pParty->PartyTimes.bountyHuntNextGenTime[house] < pParty->GetPlayingTime()) {
        pParty->monster_for_hunting_killed[house] = false;
        pParty->PartyTimes.bountyHuntNextGenTime[house] = Time::fromMonths(pParty->GetPlayingTime().toMonths() + 1);
        pParty->monster_id_for_hunting[house] = randomMonsterForHunting(house);
    }

    _bountyHuntMonsterId = pParty->monster_id_for_hunting[house];

    // The reply texts are the same three consecutive npctext rows in both games, at each game's own
    // row numbers: MM6 368-370, MM7 352-354.
    if (!pParty->monster_for_hunting_killed[house]) {
        if (pParty->monster_id_for_hunting[house] != MONSTER_INVALID) {
            _bountyHuntText = pNPCTopics[mm6 ? 367 : 351].pText; // "This month's bounty is on a %s..."
        } else {
            _bountyHuntText = pNPCTopics[mm6 ? 369 : 353].pText; // "Someone has already claimed the bounty this month..."
        }
    } else {
        // Get prize
        if (pParty->monster_id_for_hunting[house] != MONSTER_INVALID) {
            int level = pMonsterStats->infos[pParty->monster_id_for_hunting[house]].level;
            int bounty = 100 * level;

            pParty->partyFindsGold(bounty, GOLD_RECEIVE_SHARE);
            if (mm6) {
                // MM6.EXE 0x4A32BE: every character gets MM6's award 81 ("Collected %u bounties"),
                // the bounty counter counts claims (not gold like MM7's), and the party's reputation
                // slides toward notorious by the monster's level - bounty hunting is killing for money.
                for (Character &player : pParty->pCharacters) {
                    player.SetVariable(VAR_Award, 81);
                }
                pParty->uNumBountiesCollected++;
                LocationInfo &location = currentLocationInfo();
                location.reputation = std::min(location.reputation + level, 10000);
            } else {
                for (Character &player : pParty->pCharacters) {
                    player.SetVariable(VAR_Award, std::to_underlying(AWARD_BOUNTIES_COLLECTED));
                }
                pParty->uNumBountiesCollected += bounty;
            }
            pParty->monster_id_for_hunting[house] = MONSTER_INVALID;
            pParty->monster_for_hunting_killed[house] = false;
        }

        _bountyHuntText = pNPCTopics[mm6 ? 368 : 352].pText; // "Congratulations on defeating the %s! Here is the %lu gold reward..."
    }
}

std::string GUIWindow_TownHall::bountyHuntingText() {
    assert(!_bountyHuntText.empty());

    // This happens when you claim a bounty and revisit the town hall the same month.
    // Assumes _bountyHuntText is already containing the "someone has already" text (pNPCTopics[353]).
    if (_bountyHuntMonsterId == MONSTER_INVALID)
        return _bountyHuntText;

    // TODO(captainurist): what do we do with exceptions inside fmt?
    std::string name = fmt::format("{::}{}{::}", colorTable.PaleCanary.tag(), pMonsterStats->infos[_bountyHuntMonsterId].name, colorTable.White.tag());
    return fmt::sprintf(_bountyHuntText, name, 100 * pMonsterStats->infos[_bountyHuntMonsterId].level); // NOLINT: this is not ::sprintf.
}

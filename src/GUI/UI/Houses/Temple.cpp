#include "Temple.h"

#include <algorithm>
#include <string>
#include <vector>

#include "GUI/UI/UIStatusBar.h"
#include "GUI/UI/UIGame.h"
#include "GUI/GUIFont.h"
#include "GUI/GUIMessageQueue.h"

#include "Engine/Localization.h"
#include "Engine/PriceCalculator.h"
#include "Engine/Graphics/LocationInfo.h"
#include "Engine/Graphics/LocationFunctions.h"
#include "Engine/Spells/CastSpellInfo.h"
#include "Engine/Party.h"
#include "Engine/Engine.h"

#include "Media/Audio/AudioPlayer.h"

void GUIWindow_Temple::mainDialogue() {
    int price = PriceCalculator::templeHealingCostForPlayer(&pParty->activeCharacter(), houseTable[houseId()].fPriceMultiplier);
    std::string healString = fmt::format("{} {} {}", localization->str(LSTR_HEAL), price, localization->str(LSTR_GOLD));
    std::vector<std::string> optionsText = {isPlayerHealableByTemple(pParty->activeCharacter()) ? healString : "",
                                            localization->str(LSTR_DONATE), localization->str(LSTR_LEARN_SKILLS)};

    drawOptions(optionsText, colorTable.PaleCanary);
}

void GUIWindow_Temple::healDialogue() {
    if (!isPlayerHealableByTemple(pParty->activeCharacter())) {
        return;
    }

    int price = PriceCalculator::templeHealingCostForPlayer(&pParty->activeCharacter(), houseTable[houseId()].fPriceMultiplier);
    if (pParty->GetGold() < price) {
        engine->_statusBar->setEvent(LSTR_YOU_DONT_HAVE_ENOUGH_GOLD);
        playHouseSound(houseId(), HOUSE_SOUND_GENERAL_NOT_ENOUGH_GOLD);
        engine->_messageQueue->addMessageCurrentFrame(UIMSG_Escape, 1, 0);
        return;
    }

    bool setZombie = false;
    // MM6 has no zombie mechanic - its heal (MM6.EXE 0x49e027) has no per-house branch, and the
    // MM7 evil-temple ids collide with MM6 2dEvents rows (HOUSE_TEMPLE_DEYJA == 78 == Temple Baa).
    bool isEvilTemple = engine->gameVersion() != GAME_VERSION_MM6 &&
                        (houseId() == HOUSE_TEMPLE_DEYJA || houseId() == HOUSE_TEMPLE_PIT || houseId() == HOUSE_TEMPLE_MOUNT_NIGHON);
    if (isEvilTemple) {
        setZombie = pParty->activeCharacter().conditions.has(CONDITION_ZOMBIE);
        if (!pParty->activeCharacter().conditions.has(CONDITION_ZOMBIE)) {
            if (pParty->activeCharacter().conditions.hasAny({CONDITION_ERADICATED, CONDITION_PETRIFIED, CONDITION_DEAD})) {
                pParty->activeCharacter().uPrevFace = pParty->activeCharacter().uCurrentFace;
                pParty->activeCharacter().uPrevVoiceID = pParty->activeCharacter().uVoiceID;
                pParty->activeCharacter().uVoiceID = (pParty->activeCharacter().GetSexByVoice() != SEX_MALE) + 23;
                pParty->activeCharacter().uCurrentFace = (pParty->activeCharacter().GetSexByVoice() != SEX_MALE) + 23;
                GameUI_ReloadPlayerPortraits(pParty->activeCharacterIndex() - 1, (pParty->activeCharacter().GetSexByVoice() != SEX_MALE) + 23);
                setZombie = true;
            }
        }
    } else {
        if (pParty->activeCharacter().conditions.has(CONDITION_ZOMBIE)) {
            pParty->activeCharacter().uCurrentFace = pParty->activeCharacter().uPrevFace;
            pParty->activeCharacter().uVoiceID = pParty->activeCharacter().uPrevVoiceID;
            GameUI_ReloadPlayerPortraits(pParty->activeCharacterIndex() - 1, pParty->activeCharacter().uPrevFace);
        }
    }

    pParty->activeCharacter().conditions.resetAll();
    if (setZombie) {
        pParty->activeCharacter().conditions.set(CONDITION_ZOMBIE, pParty->GetPlayingTime());
    }
    pParty->TakeGold(price);
    pParty->activeCharacter().health = pParty->activeCharacter().GetMaxHealth();
    pParty->activeCharacter().mana = pParty->activeCharacter().GetMaxMana();
    pAudioPlayer->playExclusiveSound(SOUND_heal);
    pParty->activeCharacter().playReaction(SPEECH_TEMPLE_HEAL);
    engine->_messageQueue->addMessageCurrentFrame(UIMSG_Escape, 1, 0);
}

void GUIWindow_Temple::donateDialogue() {
    int price = houseTable[houseId()].fPriceMultiplier;
    if (pParty->GetGold() >= price) {
        pParty->TakeGold(price);
        LocationInfo *ddm = &currentLocationInfo();

        if (engine->gameVersion() == GAME_VERSION_MM6) {
            // MM6.EXE 0x49DDC6..0x49DF50, positive = good: a donation lifts reputation by 200
            // up to a cap of +200; on the weekday matching the donation counter the temple
            // blesses the party with one spell per display-reputation band above 200 (MM6
            // native ids, cast at day-of-month%7+1 like MM7's); and New Sorpigal's Temple Baa
            // (house 78) then takes the 200 straight back while above -800 - Baa keeps the
            // gold. The counter is per character; the buff gates read the hireling-adjusted
            // display value (the getter at 0x47D600).
            if (ddm->reputation < 200) {
                ddm->reputation = std::min(ddm->reputation + 200, 200);
            }
            int day = pParty->uCurrentDayOfMonth % 7;
            int counter = _templeSpellCounter[pParty->activeCharacterIndex() - 1] % 7;
            if (counter == day) {
                int displayReputation = pParty->GetPartyReputation();
                if (displayReputation > 200) {
                    pushTempleSpell(static_cast<SpellId>(50)); // Guardian Angel
                }
                if (displayReputation > 400) {
                    pushTempleSpell(static_cast<SpellId>(12)); // Wizard Eye
                }
                if (displayReputation > 600) {
                    pushTempleSpell(static_cast<SpellId>(83)); // Day of the Gods
                }
                if (displayReputation > 800) {
                    pushTempleSpell(static_cast<SpellId>(85)); // Hour of Power
                }
                if (displayReputation > 1000) {
                    pushTempleSpell(static_cast<SpellId>(94)); // Day of Protection
                }
            }
            _templeSpellCounter[pParty->activeCharacterIndex() - 1]++;
            if (houseId() == HouseId(78) && ddm->reputation > -800) {
                ddm->reputation -= 200;
            }
        } else {
            if (ddm->reputation > -5) {
                ddm->reputation -= 1;
            }
            int day = pParty->uCurrentDayOfMonth % 7;
            int counter = _templeSpellCounter[pParty->activeCharacterIndex() - 1] % 7;
            if (counter == day) {
                if (ddm->reputation <= -5) {
                    pushTempleSpell(SPELL_AIR_WIZARD_EYE);
                }
                if (ddm->reputation <= -10) {
                    pushTempleSpell(SPELL_SPIRIT_PRESERVATION);
                }
                if (ddm->reputation <= -15) {
                    pushTempleSpell(SPELL_BODY_PROTECTION_FROM_MAGIC);
                }
                if (ddm->reputation <= -20) {
                    pushTempleSpell(SPELL_LIGHT_HOUR_OF_POWER);
                }
                if (ddm->reputation <= -25) {
                    pushTempleSpell(SPELL_LIGHT_DAY_OF_PROTECTION);
                }
            }
            _templeSpellCounter[pParty->activeCharacterIndex() - 1]++;
        }
        pParty->activeCharacter().playReaction(SPEECH_TEMPLE_DONATE);
        engine->_statusBar->setEvent(LSTR_THANK_YOU);
    } else {
        engine->_statusBar->setEvent(LSTR_YOU_DONT_HAVE_ENOUGH_GOLD);
    }
    engine->_messageQueue->addMessageCurrentFrame(UIMSG_Escape, 1, 0);
}

GUIWindow_Temple::GUIWindow_Temple(HouseId houseId) : GUIWindow_House(houseId) {
    _templeSpellCounter.resize(pParty->pCharacters.size());
    std::fill(_templeSpellCounter.begin(), _templeSpellCounter.end(), 0);
}

void GUIWindow_Temple::houseDialogueOptionSelected(DialogueId option) {
    _currentDialogue = option;
    if (IsSkillLearningDialogue(option)) {
        learnSelectedSkill(GetLearningDialogueSkill(option));
    }
}

void GUIWindow_Temple::houseSpecificDialogue() {
    // TODO(pskelton): check this behaviour
    if (!pParty->hasActiveCharacter()) {  // avoid nzi
        pParty->setActiveToFirstCanAct();
    }

    switch (_currentDialogue) {
      case DIALOGUE_MAIN:
        mainDialogue();
        break;
      case DIALOGUE_TEMPLE_HEAL:
        healDialogue();
        break;
      case DIALOGUE_TEMPLE_DONATE:
        donateDialogue();
        break;
      case DIALOGUE_LEARN_SKILLS:
        learnSkillsDialogue(colorTable.PaleCanary);
        break;
      default:
        engine->_messageQueue->addMessageCurrentFrame(UIMSG_Escape, 1, 0);
        break;
    }
}

std::vector<DialogueId> GUIWindow_Temple::listDialogueOptions() {
    switch (_currentDialogue) {
      case DIALOGUE_MAIN:
        return {DIALOGUE_TEMPLE_HEAL, DIALOGUE_TEMPLE_DONATE, DIALOGUE_LEARN_SKILLS};
      case DIALOGUE_LEARN_SKILLS:
        return {DIALOGUE_LEARN_UNARMED, DIALOGUE_LEARN_DODGE, DIALOGUE_LEARN_MERCHANT};
      default:
        return {};
    }
}

void GUIWindow_Temple::updateDialogueOnEscape() {
    if (IsSkillLearningDialogue(_currentDialogue)) {
        _currentDialogue = DIALOGUE_LEARN_SKILLS;
        return;
    }
    if (_currentDialogue == DIALOGUE_MAIN) {
        _currentDialogue = DIALOGUE_NULL;
        return;
    }
    _currentDialogue = DIALOGUE_MAIN;
}

void GUIWindow_Temple::playHouseGoodbyeSpeech() {
    playHouseSound(houseId(), HOUSE_SOUND_TEMPLE_GOODBYE);
}

bool GUIWindow_Temple::isPlayerHealableByTemple(const Character &player) const {
    if (player.health >= player.GetMaxHealth() && player.mana >= player.GetMaxMana() && player.GetMajorConditionIdx() == CONDITION_GOOD) {
        // fully healthy
        return false;
    } else if (player.GetMajorConditionIdx() == CONDITION_ZOMBIE) {
        // zombie cant be healed at these tmeples - but MM6 temples aren't evil temples (see
        // healDialogue) and cure a zombie from a contaminated save like any other condition
        return engine->gameVersion() == GAME_VERSION_MM6 ||
               (houseId() != HOUSE_TEMPLE_DEYJA && houseId() != HOUSE_TEMPLE_PIT && houseId() != HOUSE_TEMPLE_MOUNT_NIGHON);
    }

    return true;
}

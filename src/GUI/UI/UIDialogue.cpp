#include "UIDialogue.h"

#include <algorithm>
#include <memory>
#include <vector>
#include <string>

#include "Engine/Graphics/Renderer/Renderer.h"
#include "Engine/Graphics/Image.h"
#include "Engine/Graphics/LocationFunctions.h"
#include "Engine/Objects/Decoration.h"
#include "Engine/Localization.h"
#include "Engine/Objects/Actor.h"
#include "Engine/Objects/NPC.h"
#include "Engine/Party.h"
#include "Engine/Pid.h"
#include "Engine/mm7_data.h"
#include "Engine/AssetsManager.h"
#include "Engine/Engine.h"
#include "Engine/Graphics/Viewport.h"

#include "GUI/GUIFont.h"
#include "GUI/GUIButton.h"
#include "GUI/GUIMessageQueue.h"
#include "GUI/UI/UIGame.h"
#include "GUI/UI/UIHouses.h"
#include "GUI/UI/UIStatusBar.h"
#include "GUI/UI/NPCTopics.h"

#include "Io/KeyboardInputHandler.h"

#include "Media/Audio/AudioPlayer.h"

#include "Utility/String/Ascii.h"

using Io::TextInputType;

int speakingNpcId;
Actor *currentSpeakingActor = nullptr;

const IndexedArray<std::string, PartyAlignment_Good, PartyAlignment_Evil> dialogueBackgroundResourceByAlignment = {
    {PartyAlignment_Good, "evt02-b"},
    {PartyAlignment_Neutral, "evt02"},
    {PartyAlignment_Evil, "evt02-c"}
};

void initializeNPCDialogue(int npcId, int bPlayerSaysHello, Actor *actor) {
    pNPCStats->dword_AE336C_LastMispronouncedNameFirstLetter = -1;
    pNPCStats->mm6LastAddressingAwardPick = -1;
    pEventTimer->setPaused(true);
    pMiscTimer->setPaused(true);
    speakingNpcId = npcId;
    currentSpeakingActor = actor;
    NPCData *pNPCInfo = getNPCData(npcId);
    if (engine->gameVersion() != GAME_VERSION_MM6) {
        // MM7 greeted-once/greeted-before bookkeeping. MM6's greet byte is a STATE machine instead
        // (0 never talked / 1 talked / 2 begged / 3 bribed / 4 threatened) written by the option
        // clicks; opening the dialogue leaves it alone (MM6.EXE 0x43BE50).
        if (!(pNPCInfo->flags & NPC_GREETED_SECOND)) {
            if (pNPCInfo->flags & NPC_GREETED_FIRST) {
                pNPCInfo->flags &= ~NPC_GREETED_FIRST;
                pNPCInfo->flags |= NPC_GREETED_SECOND;
            } else {
                pNPCInfo->flags |= NPC_GREETED_FIRST;
            }
        }
    }

    if (engine->gameVersion() == GAME_VERSION_MM6) {
        // MM6.EXE 0x43be50: street dialogue always uses the fixed marble panel evpan019.
        game_ui_dialogue_background = assets->getImage_Solid("evpan019");
    } else {
        game_ui_dialogue_background = assets->getImage_Solid(dialogueBackgroundResourceByAlignment[pParty->alignment]);
    }

    currentHouseNpc = 0;

    HouseNpcDesc desc;
    desc.type = HOUSE_NPC;
    desc.label = localization->format(LSTR_CONVERSE_WITH_S, pNPCInfo->name);
    desc.icon = assets->getImage_ColorKey(fmt::format("npc{:03}", pNPCInfo->portraitId));
    desc.npc = pNPCInfo;

    houseNpcs.push_back(desc);

    // TODO(Nik-RE-dev): this looks like checks for NPC that only talk if party has enough fame
    //                   which is a thing only for MM8 if I remember correctly
#if 0
    int pNumberContacts = 0;
    int v9 = 0;
    if (!pNPCInfo->Hired() && pNPCInfo->house >= 0) {
        if (pParty->getPartyFame() <= pNPCInfo->fame ||
            (pNumberContacts = pNPCInfo->flags & 0xFFFFFF7F,
             (pNumberContacts & 0x80000000u) != 0)) {
            v9 = 1;
        } else {
            if (pNumberContacts > 1) {
                if (pNumberContacts == 2) {
                    v9 = 3;
                } else {
                    if (pNumberContacts != 3) {
                        if (pNumberContacts != 4) v9 = 1;
                    } else {
                        v9 = 2;
                    }
                }
            } else if (pNPCInfo->rep) {
                v9 = 2;
            }
        }
    }
    if (speakingNpcId < 0) v9 = 4;
#endif

    pDialogueWindow = std::make_unique<GUIWindow_Dialogue>(DIALOG_WINDOW_FULL);

    if (bPlayerSaysHello && pParty->hasActiveCharacter() && !pNPCInfo->Hired()) {
        if (engine->gameVersion() == GAME_VERSION_MM6) {
            // MM6.EXE 0x43C08B: the hello reaction is picked by the party's display reputation,
            // not by the hour - MM6's speech bank keeps a friendly and a wary hello in these slots.
            pParty->activeCharacter().playReaction(pParty->GetPartyReputation() >= 0 ? SPEECH_GOOD_DAY : SPEECH_GOOD_EVENING);
        } else if (pParty->uCurrentHour < 5 || pParty->uCurrentHour > 21) {
            pParty->activeCharacter().playReaction(SPEECH_GOOD_EVENING);
        } else {
            pParty->activeCharacter().playReaction(SPEECH_GOOD_DAY);
        }
    }
}

// MM6's NPC greet state (the low bits of the flags byte): 0 = never talked to, 1 = talked to,
// 2 = begged, 3 = bribed, 4 = threatened. The hired bit 0x80 lives in the same byte.
static int mm6GreetState(const NPCData *npc) {
    return std::to_underlying(npc->flags) & 0x7f;
}

// The MM6 street reputation gate (MM6.EXE 0x43BF71): no requirement always passes; a good NPC
// (positive requirement) demands display reputation ABOVE it, an evil one (negative) demands
// display reputation BELOW it.
static bool mm6RepGatePassed(const NPCData *npc) {
    if (npc->rep == 0)
        return true;
    int reputation = pParty->GetPartyReputation();
    if (reputation > 0 && npc->rep > 0 && reputation > npc->rep)
        return true;
    return reputation < 0 && npc->rep < 0 && reputation < npc->rep;
}

// Which menu an MM6 street NPC offers (MM6.EXE 0x43BF39): hired NPCs always talk; then the fame
// gate, then by greet state - begged/bribed-before keep only the Beg/Threaten/Bribe menu (a bribe
// must be paid again on every visit; a beggar gets brushed off), threatened-before talk forever,
// and fresh conversations run the reputation gate.
static Mm6StreetDialoguePage mm6StreetPageFor(const NPCData *npc) {
    if (npc->flags & NPC_HIRED)
        return MM6_STREET_PAGE_TALK;
    if (pParty->getPartyFame() <= npc->fame)
        return MM6_STREET_PAGE_FAME_REFUSAL;
    switch (mm6GreetState(npc)) {
      case 2:
      case 3:
        return MM6_STREET_PAGE_BTB;
      case 4:
        return MM6_STREET_PAGE_TALK;
      default:
        return mm6RepGatePassed(npc) ? MM6_STREET_PAGE_TALK : MM6_STREET_PAGE_BTB;
    }
}

// The character whose voice and stats street dialogue uses - the active one, like MM6.EXE's
// [0x4D50E8] (falling back to the first character when nobody is active).
static Character &mm6Speaker() {
    return pParty->hasActiveCharacter() ? pParty->activeCharacter() : pParty->pCharacters[0];
}

static int mm6SpeakerId() {
    return pParty->hasActiveCharacter() ? pParty->activeCharacterIndex() - 1 : 0;
}

// One npcbtb.txt reaction line for the NPC's personality, %-tokens expanded.
static std::string mm6BtbText(int row, NPCData *npc) {
    NpcPersonality personality = pNPCStats->mm6PersonalityByProfession[npc->profession];
    return BuildDialogueString(pNPCStats->mm6BtbTexts[row][personality], mm6SpeakerId(), npc);
}

// The greeting an MM6 street NPC opens with - the npcbtb.txt row selection of the dialogue text
// drawer (MM6.EXE 0x43AEB3): fame refusal, beg/bribe/threat returns, or a reputation-flavored
// greeting/refusal with distinct first-visit and repeat-visit lines.
static std::string mm6StreetGreeting(NPCData *npc) {
    if (!npc->Hired() && pParty->getPartyFame() <= npc->fame)
        return mm6BtbText(6, npc); // Fame too low.

    int greet = mm6GreetState(npc);
    if (greet >= 2 && greet <= 4)
        return mm6BtbText(greet + 1, npc); // Rows 3/4/5: begged / bribed / threatened before.

    int reputation = pParty->GetPartyReputation();
    int required = npc->rep;
    if (mm6RepGatePassed(npc)) {
        if (reputation <= -1000 && required < 0)
            return mm6BtbText(8, npc); // Notorious party, evil NPC: an extra-warm welcome.
        if (reputation >= 1000 && required > 0)
            return mm6BtbText(9, npc); // Saintly party, good NPC.
        return mm6BtbText(greet == 0 ? 1 : 2, npc); // The plain first/repeat greeting.
    }
    if (reputation <= -1000 && required > 0)
        return mm6BtbText(7, npc); // Notorious party, good NPC.
    if (reputation >= 1000 && required < 0)
        return mm6BtbText(10, npc); // Saintly party, evil NPC.
    int base = greet == 0 ? 11 : 15; // Rows 11-14 on the first visit, 15-18 on repeats.
    if (reputation <= 0 && required > 0)
        return mm6BtbText(base, npc); // "Rep below zero".
    if (reputation >= 0 && required < 0)
        return mm6BtbText(base + 1, npc); // "Rep above ten, and I'm evil".
    if (reputation > 0 && required >= reputation)
        return mm6BtbText(base + 2, npc); // "You aren't good enough".
    return mm6BtbText(base + 3, npc); // "You aren't bad enough".
}

int mm6DiplomacyBonus(const Character &character) {
    CombinedSkillValue diplomacy = character.pActiveSkills[SKILL_DIPLOMACY];
    int level = diplomacy.level();
    if (CheckHiredNPCSpeciality(Counselor))
        level += 4;
    if (CheckHiredNPCSpeciality(Barrister))
        level += 8;
    if (CheckHiredNPCSpeciality(Negotiator))
        level += 4;
    int multiplier = diplomacy.mastery() >= MASTERY_MASTER ? 4 : diplomacy.mastery() == MASTERY_EXPERT ? 3 : 2;
    return level * multiplier;
}

int mm6BribeCost() {
    return std::max(10, (100 - mm6DiplomacyBonus(mm6Speaker())) * (pParty->_mm6NpcBribeCount + 1) / 2);
}

GUIWindow_Dialogue::GUIWindow_Dialogue(DialogWindowType type) : GUIWindow(WINDOW_Dialogue, {0, 0}, render->GetRenderDimensions()) {
    prev_screen_type = current_screen_type;
    current_screen_type = SCREEN_NPC_DIALOGUE;
    if (engine->gameVersion() == GAME_VERSION_MM6) {
        pBtn_ExitCancel = CreateButton(MM6_DIALOGUE_ESC_CENTERED_POS, MM6_DIALOGUE_BUTTON_SIZE, BUTTON_TYPE_NORMAL, 0, UIMSG_Escape, 0, INPUT_ACTION_INVALID,
                                       localization->str(LSTR_EXIT_DIALOGUE), {ui_exit_cancel_button_background});
    } else {
        pBtn_ExitCancel = CreateButton({0x1D7u, 0x1BDu}, {0xA9u, 0x23u}, BUTTON_TYPE_NORMAL, 0, UIMSG_Escape, 0, INPUT_ACTION_INVALID,
                                       localization->str(LSTR_EXIT_DIALOGUE), {ui_exit_cancel_button_background});
    }

    int text_line_height = assets->pFontArrus->GetHeight() - 3;
    NPCData *speakingNPC = getNPCData(speakingNpcId);
    std::vector<DialogueId> optionList;

    if (type == DIALOG_WINDOW_FULL) {
        if (getNPCType(speakingNpcId) == NPC_TYPE_QUEST) {
            optionList = prepareScriptedNPCDialogueTopics(speakingNPC);
        } else if (engine->gameVersion() == GAME_VERSION_MM6) {
            // MM6 street menu (MM6.EXE window pages @0x4195F9): an NPC that talks offers the
            // profession small talk / Join / News trio; one that refuses over reputation (or was
            // begged or bribed before) offers only Beg / Threaten / Bribe; one that refuses over
            // fame offers nothing. The news line is assigned once per NPC, at the first dialogue.
            if (speakingNPC->mm6News.text.empty())
                speakingNPC->mm6News = pNPCStats->pickRandomNewsEntry(engine->_currentLoadedMapId);
            _mm6StreetPage = mm6StreetPageFor(speakingNPC);
            switch (_mm6StreetPage) {
              case MM6_STREET_PAGE_TALK:
                optionList = {DIALOGUE_STREET_MM6_PROF_TOPIC, DIALOGUE_HIRE_FIRE, DIALOGUE_STREET_MM6_NEWS};
                break;
              case MM6_STREET_PAGE_FAME_REFUSAL:
                break;
              case MM6_STREET_PAGE_BTB:
                optionList = {DIALOGUE_STREET_MM6_BEG, DIALOGUE_STREET_MM6_THREATEN, DIALOGUE_STREET_MM6_BRIBE};
                break;
            }
        } else if (speakingNPC->canJoin) {
            optionList = {DIALOGUE_PROFESSION_DETAILS, DIALOGUE_HIRE_FIRE};
        }
        if (speakingNPC->Hired() && !speakingNPC->hasUsedAbility) {
            if (speakingNPC->profession == Healer || speakingNPC->profession == ExpertHealer ||
                speakingNPC->profession == MasterHealer || speakingNPC->profession == Cook ||
                speakingNPC->profession == Chef || speakingNPC->profession == WindMaster ||
                speakingNPC->profession == WaterMaster || speakingNPC->profession == GateMaster ||
                speakingNPC->profession == Acolyte ||  // or Chaplain? mb discrepancy between game versions?
                speakingNPC->profession == Piper || speakingNPC->profession == FallenWizard) {
                optionList.push_back(DIALOGUE_USE_HIRED_NPC_ABILITY);
                // TODO(Nik-RE-dev): this is for compatability. Previously when NPC can use ability, dialogue allocated 4 buttons unconditionally.
                //                   Without it many test will fail because of changed buttons positions.
                optionList.push_back(DIALOGUE_NULL);
            }
        }
    } else {
        assert(type == DIALOG_WINDOW_HIRE_FIRE_SHORT);
        if (!pNPCStats->pProfessions[speakingNPC->profession].pBenefits.empty()) {
            optionList.push_back(DIALOGUE_PROFESSION_DETAILS);
        }
        optionList.push_back(DIALOGUE_HIRE_FIRE);
    }
    for (int i = 0; i < optionList.size(); i++) {
        CreateButton({480, 130 + i * text_line_height}, {140, text_line_height}, BUTTON_TYPE_NORMAL, 0, UIMSG_SelectNPCDialogueOption, std::to_underlying(optionList[i]), INPUT_ACTION_INVALID, "");
    }
    setKeyboardControlGroup(optionList.size(), false, 0, 1);

    CreateCharacterButtons();
}

GUIWindow_Dialogue::~GUIWindow_Dialogue() {
    if (houseNpcs[0].icon) {
        houseNpcs[0].icon->release();
    }
    houseNpcs.clear();

    if (game_ui_dialogue_background) {
        game_ui_dialogue_background->release();
        game_ui_dialogue_background = nullptr;
    }

    current_screen_type = prev_screen_type;
    currentSpeakingActor = nullptr;
    pParty->switchToNextActiveCharacter();
}

void GUIWindow_Dialogue::Update() {
    if (!pDialogueWindow) {
        return;
    }

    // Window title(Заголовок окна)----
    NPCData *pNPC = getNPCData(speakingNpcId);
    NpcType npcType = getNPCType(speakingNpcId);
    if (engine->gameVersion() == GAME_VERSION_MM6) {
        // MM6.EXE 0x43ab90: the HUD is already drawn; blit the marble panel over the right column
        // and the NPC portrait directly on it - MM6 has no evtnpc portrait frame.
        render->DrawQuad2D(game_ui_dialogue_background, MM6_DIALOGUE_PANEL_POS);
        render->DrawQuad2D(houseNpcs[0].icon, MM6_DIALOGUE_PORTRAIT_POS);
    } else {
        render->DrawQuad2D(game_ui_dialogue_background, {477, 0});
        render->DrawQuad2D(game_ui_right_panel_frame, {468, 0});
        render->DrawQuad2D(game_ui_evtnpc, {pNPCPortraits_x[0][0] - 4, pNPCPortraits_y[0][0] - 4});
        render->DrawQuad2D(houseNpcs[0].icon, {pNPCPortraits_x[0][0], pNPCPortraits_y[0][0]});
    }

    Recti titleWindow = pDialogueWindow->frameRect;
    titleWindow.w -= 10;
    DrawTitleText(assets->pFontArrus.get(), SIDE_TEXT_BOX_POS_X, SIDE_TEXT_BOX_POS_Y, ui_game_dialogue_npc_name_color, NameAndTitle(pNPC), 3, titleWindow);

    // TODO(pskelton): nothing done with fame here?
    pParty->getPartyFame();

    std::string dialogue_string;
    switch (_displayedDialogue) {
        case DIALOGUE_13_hiring_related:
            dialogue_string = BuildDialogueString(pNPCStats->pProfessions[pNPC->profession].pJoinText, 0, pNPC);
            break;

        case DIALOGUE_STREET_MM6_PROF_TOPIC:
            // The profession's small talk for the current weekday, drawn raw (MM6.EXE 0x43AD0B).
            dialogue_string = pNPCStats->mm6ProfText[pNPC->profession][pParty->uCurrentDayOfMonth % 7].text;
            break;

        case DIALOGUE_STREET_MM6_NEWS:
            dialogue_string = pNPC->mm6News.text;
            break;

        case DIALOGUE_HIRE_FIRE:
            // Displayed only when an MM6 one-click Join failed (not enough gold / party full) -
            // success closes the dialogue. The join offer stays up, like MM6.EXE's state 13.
            dialogue_string = BuildDialogueString(pNPCStats->pProfessions[pNPC->profession].pJoinText, 0, pNPC);
            break;

        case DIALOGUE_STREET_MM6_BEG:
            dialogue_string = mm6BtbText(pNPCStats->mm6PersonalityAcceptsBeg[pNPCStats->mm6PersonalityByProfession[pNPC->profession]] ? 19 : 20, pNPC);
            break;

        case DIALOGUE_STREET_MM6_THREATEN:
            dialogue_string = mm6BtbText(pNPCStats->mm6PersonalityAcceptsThreat[pNPCStats->mm6PersonalityByProfession[pNPC->profession]] ? 23 : 24, pNPC);
            break;

        case DIALOGUE_STREET_MM6_BRIBE:
            dialogue_string = mm6BtbText(pNPCStats->mm6PersonalityAcceptsBribe[pNPCStats->mm6PersonalityByProfession[pNPC->profession]] ? 21 : 22, pNPC);
            break;

        case DIALOGUE_PROFESSION_DETAILS: {
            if (dialogue_show_profession_details) {
                dialogue_string = BuildDialogueString(pNPCStats->pProfessions[pNPC->profession].pBenefits, 0, pNPC);
            } else if (pNPC->Hired()) {
                dialogue_string = BuildDialogueString(pNPCStats->pProfessions[pNPC->profession].pDismissText, 0, pNPC);
            } else {
                dialogue_string = BuildDialogueString(pNPCStats->pProfessions[pNPC->profession].pJoinText, 0, pNPC);
            }
            break;
        }

        case DIALOGUE_ARENA_WELCOME:
            dialogue_string = localization->str(LSTR_WELCOME_TO_THE_ARENA_OF_LIFE_AND_DEATH);
            break;

        case DIALOGUE_ARENA_FIGHT_NOT_OVER_YET:
            dialogue_string = localization->str(LSTR_GET_BACK_IN_THERE_YOU_WIMPS);
            break;

        case DIALOGUE_ARENA_REWARD:
            dialogue_string = localization->format(LSTR_CONGRATULATIONS_ON_YOUR_WIN_HERES_YOUR, gold_transaction_amount);
            break;

        case DIALOGUE_ARENA_ALREADY_WON:
            dialogue_string = localization->str(LSTR_YOU_ALREADY_WON_THIS_TRIP_TO_THE_ARENA);
            break;

        default:
            if (_displayedDialogue >= DIALOGUE_SCRIPTED_LINE_1 && _displayedDialogue < DIALOGUE_SCRIPTED_LINE_6 &&
                branchless_dialogue_str.empty()) {
                dialogue_string = current_npc_text;
            } else if (npcType == NPC_TYPE_QUEST) {
                if (pNPC->greetingIndex) {
                    if (pNPC->flags & NPC_GREETED_SECOND)
                        dialogue_string = pNPCStats->pNPCGreetings[pNPC->greetingIndex].pGreeting2;
                    else
                        dialogue_string = pNPCStats->pNPCGreetings[pNPC->greetingIndex].pGreeting1;
                }
            } else if (npcType == NPC_TYPE_HIREABLE) {
                NPCProfession *prof = &pNPCStats->pProfessions[pNPC->profession];

                if (pNPC->Hired()) {
                    dialogue_string = BuildDialogueString(prof->pDismissText, 0, pNPC);
                } else if (engine->gameVersion() == GAME_VERSION_MM6) {
                    dialogue_string = mm6StreetGreeting(pNPC);
                } else {
                    dialogue_string = BuildDialogueString(prof->pJoinText, 0, pNPC);
                }
            }
            break;
    }

    // Message window
    pDialogueWindow->DrawDialoguePanel(dialogue_string);

    // Right panel(Правая панель)-------
    for (int i = pDialogueWindow->pStartingPosActiveItem; i < pDialogueWindow->pStartingPosActiveItem + pDialogueWindow->pNumPresenceButton; ++i) {
        GUIButton *pButton = pDialogueWindow->GetControl(i);
        if (!pButton) {
            break;
        }

        DialogueId topic = (DialogueId)pButton->msg_param;
        pButton->sLabel = npcDialogueOptionString(topic, pNPC);
        if (pButton->sLabel.empty() && topic >= DIALOGUE_SCRIPTED_LINE_1 && topic <= DIALOGUE_SCRIPTED_LINE_6) {
            pButton->msg_param = 0;
        }

        if (pParty->arenaState == ARENA_STATE_FIGHTING) {
            int num_dead_actors = 0;
            for (const Actor &actor : pActors) {
                if (actor.aiState == Dead ||
                    actor.aiState == Removed ||
                    actor.aiState == Disabled) {
                    ++num_dead_actors;
                } else {
                    if (actor.summonerId.type() == OBJECT_Character)
                        ++num_dead_actors;
                }
            }
            if (num_dead_actors == pActors.size()) {
                pButton->sLabel = localization->str(LSTR_COLLECT_PRIZE);
            }
        }
    }

    // Install Buttons(Установка кнопок)--------
    Recti window = pDialogueWindow->frameRect;
    window.x = SIDE_TEXT_BOX_POS_X;
    window.w = SIDE_TEXT_BOX_WIDTH;
    int index = 0;
    int all_text_height = 0;
    for (int i = pDialogueWindow->pStartingPosActiveItem; i < pDialogueWindow->pStartingPosActiveItem + pDialogueWindow->pNumPresenceButton; ++i) {
        GUIButton *pButton = pDialogueWindow->GetControl(i);
        if (!pButton)
            break;
        all_text_height += assets->pFontArrus->CalcTextHeight(pButton->sLabel, window.w, 0);
        index++;
    }

    if (index) {
        int v45 = (SIDE_TEXT_BOX_BODY_TEXT_HEIGHT - all_text_height) / index;
        if (v45 > SIDE_TEXT_BOX_MAX_SPACING)
            v45 = SIDE_TEXT_BOX_MAX_SPACING;
        int v42 = (SIDE_TEXT_BOX_BODY_TEXT_HEIGHT - v45 * index - all_text_height) / 2 - v45 / 2 + SIDE_TEXT_BOX_BODY_TEXT_OFFSET;
        for (int i = pDialogueWindow->pStartingPosActiveItem; i < pDialogueWindow->pNumPresenceButton + pDialogueWindow->pStartingPosActiveItem; ++i) {
            GUIButton *pButton = pDialogueWindow->GetControl(i);
            if (!pButton)
                break;
            pButton->rect.y = v45 + v42;
            int pTextHeight = assets->pFontArrus->CalcTextHeight(pButton->sLabel, window.w, 0);
            pButton->rect.h = pTextHeight + 1;
            v42 = pButton->rect.y + pTextHeight - 1;
            Color pTextColor = ui_game_dialogue_option_normal_color;
            if (pDialogueWindow->pCurrentPosActiveItem == i) {
                pTextColor = ui_game_dialogue_option_highlight_color;
            }
            DrawTitleText(assets->pFontArrus.get(), 0, pButton->rect.y, pTextColor, pButton->sLabel, 3, window);
        }
    }
    render->DrawQuad2D(ui_exit_cancel_button_background,
                       engine->gameVersion() == GAME_VERSION_MM6 ? MM6_DIALOGUE_ESC_CENTERED_POS : Pointi(471, 445));
}

void BuildHireableNpcDialogue() {
    pDialogueWindow = std::make_unique<GUIWindow_Dialogue>(DIALOG_WINDOW_HIRE_FIRE_SHORT);
}

// The MM6 Beg / Threaten / Bribe click handlers (MM6.EXE 0x4A3E19 / 0x4A3F16 / 0x4A4077). Whether
// the NPC gives in is purely its personality's npcbtb.txt flag; success writes the greet state
// (2/3/4) and costs reputation - the better the speaker's Diplomacy, the less: -max(0, 10/50/20 -
// bonus) for beg/threaten/bribe. A bribe also costs gold, rising with every bribe ever paid, and a
// refused personality doesn't take the money. The reaction text itself is keyed off the same
// personality flag by the drawer, so the handlers only fire speech and mutate state.
static void mm6StreetBtbAction(DialogueId option, NPCData *npc) {
    NpcPersonality personality = pNPCStats->mm6PersonalityByProfession[npc->profession];
    auto setGreetState = [&](int state) {
        npc->flags = NpcFlags((std::to_underlying(npc->flags) & std::to_underlying(NPC_HIRED)) | state);
    };
    auto loseReputation = [&](int base) {
        currentLocationInfo().reputation -= std::max(0, base - mm6DiplomacyBonus(mm6Speaker()));
    };
    auto playSpeech = [&](SpeechId speech) {
        if (pParty->hasActiveCharacter())
            pParty->activeCharacter().playReaction(speech);
    };

    switch (option) {
      case DIALOGUE_STREET_MM6_BEG:
        if (mm6GreetState(npc) == 2) {
            // Begging worked once already - back to the row-3 brush-off greeting (MM6.EXE 0x4A3E26).
            static_cast<GUIWindow_Dialogue *>(pDialogueWindow.get())->setDisplayedDialogueType(DIALOGUE_MAIN);
            return;
        }
        if (pNPCStats->mm6PersonalityAcceptsBeg[personality]) {
            setGreetState(2);
            loseReputation(10);
            playSpeech(SPEECH_BEG);
        } else {
            playSpeech(SPEECH_BEG_FAIL);
        }
        return;

      case DIALOGUE_STREET_MM6_THREATEN:
        if (pNPCStats->mm6PersonalityAcceptsThreat[personality]) {
            setGreetState(4);
            loseReputation(50);
            playSpeech(SPEECH_THREAT);
        } else {
            playSpeech(SPEECH_THREAT_FAIL);
        }
        return;

      default: {
        assert(option == DIALOGUE_STREET_MM6_BRIBE);
        if (!pNPCStats->mm6PersonalityAcceptsBribe[personality]) {
            playSpeech(SPEECH_BRIBE_FAIL);
            return;
        }
        int cost = mm6BribeCost();
        if (pParty->GetGold() < cost) {
            engine->_statusBar->setEvent(LSTR_YOU_DONT_HAVE_ENOUGH_GOLD);
            playSpeech(SPEECH_NOT_ENOUGH_GOLD);
            static_cast<GUIWindow_Dialogue *>(pDialogueWindow.get())->setDisplayedDialogueType(DIALOGUE_MAIN);
            return;
        }
        pParty->TakeGold(cost);
        setGreetState(3);
        pParty->_mm6NpcBribeCount++;
        loseReputation(20);
        playSpeech(SPEECH_BRIBE);
        return;
      }
    }
}

void selectNPCDialogueOption(DialogueId option) {
    NPCData *speakingNPC = getNPCData(speakingNpcId);

    ((GUIWindow_Dialogue*)pDialogueWindow.get())->setDisplayedDialogueType(option);

    if (!speakingNPC->flags) {
        speakingNPC->flags = NPC_GREETED_FIRST;
    }

    if (option == DIALOGUE_STREET_MM6_BEG || option == DIALOGUE_STREET_MM6_THREATEN ||
        option == DIALOGUE_STREET_MM6_BRIBE) {
        mm6StreetBtbAction(option, speakingNPC);
        return;
    }

    if (option >= DIALOGUE_SCRIPTED_LINE_1 && option <= DIALOGUE_SCRIPTED_LINE_6) {
        DialogueId newTopic = handleScriptedNPCTopicSelection(option, speakingNPC);

        if (newTopic != DIALOGUE_MAIN) {
            std::vector<DialogueId> topics = listNPCDialogueOptions(newTopic);
            ((GUIWindow_Dialogue*)pDialogueWindow.get())->setDisplayedDialogueType(newTopic);
            pDialogueWindow->DeleteButtons();
            pBtn_ExitCancel = pDialogueWindow->CreateButton({471, 445}, {0xA9u, 0x23u}, BUTTON_TYPE_NORMAL, 0, UIMSG_Escape, 0, INPUT_ACTION_INVALID,
                                                            localization->str(LSTR_EXIT_DIALOGUE), {ui_exit_cancel_button_background});

            for (int i = 0; i < topics.size(); i++) {
                pDialogueWindow->CreateButton({480, 160 + i * 30}, {140, 30}, BUTTON_TYPE_NORMAL, 0, UIMSG_SelectNPCDialogueOption, std::to_underlying(topics[i]), INPUT_ACTION_INVALID, "");
            }
            pDialogueWindow->setKeyboardControlGroup(topics.size(), false, 0, 1);

            pDialogueWindow->CreateCharacterButtons();
        }
        return;
    }

    if (option == DIALOGUE_13_hiring_related) {
        if (!speakingNPC->Hired()) {
            BuildHireableNpcDialogue();
            dialogue_show_profession_details = false;
        } else {
            for (unsigned i = 0; i < (signed int)pNPCStats->uNumNewNPCs; ++i) {
                if (pNPCStats->pNPCData[i].Hired() && speakingNPC->name == pNPCStats->pNPCData[i].name)
                    pNPCStats->pNPCData[i].flags &= ~NPC_HIRED;
            }
            if (ascii::noCaseEquals(pParty->pHirelings[0].name, speakingNPC->name)) // TODO(captainurist): #unicode this is not ascii
                pParty->pHirelings[0] = NPCData();
            else if (ascii::noCaseEquals(pParty->pHirelings[1].name, speakingNPC->name)) // TODO(captainurist): #unicode this is not ascii
                pParty->pHirelings[1] = NPCData();
            pParty->hirelingScrollPosition = 0;
            pParty->CountHirelings();
            engine->_messageQueue->addMessageCurrentFrame(UIMSG_Escape, 1, 0);
        }
        return;
    }

    selectSpecialNPCTopicSelection(option, speakingNPC);

    if (option == DIALOGUE_HIRE_FIRE) {
        if (speakingNPC->Hired()) {
            if (currentSpeakingActor && currentSpeakingActor->npcId >= 0) {
                currentSpeakingActor->aiState = Removed;
            }
        }
    }
}

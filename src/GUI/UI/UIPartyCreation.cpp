#include <array>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>

#include "Engine/AssetsManager.h"
#include "Engine/Engine.h"
#include "Engine/EngineGlobals.h"
#include "Engine/Objects/CharacterEnumFunctions.h"
#include "Engine/Graphics/Renderer/Renderer.h"
#include "Engine/Graphics/Image.h"
#include "Engine/Graphics/Sprites.h"
#include "Engine/Localization.h"
#include "Engine/Party.h"
#include "Engine/Random/Random.h"
#include "Engine/Tables/ItemTable.h"
#include "Engine/Tables/IconFrameTable.h"
#include "Engine/Tables/NPCTable.h"
#include "Engine/TurnEngine/TurnEngine.h"
#include "Engine/Spells/SpellEnumFunctions.h"
#include "Engine/Time/Timer.h"

#include "GUI/GUIButton.h"
#include "GUI/GUIFont.h"
#include "GUI/GUIMessageQueue.h"
#include "GUI/UI/UIPartyCreation.h"

#include "Io/Mouse.h"
#include "Io/KeyboardInputHandler.h"

#include "Media/Audio/AudioPlayer.h"

#include "Library/Logger/Logger.h"

using Io::TextInputType;


GraphicsImage *ui_partycreation_top = nullptr;
GraphicsImage *ui_partycreation_sky_scroller = nullptr;

GraphicsImage *ui_partycreation_left = nullptr;
GraphicsImage *ui_partycreation_right = nullptr;
GraphicsImage *ui_partycreation_minus = nullptr;
GraphicsImage *ui_partycreation_plus = nullptr;
GraphicsImage *ui_partycreation_buttmake2 = nullptr;
GraphicsImage *ui_partycreation_buttmake = nullptr;

GraphicsImage *ui_partycreation_character_frame = nullptr;

std::array<GraphicsImage *, 9> ui_partycreation_class_icons;
std::array<GraphicsImage *, 22> ui_partycreation_portraits;

std::array<GraphicsImage *, 19> ui_partycreation_arrow_r;
std::array<GraphicsImage *, 19> ui_partycreation_arrow_l;


std::array<GUIButton*, 4> pCreationUI_BtnPressRight2;
std::array<GUIButton*, 4> pCreationUI_BtnPressLeft2;
std::array<GUIButton*, 4> pCreationUI_BtnPressLeft;
std::array<GUIButton*, 4> pCreationUI_BtnPressRight;

GUIButton* pPlayerCreationUI_BtnReset;
GUIButton* pPlayerCreationUI_BtnOK;
GUIButton* pPlayerCreationUI_BtnPlus;
GUIButton* pPlayerCreationUI_BtnMinus;



static Duration errorMessageExpireTime; // expiration time (misc timer) of error message

static const int ARROW_SPIN_PERIOD_MS = 475;

// --- MM6 party-creation skin, reversed from MM6.EXE (loader 0x451D00, draw 0x450DC0) -----------
static std::array<GraphicsImage *, 12> creationMm6Portraits = {{}};    // ccmalea..h + ccgirla..d stills; ids 0-7 male, 8-11 female.
static std::array<GraphicsImage *, 29> creationMm6FlamesLeft = {{}};   // fl1..fl29 - the left pillar torch.
static std::array<GraphicsImage *, 29> creationMm6FlamesRight = {{}};  // fr1..fr29 - the right pillar torch.
static int creationMm6FlameFramesetId = 0;                             // aframe1 sprite frameset - the flame below the selected portrait.
static std::array<bool, 4> creationMm6NameTyped = {{}};                // A typed name stops face changes from rerolling it (MM6.EXE 0x482CD0).
static constexpr std::array<int, 4> kMm6SelectedFlameX = {45, 204, 362, 521};  // MM6.EXE 0x451CC4 jump table.

bool PartyCreationUI_LoopInternal();
static void givePartyItemsMm6();

bool PlayerCreation_Choose4Skills() {
    for (const auto& character : pParty->pCharacters) {
        int skills_count = 0;
        for (Skill i : allVisibleSkills()) {
            if (character.pActiveSkills[i])
                ++skills_count;
        }

        if (skills_count != 4) return false;
    }

    return true;
}

void CreateParty_EventLoop() {
    auto pPlayer = pParty->pCharacters.data();
    while (engine->_messageQueue->haveMessages()) {
        UIMessageType msg;
        int param, param2;
        engine->_messageQueue->popMessage(&msg, &param, &param2);

        switch (msg) {
        case UIMSG_PlayerCreation_SelectAttribute:
        {
            pGUIWindow_CurrentMenu->pCurrentPosActiveItem =
                (pGUIWindow_CurrentMenu->pCurrentPosActiveItem -
                    pGUIWindow_CurrentMenu->pStartingPosActiveItem) %
                7 +
                pGUIWindow_CurrentMenu->pStartingPosActiveItem + 7 * param;
            uPlayerCreationUI_SelectedCharacter = param;
            pAudioPlayer->playUISound(SOUND_SelectingANewCharacter);
            break;
        }
        case UIMSG_PlayerCreation_VoicePrev:
        {
            Sex sex = pParty->pCharacters[param].GetSexByVoice();
            do {
                if (pParty->pCharacters[param].uVoiceID == 0)
                    pParty->pCharacters[param].uVoiceID = 19;
                else
                    --pParty->pCharacters[param].uVoiceID;
            } while (pParty->pCharacters[param].GetSexByVoice() != sex);
            auto pButton = pCreationUI_BtnPressLeft2[param];

            new OnButtonClick(pButton->rect.topLeft(), {0, 0}, pButton, std::string(), false);
            pAudioPlayer->playUISound(SOUND_SelectingANewCharacter);
            pAudioPlayer->stopVoiceSounds();
            pParty->pCharacters[param].playReaction(SPEECH_PICK_ME);
            break;
        }
        case UIMSG_PlayerCreation_VoiceNext:
        {
            Sex sex = pParty->pCharacters[param].GetSexByVoice();
            do {
                pParty->pCharacters[param].uVoiceID =
                    (pParty->pCharacters[param].uVoiceID + 1) % 20;
            } while (pParty->pCharacters[param].GetSexByVoice() != sex);
            auto pButton = pCreationUI_BtnPressRight2[param];
            new OnButtonClick(pButton->rect.topLeft(), {0, 0}, pButton, std::string(), false);
            pAudioPlayer->playUISound(SOUND_SelectingANewCharacter);
            pAudioPlayer->stopVoiceSounds();
            pParty->pCharacters[param].playReaction(SPEECH_PICK_ME);
            break;
        }
        case UIMSG_PlayerCreation_FacePrev:
        case UIMSG_PlayerCreation_FaceNext: {
            Character &character = pParty->pCharacters[param];
            if (engine->gameVersion() == GAME_VERSION_MM6) {
                // MM6 cycles 12 stills; the face fixes voice and sex, and rerolls the name
                // from npcnames.txt unless one was typed (MM6.EXE 0x43048D/0x482CD0). Stats
                // are untouched - MM6's point buy anchors on the class, not the portrait.
                character.uCurrentFace = (character.uCurrentFace + (msg == UIMSG_PlayerCreation_FaceNext ? 1 : 11)) % 12;
                character.uVoiceID = character.uCurrentFace;
                character.uSex = character.uCurrentFace >= 8 ? SEX_FEMALE : SEX_MALE;
                if (!creationMm6NameTyped[param])
                    character.name = grng->randomSample(pNPCStats->pNPCNames[character.uSex]);
            } else {
                if (msg == UIMSG_PlayerCreation_FaceNext)
                    character.uCurrentFace = (character.uCurrentFace + 1) % 20;
                else
                    character.uCurrentFace = character.uCurrentFace ? character.uCurrentFace - 1 : 19;
                character.uVoiceID = character.uCurrentFace;
                character.SetInitialStats();
                character.SetSexByVoice();
                character.RandomizeName();
            }
            pGUIWindow_CurrentMenu->pCurrentPosActiveItem =
                (pGUIWindow_CurrentMenu->pCurrentPosActiveItem -
                    pGUIWindow_CurrentMenu->pStartingPosActiveItem) %
                7 +
                pGUIWindow_CurrentMenu->pStartingPosActiveItem + 7 * param;
            uPlayerCreationUI_SelectedCharacter = param;
            if (engine->gameVersion() != GAME_VERSION_MM6) {
                // MM6's face arrows are baked into the background and don't flash.
                GUIButton *arrowButton = msg == UIMSG_PlayerCreation_FaceNext
                    ? pCreationUI_BtnPressRight[param] : pCreationUI_BtnPressLeft[param];
                new OnButtonClick(arrowButton->rect.topLeft(), {0, 0}, arrowButton, std::string(), false);
            }
            pAudioPlayer->playUISound(SOUND_SelectingANewCharacter);
            pAudioPlayer->stopVoiceSounds();
            character.playReaction(SPEECH_PICK_ME);
            break;
        }
        case UIMSG_PlayerCreationClickPlus:
            // The pressed image flashes inside the socket painted into makeme.pcx for MM6
            // (MM6.EXE 0x42FF65/0x42FFD3 use these exact spots).
            new OnButtonClick(engine->gameVersion() == GAME_VERSION_MM6 ? Pointi(588, 405) : Pointi(613, 393),
                {0, 0}, pPlayerCreationUI_BtnPlus, std::string(), false);
            pPlayer[uPlayerCreationUI_SelectedCharacter].IncreaseAttribute(
                static_cast<Attribute>((pGUIWindow_CurrentMenu->pCurrentPosActiveItem - pGUIWindow_CurrentMenu->pStartingPosActiveItem) % 7));
            pAudioPlayer->playUISound(SOUND_ClickMinus);
            break;
        case UIMSG_PlayerCreationClickMinus:
            new OnButtonClick(engine->gameVersion() == GAME_VERSION_MM6 ? Pointi(485, 408) : Pointi(523, 393),
                {0, 0}, pPlayerCreationUI_BtnMinus, std::string(), false);
            pPlayer[uPlayerCreationUI_SelectedCharacter].DecreaseAttribute(
                static_cast<Attribute>((pGUIWindow_CurrentMenu->pCurrentPosActiveItem - pGUIWindow_CurrentMenu->pStartingPosActiveItem) % 7));
            pAudioPlayer->playUISound(SOUND_ClickPlus);
            break;
        case UIMSG_PlayerCreationSelectActiveSkill:
            if (pPlayer[uPlayerCreationUI_SelectedCharacter].GetSkillIdxByOrder(3) == SKILL_INVALID)
                pParty->pCharacters[uPlayerCreationUI_SelectedCharacter].pActiveSkills[pPlayer[uPlayerCreationUI_SelectedCharacter]
                    .GetSkillIdxByOrder(param + 4)] = CombinedSkillValue::novice();
            pAudioPlayer->playUISound(SOUND_ClickSkill);
            break;
        case UIMSG_PlayerCreationSelectClass:
            pPlayer[uPlayerCreationUI_SelectedCharacter].ChangeClass((Class)param);
            pAudioPlayer->playUISound(SOUND_SelectingANewCharacter);
            break;
        case UIMSG_PlayerCreationClickOK:
            new OnButtonClick(engine->gameVersion() == GAME_VERSION_MM6 ? Pointi(511, 439) : Pointi(580, 431),
                {0, 0}, pPlayerCreationUI_BtnOK);
            // Both games gate OK on a fully spent bonus pool and four skills per character
            // (MM6.EXE 0x42FF14; the version-aware point-buy math lives in Character.cpp).
            if (CharacterCreation_GetUnspentAttributePointCount() || !PlayerCreation_Choose4Skills()) {
                errorMessageExpireTime = pMiscTimer->time() + Duration::fromRealtimeSeconds(4); // show message for 4 seconds
            } else {
                uGameState = GAME_STATE_STARTING_NEW_GAME;
            }
            break;
        case UIMSG_PlayerCreationClickReset:
            new OnButtonClick({527, 431}, {0, 0}, pPlayerCreationUI_BtnReset);
            pParty->Reset();
            break;
        case UIMSG_PlayerCreationRemoveUpSkill:
        {
            int v4;
            v4 = pGUIWindow_CurrentMenu->pCurrentPosActiveItem - pGUIWindow_CurrentMenu->pStartingPosActiveItem;
            pGUIWindow_CurrentMenu->pCurrentPosActiveItem = v4 % 7 + pGUIWindow_CurrentMenu->pStartingPosActiveItem + 7 * param;
            if (pPlayer[param].GetSkillIdxByOrder(2) != SKILL_INVALID) {
                pParty->pCharacters[param].pActiveSkills[pPlayer[param].GetSkillIdxByOrder(2)] = CombinedSkillValue::none();
            }
            break;
        }
        case UIMSG_PlayerCreationRemoveDownSkill:
        {
            int v4;
            v4 = pGUIWindow_CurrentMenu->pCurrentPosActiveItem - pGUIWindow_CurrentMenu->pStartingPosActiveItem;
            pGUIWindow_CurrentMenu->pCurrentPosActiveItem = v4 % 7 + pGUIWindow_CurrentMenu->pStartingPosActiveItem + 7 * param;
            if (pPlayer[param].GetSkillIdxByOrder(3) != SKILL_INVALID)
                pParty->pCharacters[param].pActiveSkills[pPlayer[param].GetSkillIdxByOrder(3)] = CombinedSkillValue::none();
        } break;
        case UIMSG_PlayerCreationChangeName:
            pAudioPlayer->playUISound(SOUND_ClickSkill);
            uPlayerCreationUI_NameEditCharacter = param;
            keyboardInputHandler->StartTextInput(TextInputType::Text, 15, pGUIWindow_CurrentMenu.get());
            break;
        case UIMSG_Escape:
            if (!(dword_6BE364_game_settings_1 & GAME_SETTINGS_4000)) break;
            if (GetCurrentMenuID() == MENU_MAIN ||
                GetCurrentMenuID() == MENU_MMT_MAIN_MENU ||
                GetCurrentMenuID() == MENU_CREATEPARTY ||
                GetCurrentMenuID() == MENU_NAMEPANELESC) {
                // if ( current_screen_type == SCREEN_VIDEO )
                // pVideoPlayer->FastForwardToFrame(pVideoPlayer->pResetflag);
                engine->_messageQueue->addMessageCurrentFrame(UIMSG_ChangeGameState,
                    0, 0);
            }
            break;
        case UIMSG_ChangeGameState:
            uGameState = GAME_FINISHED;
            break;
        default:
            break;
        }
    }
}

// Everything a new game needs before the party exists: the default party, and the engine state it
// starts from - music stopped, event timer paused, turn-based combat ended, NPC tables restored.
// Shared by the creation screen and by MM6's Quick Start, which skips that screen.
static void resetForNewGame() {
    pAudioPlayer->MusicStop();
    pEventTimer->setPaused(true);

    // This call is here b/c otherwise Character::timeToRecovery will be overwritten in the main loop from the
    // turn-based queue if we're currently in turn-based combat.
    pTurnEngine->End(false);

    pParty->Reset();
    pParty->createDefaultParty();

    pNPCStats->pNPCData = pNPCStats->pOriginalNPCData;
    pNPCStats->pGroups = pNPCStats->pOriginalGroups;
    if (engine->gameVersion() != GAME_VERSION_MM6)
        pNPCStats->pNPCData[3].flags |= NPC_HIRED; // Lady Margaret. MM6 npc 3 is an unrelated quest NPC.
}

bool PartyCreationUI_Loop() {
    resetForNewGame();

    pGUIWindow_CurrentMenu = std::make_unique<GUIWindow_PartyCreation>();
    return !PartyCreationUI_LoopInternal();
}

// What MM6.EXE does: the Quick Start handler (@0x42fe0e) sets screen id 10, and its caller
// (@0x453664) fills the four characters in from global.txt rows 506-509 - it does not run the
// creation screen at all.
//
// What we do instead: the same default party the creation screen would open with
// (Party::createDefaultParty -> resetCharactersMm6, itself taken from new.lod's party.bin), plus the
// skill-derived starting inventory that leaving the creation screen grants (givePartyItemsMm6).
//
// Why those agree: new.lod's template party IS MM6's default party, and - as givePartyItemsMm6()'s
// own comment records - the template's gear is exactly that grant's output for the default skill
// sets. So the two constructions land on the same party from either end.
//
// Note there is no stopSounds() here, unlike the creation screen's MM6 tail (which has one to kill
// the sounds the screen itself made): the only sound in flight on this path is the Quick Start
// button's own click, and stopping it here would silence the button.
void mm6QuickStartParty() {
    resetForNewGame();

    givePartyItemsMm6();
}

// The MM6 creation draw, layout verbatim from MM6.EXE 0x450DC0. Character columns: portrait
// stills at (17/176/334/493, 35) with an animated aframe1 flame below the selected one, class
// icon and BLACK class name / character name on the light marble plates, stats from y=160 and
// the four skill slots from y=308 on the green marble. Bottom row: class list at x=60/140,
// the nine pickable skills from x=230, and the bonus pool at the right.
void GUIWindow_PartyCreation::updateMm6() {
    render->BeginScene2D();

    // The scrolling sky peeks through the MAKETOP band; makeme.pcx is the body below it.
    int skyScrollX = static_cast<int>(std::fmod(pMiscTimer->time().realtimeMillisecondsFloat() * 640.0 / 20, 640.0));
    render->DrawQuad2D(ui_partycreation_sky_scroller, {skyScrollX, 2});
    render->DrawQuad2D(ui_partycreation_sky_scroller, {skyScrollX - 640, 2});
    render->DrawQuad2D(ui_partycreation_top, {0, 0});
    render->DrawQuad2D(main_menu_background, {0, 23});

    uPlayerCreationUI_SelectedCharacter = (pCurrentPosActiveItem - pStartingPosActiveItem) / 7;

    int pTextCenter = ui_partycreation_font->AlignText_Center(640, localization->str(LSTR_C_R_E_A_T_E_P_A_R_T_Y));
    DrawText(ui_partycreation_font.get(), {pTextCenter + 1, 0}, colorTable.White, localization->str(LSTR_C_R_E_A_T_E_P_A_R_T_Y), frameRect);

    static constexpr std::array<int, 4> kPortraitX = {17, 176, 334, 493};
    for (int i = 0; i < 4; i++)
        render->DrawQuad2D(creationMm6Portraits[pParty->pCharacters[i].uCurrentFace], {kPortraitX[i], 35});

    // The flame burning below the selected character's portrait - an animated sprite frameset,
    // anchored bottom-center like the screen overlays (MM6.EXE 0x450FF9-0x4510A0).
    if (creationMm6FlameFramesetId > 0) {
        Duration animTime = Duration::fromTicks(pMiscTimer->time().realtimeMilliseconds() * 128 / 1000);
        SpriteFrame *frame = pSpriteFrameTable->GetFrame(creationMm6FlameFramesetId, animTime);
        if (frame && frame->sprites[0] && frame->sprites[0]->texture) {
            Sprite *sprite = frame->sprites[0];
            render->DrawImage(sprite->texture,
                              Recti(kMm6SelectedFlameX[uPlayerCreationUI_SelectedCharacter] - sprite->uWidth / 2,
                                    119 - sprite->uHeight, sprite->uWidth, sprite->uHeight),
                              frame->paletteId);
        }
    }

    // Spinning arrows flank the keyboard-focused control (left arrow at x-14, right at x+w-5).
    GUIButton *activeControl = GetControl(pCurrentPosActiveItem);
    int arrowAnimTextureNum = ui_partycreation_arrow_l.size() - 1 - (pMiscTimer->time().realtimeMilliseconds() % ARROW_SPIN_PERIOD_MS) / (ARROW_SPIN_PERIOD_MS / ui_partycreation_arrow_l.size());
    render->DrawQuad2D(ui_partycreation_arrow_l[arrowAnimTextureNum], {activeControl->rect.x - 14, activeControl->rect.y});
    render->DrawQuad2D(ui_partycreation_arrow_r[arrowAnimTextureNum], {activeControl->rect.x + activeControl->rect.w - 5, activeControl->rect.y});

    // The pillar torches at both screen edges.
    int flameFrame = (pMiscTimer->time().realtimeMilliseconds() / 55) % creationMm6FlamesLeft.size();
    render->DrawQuad2D(creationMm6FlamesLeft[flameFrame], {5, 379});
    render->DrawQuad2D(creationMm6FlamesRight[flameFrame], {600, 379});

    std::string skillsLabel = localization->str(LSTR_SKILLS);
    for (int i = skillsLabel.size() - 1; i >= 0; i--)
        skillsLabel[i] = toupper(skillsLabel[i]); // TODO(captainurist): #unicode this won't work with a Russian localization.

    int v0 = assets->pFontCreate->GetHeight() - 2;
    for (int i = 0; i < 4; ++i) {
        Character &character = pParty->pCharacters[i];
        int columnX = 158 * i;

        // Class icon and class name on the marble plate right of the portrait. MM6.EXE passes
        // color 0 here, which is "draw with the font's own palette" (FONT.CPP 0x44386b picks the
        // paletted glyph blit 0x40aa60), and FONTPAL holds white letters over a black shadow.
        if (GraphicsImage *classIcon = ui_partycreation_class_icons[std::to_underlying(character.classType) / 4])
            render->DrawQuad2D(classIcon, {95 + 159 * i, 50});
        DrawText(assets->pFontCreate.get(), {85 + 159 * i, 97}, colorTable.White, localization->className(character.classType), frameRect);

        // Character name on the plate strip, editable in place. Also color 0 in MM6.EXE - white.
        if (keyboard_input_status != WINDOW_INPUT_NONE && uPlayerCreationUI_NameEditCharacter == i) {
            switch (keyboard_input_status) {
            case WINDOW_INPUT_IN_PROGRESS: {
                int cursorX = DrawTextInRect(assets->pFontCreate.get(), {159 * i + 18, 124}, colorTable.White, keyboardInputHandler->GetTextInput(), 120, 1);
                DrawFlashingInputCursor(159 * i + cursorX + 20, 124, assets->pFontCreate.get(), frameRect);
                break;
            }
            case WINDOW_INPUT_CONFIRMED: {
                keyboard_input_status = WINDOW_INPUT_NONE;
                int spaces = 0;
                for (char c : keyboardInputHandler->GetTextInput())
                    spaces += c == ' ';
                if (keyboardInputHandler->GetTextInput().size() > 0 && spaces != keyboardInputHandler->GetTextInput().size()) {
                    character.name = keyboardInputHandler->GetTextInput();
                    creationMm6NameTyped[i] = true; // Face changes no longer reroll it.
                }
                DrawTextInRect(assets->pFontCreate.get(), {159 * i + 18, 124}, colorTable.White, character.name, 130, 0);
                break;
            }
            default:
                break;
            }
        } else {
            DrawTextInRect(assets->pFontCreate.get(), {159 * i + 18, 124}, colorTable.White, character.name, 130, 0);
        }

        // Seven stat rows from y=160 (MM7 uses 169), numbers right-aligned at the \r stop.
        int statNameX = 32 + columnX;
        int statNumbersX = 493 - columnX;
        static constexpr std::array<LstrId, 7> kStatLabels = {
            LSTR_MIGHT, LSTR_INTELLECT, LSTR_PERSONALITY, LSTR_ENDURANCE, LSTR_ACCURACY, LSTR_SPEED, LSTR_LUCK};
        for (int stat = 0; stat < 7; stat++) {
            Attribute attribute = static_cast<Attribute>(stat);
            int actualValue = 0;
            switch (attribute) {
            case ATTRIBUTE_MIGHT:        actualValue = character.GetActualMight(); break;
            case ATTRIBUTE_INTELLIGENCE: actualValue = character.GetActualIntelligence(); break;
            case ATTRIBUTE_PERSONALITY:  actualValue = character.GetActualPersonality(); break;
            case ATTRIBUTE_ENDURANCE:    actualValue = character.GetActualEndurance(); break;
            case ATTRIBUTE_ACCURACY:     actualValue = character.GetActualAccuracy(); break;
            case ATTRIBUTE_SPEED:        actualValue = character.GetActualSpeed(); break;
            case ATTRIBUTE_LUCK:         actualValue = character.GetActualLuck(); break;
            default: break;
            }
            std::string statLine = fmt::format("{}\r{:03}{}", localization->str(kStatLabels[stat]), statNumbersX, actualValue);
            DrawText(assets->pFontCreate.get(), {statNameX, 160 + v0 * stat}, character.GetStatColor(attribute), statLine, frameRect);
        }

        // The per-column SKILLS header and the four skill slots (two fixed, two picked).
        pTextCenter = assets->pFontCreate->AlignText_Center(150, skillsLabel);
        DrawText(assets->pFontCreate.get(), {pTextCenter + statNameX - 24, 289}, colorTable.Tacha, skillsLabel, frameRect);
        for (int slot = 0; slot < 4; slot++) {
            Skill skill = character.GetSkillIdxByOrder(slot);
            std::string skillName = localization->skillName(skill);
            pTextCenter = assets->pFontCreate->AlignText_Center(150, skillName);
            Color slotColor = colorTable.White;
            if (slot >= 2)
                slotColor = skill == SKILL_INVALID ? colorTable.Aqua : colorTable.Green;
            DrawText(assets->pFontCreate.get(), {statNameX - 24, 308 + v0 * slot}, slotColor,
                     fmt::format("\t{:03}{}", pTextCenter, skillName), frameRect);
        }
    }

    // Bottom-left: the class list (label centered over 193px from x=37, names over 70px columns).
    std::string classLabel = localization->str(LSTR_CLASS);
    for (int i = classLabel.size() - 1; i >= 0; i--)
        classLabel[i] = toupper(classLabel[i]); // TODO(captainurist): #unicode this won't work for Russian localization.
    Class selectedClass = pParty->pCharacters[uPlayerCreationUI_SelectedCharacter].classType;
    pTextCenter = assets->pFontCreate->AlignText_Center(193, classLabel);
    DrawText(assets->pFontCreate.get(), {pTextCenter + 37, 398}, colorTable.Tacha, classLabel, frameRect);
    static constexpr std::array<std::pair<Class, Pointi>, 6> kClassListSlots = {{
        {CLASS_KNIGHT, {60, 0}}, {CLASS_CLERIC, {60, 1}}, {CLASS_SORCERER, {60, 2}},
        {CLASS_PALADIN, {140, 0}}, {CLASS_ARCHER, {140, 1}}, {CLASS_DRUID, {140, 2}},
    }};
    for (const auto &[classType, slot] : kClassListSlots) {
        Color classColor = classType == selectedClass ? colorTable.Aqua : colorTable.White;
        pTextCenter = assets->pFontCreate->AlignText_Center(70, localization->className(classType));
        DrawText(assets->pFontCreate.get(), {pTextCenter + slot.x, 417 + v0 * slot.y}, classColor, localization->className(classType), frameRect);
    }

    // Bottom-center: the nine skills the selected character may pick from, first word only
    // ("Body Building" draws as "Body" - MM6.EXE truncates at the first space).
    pTextCenter = assets->pFontCreate->AlignText_Center(236, localization->str(LSTR_AVAILABLE_SKILLS));
    DrawText(assets->pFontCreate.get(), {pTextCenter + 238, 398}, colorTable.Tacha, localization->str(LSTR_AVAILABLE_SKILLS), frameRect);
    for (int i = 0; i < 9; ++i) {
        Skill skill = pParty->pCharacters[uPlayerCreationUI_SelectedCharacter].GetSkillIdxByOrder(i + 4);
        std::string skillName = localization->skillName(skill);
        if (size_t space = skillName.find(' '); space != std::string::npos)
            skillName.resize(space);
        Color skillColor = pParty->pCharacters[uPlayerCreationUI_SelectedCharacter].pActiveSkills[skill]
            ? colorTable.Aqua : colorTable.White;
        pTextCenter = assets->pFontCreate->AlignText_Center(80, skillName);
        DrawText(assets->pFontCreate.get(), {230 + 80 * (i / 3) + pTextCenter, 417 + v0 * (i % 3)}, skillColor, skillName, frameRect);
    }

    // Bottom-right: the bonus-point pool.
    pTextCenter = assets->pFontCreate->AlignText_Center(92, localization->str(LSTR_BONUS_1));
    DrawText(assets->pFontCreate.get(), {pTextCenter + 498, 394}, colorTable.Tacha, localization->str(LSTR_BONUS_1), frameRect);

    // force draw so overlays dont get muddled
    render->DrawTwodVerts();
    render->EndTextNew();

    int pBonusNum = CharacterCreation_GetUnspentAttributePointCount();
    std::string bonusLabel = fmt::format("{}", pBonusNum);
    pTextCenter = assets->pFontCreate->AlignText_Center(84, bonusLabel);
    DrawText(assets->pFontCreate.get(), {pTextCenter + 502, 410}, colorTable.White, bonusLabel, frameRect);

    if (errorMessageExpireTime > pMiscTimer->time()) {
        auto &sHint = pBonusNum < 0 ? localization->str(LSTR_YOU_CANT_SPEND_MORE_THAN_50_POINTS) : localization->str(LSTR_CREATE_PARTY_CANNOT_BE_COMPLETED_UNLESS);
        Recti popupRect(170, 140, 300, 100);
        DrawMessageBox(0, popupRect, sHint);
    }

    // force draw so overlays dont get muddled
    render->DrawTwodVerts();
    render->EndTextNew();
}

//----- (00495B39) --------------------------------------------------------
// void PlayerCreationUI_Draw()
void GUIWindow_PartyCreation::Update() {
    if (engine->gameVersion() == GAME_VERSION_MM6) {
        updateMm6();
        return;
    }

    int pTextCenter;                // eax@3
    int pX;                         // ecx@7
    GUIButton *uPosActiveItem;      // edi@12
    int v17;                        // eax@33
    Color pStatColor;        // eax@44
    Skill pSkillsType;  // eax@44
    Class uClassType;   // edi@53
    Color pColorText;                 // eax@53
    Skill pSkillId;     // edi@72
    size_t pLenText;                // eax@72
    signed int v104;                // ecx@72
    signed int pBonusNum;           // edi@82
    std::string pText;                // [sp+10h] [bp-160h]@14
    int v126;                       // [sp+148h] [bp-28h]@25
    int pIntervalY;                 // [sp+150h] [bp-20h]@14
    int pX_Numbers;                 // [sp+154h] [bp-1Ch]@18
    int uX;                         // [sp+160h] [bp-10h]@18
    int pIntervalX;
    int pCorrective;

    // move sky
    render->BeginScene2D();
    render->DrawQuad2D(main_menu_background, {0, 0});
    int sky_slider_anim_timer = static_cast<int>(std::fmod(pMiscTimer->time().realtimeMillisecondsFloat() * 640.0 / 20, 640.0));
    render->DrawQuad2D(ui_partycreation_sky_scroller, {sky_slider_anim_timer, 2});
    render->DrawQuad2D(ui_partycreation_sky_scroller, {sky_slider_anim_timer - 640, 2});
    render->DrawQuad2D(ui_partycreation_top, {0, 0});

    uPlayerCreationUI_SelectedCharacter = (pGUIWindow_CurrentMenu->pCurrentPosActiveItem - pGUIWindow_CurrentMenu->pStartingPosActiveItem) / 7;
    switch (uPlayerCreationUI_SelectedCharacter) {
        case 0:
            pX = 12;
            break;
        case 1:
            pX = 171;
            break;
        case 2:
            pX = 329;
            break;
        case 3:
            pX = 488;
            break;
        default:
            assert(false);
            pX = 0;
            break;
    }

    pTextCenter = ui_partycreation_font->AlignText_Center(
        640, localization->str(LSTR_C_R_E_A_T_E_P_A_R_T_Y));
    DrawText(ui_partycreation_font.get(), {pTextCenter + 1, 0}, colorTable.White,
        localization->str(LSTR_C_R_E_A_T_E_P_A_R_T_Y), pGUIWindow_CurrentMenu->frameRect);

    render->DrawQuad2D(ui_partycreation_portraits[pParty->pCharacters[0].uCurrentFace], {17, 35});
    render->DrawQuad2D(ui_partycreation_portraits[pParty->pCharacters[1].uCurrentFace], {176, 35});
    render->DrawQuad2D(ui_partycreation_portraits[pParty->pCharacters[2].uCurrentFace], {335, 35});
    render->DrawQuad2D(ui_partycreation_portraits[pParty->pCharacters[3].uCurrentFace], {494, 35});

    // arrows
    render->DrawQuad2D(ui_partycreation_character_frame, {pX, 29});
    uPosActiveItem = pGUIWindow_CurrentMenu->GetControl(pGUIWindow_CurrentMenu->pCurrentPosActiveItem);
    // cycle arrows backwards
    int arrowAnimTextureNum = ui_partycreation_arrow_l.size() - 1 - (pMiscTimer->time().realtimeMilliseconds() % ARROW_SPIN_PERIOD_MS) / (ARROW_SPIN_PERIOD_MS / ui_partycreation_arrow_l.size());
    render->DrawQuad2D(ui_partycreation_arrow_l[arrowAnimTextureNum], {uPosActiveItem->rect.x + uPosActiveItem->rect.w - 4, uPosActiveItem->rect.y});
    render->DrawQuad2D(ui_partycreation_arrow_r[arrowAnimTextureNum], {uPosActiveItem->rect.x - 12, uPosActiveItem->rect.y});

    pText = localization->str(LSTR_SKILLS);
    for (int i = pText.size() - 1; i >= 0; i--)
        pText[i] = toupper(pText[i]); // TODO(captainurist): #unicode this won't work with a Russian localization.

    pIntervalX = 18;
    pIntervalY = assets->pFontCreate->GetHeight() - 2;
    uX = 32;
    pX_Numbers = 640 - 147;  // 493;

    for (int i = 0; i < 4; ++i) {
        DrawText(assets->pFontCreate.get(), {pIntervalX + 73, 100}, colorTable.White,
            localization->className(pParty->pCharacters[i].classType), pGUIWindow_CurrentMenu->frameRect);
        render->DrawQuad2D(ui_partycreation_class_icons[std::to_underlying(pParty->pCharacters[i].classType) / 4], {pIntervalX + 77, 50});

        if (pGUIWindow_CurrentMenu->keyboard_input_status != WINDOW_INPUT_NONE && uPlayerCreationUI_NameEditCharacter == i) {
            switch (pGUIWindow_CurrentMenu->keyboard_input_status) {
            case WINDOW_INPUT_IN_PROGRESS:  // press name panel
                v17 = pGUIWindow_CurrentMenu->DrawTextInRect(assets->pFontCreate.get(), {159 * uPlayerCreationUI_NameEditCharacter + 18, 124}, colorTable.White,
                    keyboardInputHandler->GetTextInput(), 120, 1);
                DrawFlashingInputCursor(159 * uPlayerCreationUI_NameEditCharacter + v17 + 20, 124, assets->pFontCreate.get(), pGUIWindow_CurrentMenu->frameRect);
                break;
            case WINDOW_INPUT_CONFIRMED:  // press enter
                pGUIWindow_CurrentMenu->keyboard_input_status = WINDOW_INPUT_NONE;
                v126 = 0;
                for (int j = 0; j < keyboardInputHandler->GetTextInput().size(); ++j) {  // edit name
                    if (keyboardInputHandler->GetTextInput()[j] == ' ')
                        ++v126;
                }
                if (keyboardInputHandler->GetTextInput().size() > 0 && v126 != keyboardInputHandler->GetTextInput().size())
                    pParty->pCharacters[i].name = keyboardInputHandler->GetTextInput();
                pGUIWindow_CurrentMenu->DrawTextInRect(assets->pFontCreate.get(), {pIntervalX, 124}, colorTable.White, pParty->pCharacters[i].name, 130, 0);
                break;
            default:
                break;
            }
        } else {
            pGUIWindow_CurrentMenu->DrawTextInRect(assets->pFontCreate.get(), {pIntervalX, 124}, colorTable.White, pParty->pCharacters[i].name, 130, 0);
        }

        std::string pRaceName = pParty->pCharacters[i].GetRaceName();
        pGUIWindow_CurrentMenu->DrawTextInRect(assets->pFontCreate.get(), {pIntervalX + 72, pIntervalY + 12}, colorTable.White, pRaceName, 130, 0);

        pTextCenter = assets->pFontCreate->AlignText_Center(150, pText);
        DrawText(assets->pFontCreate.get(), {pTextCenter + uX - 24, 291}, colorTable.Tacha, pText, pGUIWindow_CurrentMenu->frameRect);  // Skills

        int posY = 169;

        auto str1 = fmt::format("{}\r{:03}{}", localization->str(LSTR_MIGHT), pX_Numbers, pParty->pCharacters[i].GetActualMight());
        pStatColor = pParty->pCharacters[i].GetStatColor(ATTRIBUTE_MIGHT);
        DrawText(assets->pFontCreate.get(), {uX, posY}, pStatColor, str1, pGUIWindow_CurrentMenu->frameRect);

        auto str2 = fmt::format("{}\r{:03}{}", localization->str(LSTR_INTELLECT), pX_Numbers, pParty->pCharacters[i].GetActualIntelligence());
        pStatColor = pParty->pCharacters[i].GetStatColor(ATTRIBUTE_INTELLIGENCE);
        DrawText(assets->pFontCreate.get(), {uX, pIntervalY + posY}, pStatColor, str2, pGUIWindow_CurrentMenu->frameRect);
        auto str3 = fmt::format("{}\r{:03}{}", localization->str(LSTR_PERSONALITY), pX_Numbers, pParty->pCharacters[i].GetActualPersonality());
        pStatColor = pParty->pCharacters[i].GetStatColor(ATTRIBUTE_PERSONALITY);
        DrawText(assets->pFontCreate.get(), {uX, 2 * pIntervalY + posY}, pStatColor, str3, pGUIWindow_CurrentMenu->frameRect);

        auto str4 = fmt::format("{}\r{:03}{}", localization->str(LSTR_ENDURANCE), pX_Numbers, pParty->pCharacters[i].GetActualEndurance());
        pStatColor = pParty->pCharacters[i].GetStatColor(ATTRIBUTE_ENDURANCE);
        DrawText(assets->pFontCreate.get(), {uX, 3 * pIntervalY + posY}, pStatColor, str4, pGUIWindow_CurrentMenu->frameRect);
        auto str5 = fmt::format("{}\r{:03}{}", localization->str(LSTR_ACCURACY), pX_Numbers, pParty->pCharacters[i].GetActualAccuracy());
        pStatColor = pParty->pCharacters[i].GetStatColor(ATTRIBUTE_ACCURACY);
        DrawText(assets->pFontCreate.get(), {uX, 4 * pIntervalY + posY}, pStatColor, str5, pGUIWindow_CurrentMenu->frameRect);

        auto str6 = fmt::format("{}\r{:03}{}", localization->str(LSTR_SPEED), pX_Numbers, pParty->pCharacters[i].GetActualSpeed());
        pStatColor = pParty->pCharacters[i].GetStatColor(ATTRIBUTE_SPEED);
        DrawText(assets->pFontCreate.get(), {uX, 5 * pIntervalY + posY}, pStatColor, str6, pGUIWindow_CurrentMenu->frameRect);
        auto str7 = fmt::format("{}\r{:03}{}", localization->str(LSTR_LUCK), pX_Numbers, pParty->pCharacters[i].GetActualLuck());
        pStatColor = pParty->pCharacters[i].GetStatColor(ATTRIBUTE_LUCK);
        DrawText(assets->pFontCreate.get(), {uX, 6 * pIntervalY + posY}, pStatColor, str7, pGUIWindow_CurrentMenu->frameRect);

        posY = 311;

        pSkillsType = pParty->pCharacters[i].GetSkillIdxByOrder(0);
        pTextCenter = assets->pFontCreate->AlignText_Center(150, localization->skillName(pSkillsType));
        auto str8 = fmt::format("\t{:03}{}", pTextCenter, localization->skillName(pSkillsType));
        DrawText(assets->pFontCreate.get(), {uX - 24, posY}, colorTable.White, str8, pGUIWindow_CurrentMenu->frameRect);

        pSkillsType = pParty->pCharacters[i].GetSkillIdxByOrder(1);
        pTextCenter = assets->pFontCreate->AlignText_Center(150, localization->skillName(pSkillsType));
        auto str9 = fmt::format("\t{:03}{}", pTextCenter, localization->skillName(pSkillsType));
        DrawText(assets->pFontCreate.get(), {uX - 24, pIntervalY + posY}, colorTable.White, str9, pGUIWindow_CurrentMenu->frameRect);

        pSkillsType = pParty->pCharacters[i].GetSkillIdxByOrder(2);
        pTextCenter = assets->pFontCreate->AlignText_Center(150, localization->skillName(pSkillsType));
        auto str10 = fmt::format("\t{:03}{}", pTextCenter, localization->skillName(pSkillsType));
        pColorText = colorTable.Green;
        if (pSkillsType == SKILL_INVALID)
            pColorText = colorTable.Aqua;
        DrawText(assets->pFontCreate.get(), {uX - 24, 2 * pIntervalY + posY}, pColorText, str10, pGUIWindow_CurrentMenu->frameRect);

        pSkillsType = pParty->pCharacters[i].GetSkillIdxByOrder(3);
        pTextCenter = assets->pFontCreate->AlignText_Center(150, localization->skillName(pSkillsType));
        auto str11 = fmt::format("\t{:03}{}", pTextCenter, localization->skillName(pSkillsType));
        pColorText = colorTable.Green;
        if (pSkillsType == SKILL_INVALID)
            pColorText = colorTable.Aqua;
        DrawText(assets->pFontCreate.get(), {uX - 24, 3 * pIntervalY + posY}, pColorText, str11, pGUIWindow_CurrentMenu->frameRect);

        pIntervalX += 159;
        pX_Numbers -= 158;
        uX += 158;
    }

    pText = localization->str(LSTR_CLASS);
    for (int i = pText.size() - 1; i >= 0; i--)
        pText[i] = toupper(pText[i]); // TODO(captainurist): #unicode this won't work for Russian localization.

    uClassType = pParty->pCharacters[uPlayerCreationUI_SelectedCharacter].classType;
    pTextCenter = assets->pFontCreate->AlignText_Center(193, pText);
    DrawText(assets->pFontCreate.get(), {pTextCenter + 324, 395}, colorTable.Tacha, pText, pGUIWindow_CurrentMenu->frameRect);  // Classes

    pColorText = colorTable.Aqua;
    if (uClassType != CLASS_KNIGHT)
        pColorText = colorTable.White;
    pTextCenter = assets->pFontCreate->AlignText_Center(65, localization->className(CLASS_KNIGHT));
    DrawText(assets->pFontCreate.get(), {pTextCenter + 323, 417}, pColorText, localization->className(CLASS_KNIGHT), pGUIWindow_CurrentMenu->frameRect);

    pColorText = colorTable.Aqua;
    if (uClassType != CLASS_PALADIN)
        pColorText = colorTable.White;
    pTextCenter =
        assets->pFontCreate->AlignText_Center(65, localization->className(CLASS_PALADIN));
    DrawText(assets->pFontCreate.get(), {pTextCenter + 323, pIntervalY + 417}, pColorText, localization->className(CLASS_PALADIN), pGUIWindow_CurrentMenu->frameRect);

    pColorText = colorTable.Aqua;
    if (uClassType != CLASS_RANGER)
        pColorText = colorTable.White;
    pTextCenter =
        assets->pFontCreate->AlignText_Center(65, localization->className(CLASS_RANGER));
    DrawText(assets->pFontCreate.get(), {pTextCenter + 323, 2 * pIntervalY + 417}, pColorText, localization->className(CLASS_RANGER), pGUIWindow_CurrentMenu->frameRect);

    pColorText = colorTable.Aqua;
    if (uClassType != CLASS_CLERIC)
        pColorText = colorTable.White;
    pTextCenter =
        assets->pFontCreate->AlignText_Center(65, localization->className(CLASS_CLERIC));
    DrawText(assets->pFontCreate.get(), {pTextCenter + 388, 417}, pColorText, localization->className(CLASS_CLERIC), pGUIWindow_CurrentMenu->frameRect);

    pColorText = colorTable.Aqua;
    if (uClassType != CLASS_DRUID)
        pColorText = colorTable.White;
    pTextCenter =
        assets->pFontCreate->AlignText_Center(65, localization->className(CLASS_DRUID));
    DrawText(assets->pFontCreate.get(), {pTextCenter + 388, pIntervalY + 417}, pColorText, localization->className(CLASS_DRUID), pGUIWindow_CurrentMenu->frameRect);

    pColorText = colorTable.Aqua;
    if (uClassType != CLASS_SORCERER)
        pColorText = colorTable.White;
    pTextCenter =
        assets->pFontCreate->AlignText_Center(65, localization->className(CLASS_SORCERER));
    DrawText(assets->pFontCreate.get(), {pTextCenter + 388, 2 * pIntervalY + 417}, pColorText, localization->className(CLASS_SORCERER), pGUIWindow_CurrentMenu->frameRect);

    pColorText = colorTable.Aqua;
    if (uClassType != CLASS_ARCHER)
        pColorText = colorTable.White;
    pTextCenter =
        assets->pFontCreate->AlignText_Center(65, localization->className(CLASS_ARCHER));
    DrawText(assets->pFontCreate.get(), {pTextCenter + 453, 417}, pColorText, localization->className(CLASS_ARCHER), pGUIWindow_CurrentMenu->frameRect);

    pColorText = colorTable.Aqua;
    if (uClassType != CLASS_MONK)
        pColorText = colorTable.White;
    pTextCenter =
        assets->pFontCreate->AlignText_Center(65, localization->className(CLASS_MONK));
    DrawText(assets->pFontCreate.get(), {pTextCenter + 453, pIntervalY + 417}, pColorText, localization->className(CLASS_MONK), pGUIWindow_CurrentMenu->frameRect);

    pColorText = colorTable.Aqua;
    if (uClassType != CLASS_THIEF)
        pColorText = colorTable.White;
    pTextCenter = assets->pFontCreate->AlignText_Center(65, localization->className(CLASS_THIEF));
    DrawText(assets->pFontCreate.get(), {pTextCenter + 453, 2 * pIntervalY + 417}, pColorText, localization->className(CLASS_THIEF), pGUIWindow_CurrentMenu->frameRect);

    pTextCenter = assets->pFontCreate->AlignText_Center(
        236, localization->str(LSTR_AVAILABLE_SKILLS));
    DrawText(assets->pFontCreate.get(), {pTextCenter + 37, 395}, colorTable.Tacha, localization->str(LSTR_AVAILABLE_SKILLS), pGUIWindow_CurrentMenu->frameRect);
    for (int i = 0; i < 9; ++i) {
        pSkillId = pParty->pCharacters[uPlayerCreationUI_SelectedCharacter].GetSkillIdxByOrder(i + 4);
        pText = localization->skillName(pSkillId);
        // trim skills that are too long
        if (pText.size() > 13)
            pText.resize(12);
        if (pText == "Body Building")
            pText = "Body Build";
        if (pText.starts_with(' '))
            pText.clear();

        pCorrective = -10;             // -5
        //if ((signed int)pLenText < 8)  // if ( (signed int)v124 > 2 )
        //    pCorrective = 0;
        pColorText = colorTable.Aqua;
        if (!pParty->pCharacters[uPlayerCreationUI_SelectedCharacter].pActiveSkills[pSkillId])
            pColorText = colorTable.White;

        pTextCenter = assets->pFontCreate->AlignText_Center(100, pText);
        DrawText(assets->pFontCreate.get(), {100 * (i / 3) + pTextCenter + pCorrective + 17, pIntervalY * (i % 3) + 417}, pColorText, pText, pGUIWindow_CurrentMenu->frameRect);
    }

    pTextCenter = assets->pFontCreate->AlignText_Center(
                0x5C, localization->str(LSTR_BONUS_1));
        DrawText(assets->pFontCreate.get(), {pTextCenter + 533, 394}, colorTable.Tacha, localization->str(LSTR_BONUS_1), pGUIWindow_CurrentMenu->frameRect);

    // force draw so overlays dont get muddled
    render->DrawTwodVerts();
    render->EndTextNew();

    pBonusNum = CharacterCreation_GetUnspentAttributePointCount();

    auto unspent_attribute_bonus_label = fmt::format("{}", pBonusNum);
    pTextCenter = assets->pFontCreate->AlignText_Center(84, unspent_attribute_bonus_label);
    DrawText(assets->pFontCreate.get(), {pTextCenter + 530, 410}, colorTable.White, unspent_attribute_bonus_label, pGUIWindow_CurrentMenu->frameRect);

    if (errorMessageExpireTime > pMiscTimer->time()) {
        auto& sHint = pBonusNum < 0 ? localization->str(LSTR_YOU_CANT_SPEND_MORE_THAN_50_POINTS) : localization->str(LSTR_CREATE_PARTY_CANNOT_BE_COMPLETED_UNLESS);
        Recti popupRect(170, 140, 300, 100);
        DrawMessageBox(0, popupRect, sHint);
    }

    // force draw so overlays dont get muddled
    render->DrawTwodVerts();
    render->EndTextNew();
}

// MM6's creation screen shares makeme.pcx/maketop/makesky/arrow asset names with MM7 but has its
// own layout: 12 face stills on marble plates with baked-in 32x16 arrow buttons, an animated
// aframe1 flame below the selected portrait, fl/fr pillar torches, six classes bottom-left, the
// nine pickable skills bottom-center, and the 50-point bonus pool bottom-right with a lone
// BUTTMAKE OK scroll (no Clear button, no voice arrows - the face fixes the voice and sex).
void GUIWindow_PartyCreation::initializeMm6() {
    int v0 = assets->pFontCreate->GetHeight() - 2;

    // Class icons keyed by the engine's base-class index; only MM6's six classes exist
    // (IC_KNIG etc. - shorter names than MM7's IC_KNIGHT). Their teal background is palette
    // index 0 but VGA-scaled to (0,252,252), which the (0,255,255) colorkey misses - so Alpha,
    // or they draw as sky-blue boxes.
    ui_partycreation_class_icons.fill(nullptr);
    ui_partycreation_class_icons[std::to_underlying(CLASS_KNIGHT) / 4] = assets->getImage_Alpha("IC_KNIG");
    ui_partycreation_class_icons[std::to_underlying(CLASS_PALADIN) / 4] = assets->getImage_Alpha("IC_PALAD");
    ui_partycreation_class_icons[std::to_underlying(CLASS_ARCHER) / 4] = assets->getImage_Alpha("IC_ARCH");
    ui_partycreation_class_icons[std::to_underlying(CLASS_CLERIC) / 4] = assets->getImage_Alpha("IC_CLER");
    ui_partycreation_class_icons[std::to_underlying(CLASS_DRUID) / 4] = assets->getImage_Alpha("IC_DRUID");
    ui_partycreation_class_icons[std::to_underlying(CLASS_SORCERER) / 4] = assets->getImage_Alpha("IC_SORC");

    // MM6 loader convention: cut-outs load as Alpha (the palette-0-transparent header flag is
    // rarely set and TealMask never matches MM6's VGA palettes), opaque plates as Solid.
    ui_partycreation_top = assets->getImage_Alpha("maketop");
    ui_partycreation_sky_scroller = assets->getImage_Solid("makesky");

    for (int face = 0; face < 12; face++)
        creationMm6Portraits[face] = assets->getImage_Solid(
            face < 8 ? fmt::format("ccmale{:c}", 'a' + face) : fmt::format("ccgirl{:c}", 'a' + face - 8));

    // The focus arrows draw through MM6's transparent blit (MM6.EXE 0x40b0c0), which skips pixels
    // whose 16-bit color is 0 - a BLACK colorkey, palette indices don't matter. The arrows keep
    // their background at palette index 3 (their only black entry, no black inside the art); index
    // 0 is an unused magenta sentinel, so palette-0 alpha would leave an opaque black box, and the
    // colorkey must be forced past the images' palette-0-transparent header flag (0x200).
    assert(ui_partycreation_arrow_l.size() == 19);
    for (int i = 0; i < ui_partycreation_arrow_l.size(); ++i) {
        ui_partycreation_arrow_l[i] = assets->getImage_ColorKey(fmt::format("arrowl{}", i + 1), colorTable.Black, true);
        ui_partycreation_arrow_r[i] = assets->getImage_ColorKey(fmt::format("arrowr{}", i + 1), colorTable.Black, true);
    }
    for (int i = 0; i < creationMm6FlamesLeft.size(); ++i) {
        creationMm6FlamesLeft[i] = assets->getImage_Alpha(fmt::format("fl{}", i + 1));
        creationMm6FlamesRight[i] = assets->getImage_Alpha(fmt::format("fr{}", i + 1));
    }

    // The selected-character flame is a sprite frameset, like the turn-based indicator's
    // newhand1/newglas1 (MM6.EXE loads it at 0x42A610 alongside them).
    creationMm6FlameFramesetId = pSpriteFrameTable->FastFindSprite("aframe1");
    if (creationMm6FlameFramesetId > 0)
        pSpriteFrameTable->InitializeSprite(creationMm6FlameFramesetId);

    ui_partycreation_minus = assets->getImage_Alpha("makeminu");
    ui_partycreation_plus = assets->getImage_Alpha("makeplus");
    ui_partycreation_buttmake = assets->getImage_Solid("BUTTMAKE");

    creationMm6NameTyped.fill(false);

    // Button layout from MM6.EXE 0x451FF4-0x452670. The face arrows are baked into the portrait
    // plates in makeme.pcx, so the buttons carry no textures.
    for (int i = 0; i < 4; i++)
        CreateButton({8 + 158 * i, 120}, {145, 25}, BUTTON_TYPE_NORMAL, 0, UIMSG_PlayerCreationChangeName, i);
    for (int i = 0; i < 4; i++)
        pCreationUI_BtnPressLeft[i] = CreateButton({86 + 159 * i, 31}, {32, 16}, BUTTON_TYPE_NORMAL, 0, UIMSG_PlayerCreation_FacePrev, i);
    for (int i = 0; i < 4; i++)
        pCreationUI_BtnPressRight[i] = CreateButton({118 + 159 * i, 31}, {32, 16}, BUTTON_TYPE_NORMAL, 0, UIMSG_PlayerCreation_FaceNext, i);

    for (int i = 0; i < 4; i++) {
        int uX = 8 + 158 * i;
        CreateButton({uX, 308}, {150, v0}, BUTTON_TYPE_NORMAL, 0, UIMSG_48, i);
        CreateButton({uX, v0 + 308}, {150, v0}, BUTTON_TYPE_NORMAL, 0, UIMSG_49, i);
        CreateButton(fmt::format("PartyCreation_RemoveSkill3_{}", i), {uX, 2 * v0 + 308}, {150, v0}, BUTTON_TYPE_NORMAL, 0, UIMSG_PlayerCreationRemoveUpSkill, i);
        CreateButton(fmt::format("PartyCreation_RemoveSkill4_{}", i), {uX, 3 * v0 + 308}, {150, v0}, BUTTON_TYPE_NORMAL, 0, UIMSG_PlayerCreationRemoveDownSkill, i);
    }

    CreateButton({5, 21}, {153, 365}, BUTTON_TYPE_NORMAL, 0, UIMSG_PlayerCreation_SelectAttribute, 0, INPUT_ACTION_SELECT_CHAR_1);
    CreateButton({163, 21}, {153, 365}, BUTTON_TYPE_NORMAL, 0, UIMSG_PlayerCreation_SelectAttribute, 1, INPUT_ACTION_SELECT_CHAR_2);
    CreateButton({321, 21}, {153, 365}, BUTTON_TYPE_NORMAL, 0, UIMSG_PlayerCreation_SelectAttribute, 2, INPUT_ACTION_SELECT_CHAR_3);
    CreateButton({479, 21}, {153, 365}, BUTTON_TYPE_NORMAL, 0, UIMSG_PlayerCreation_SelectAttribute, 3, INPUT_ACTION_SELECT_CHAR_4);

    // Stat rows sit at y=162 in MM6 (169 in MM7).
    for (int column = 0; column < 4; column++)
        for (int row = 0; row < 7; row++)
            CreateButton({23 + 158 * column, 162 + v0 * row}, {120, 20}, BUTTON_TYPE_NORMAL, 0, UIMSG_0, 7 * column + row);

    setKeyboardControlGroup(28, true, 7, 32);

    // Class picker bottom-LEFT: Knight/Cleric/Sorcerer down the first column at x=60,
    // Paladin/Archer/Druid down the second at x=140.
    CreateButton({60, 417}, {70, v0}, BUTTON_TYPE_NORMAL, 0, UIMSG_PlayerCreationSelectClass, std::to_underlying(CLASS_KNIGHT));
    CreateButton({60, v0 + 417}, {70, v0}, BUTTON_TYPE_NORMAL, 0, UIMSG_PlayerCreationSelectClass, std::to_underlying(CLASS_CLERIC));
    CreateButton({60, 2 * v0 + 417}, {70, v0}, BUTTON_TYPE_NORMAL, 0, UIMSG_PlayerCreationSelectClass, std::to_underlying(CLASS_SORCERER));
    CreateButton({140, 417}, {70, v0}, BUTTON_TYPE_NORMAL, 0, UIMSG_PlayerCreationSelectClass, std::to_underlying(CLASS_PALADIN));
    CreateButton({140, v0 + 417}, {70, v0}, BUTTON_TYPE_NORMAL, 0, UIMSG_PlayerCreationSelectClass, std::to_underlying(CLASS_ARCHER));
    CreateButton({140, 2 * v0 + 417}, {70, v0}, BUTTON_TYPE_NORMAL, 0, UIMSG_PlayerCreationSelectClass, std::to_underlying(CLASS_DRUID));

    // The nine available creation skills, bottom-center.
    for (int i = 0; i < 9; i++)
        CreateButton({230 + 80 * (i / 3), v0 * (i % 3) + 417}, {80, v0}, BUTTON_TYPE_NORMAL, 0, UIMSG_PlayerCreationSelectActiveSkill, i);

    pPlayerCreationUI_BtnOK = CreateButton("PartyCreation_OK", {511, 438}, {63, 29}, BUTTON_TYPE_NORMAL, 0, UIMSG_PlayerCreationClickOK, 0, INPUT_ACTION_PARTY_CREATION_DONE, "", {ui_partycreation_buttmake});
    pPlayerCreationUI_BtnReset = nullptr; // MM6 has no Clear button.
    pPlayerCreationUI_BtnMinus = CreateButton({482, 392}, {20, 35}, BUTTON_TYPE_NORMAL, 0, UIMSG_PlayerCreationClickMinus, 0, INPUT_ACTION_PARTY_CREATION_DEC, "", {ui_partycreation_minus});
    pPlayerCreationUI_BtnPlus = CreateButton({580, 392}, {22, 35}, BUTTON_TYPE_NORMAL, 0, UIMSG_PlayerCreationClickPlus, 1, INPUT_ACTION_PARTY_CREATION_INC, "", {ui_partycreation_plus});

    ui_partycreation_font = GUIFont::LoadFont("cchar.fnt");
}

//----- (0049695A) --------------------------------------------------------
GUIWindow_PartyCreation::GUIWindow_PartyCreation() :
    GUIWindow(WINDOW_CharacterCreation, {0, 0}, render->GetRenderDimensions()) {
    engine->_messageQueue->clear();
    errorMessageExpireTime = Duration(); // Clear any lingering error popup from previous session.

    main_menu_background = assets->getImage_PCXFromIconsLOD("makeme.pcx");

    current_screen_type = SCREEN_PARTY_CREATION;
    uPlayerCreationUI_SelectedCharacter = 0;

    if (engine->gameVersion() == GAME_VERSION_MM6) {
        initializeMm6();
        return;
    }

    int v0 = assets->pFontCreate->GetHeight() - 2;

    ui_partycreation_class_icons[0] = assets->getImage_ColorKey("IC_KNIGHT");
    ui_partycreation_class_icons[1] = assets->getImage_ColorKey("IC_THIEF");
    ui_partycreation_class_icons[2] = assets->getImage_ColorKey("IC_MONK");
    ui_partycreation_class_icons[3] = assets->getImage_ColorKey("IC_PALAD");
    ui_partycreation_class_icons[4] = assets->getImage_ColorKey("IC_ARCH");
    ui_partycreation_class_icons[5] = assets->getImage_ColorKey("IC_RANGER");
    ui_partycreation_class_icons[6] = assets->getImage_ColorKey("IC_CLER");
    ui_partycreation_class_icons[7] = assets->getImage_ColorKey("IC_DRUID");
    ui_partycreation_class_icons[8] = assets->getImage_ColorKey("IC_SORC");

    ui_partycreation_top = assets->getImage_Alpha("MAKETOP");
    ui_partycreation_sky_scroller = assets->getImage_Solid("MAKESKY");

    ui_partycreation_character_frame = assets->getImage_Solid("aframe1");

    for (int uX = 0; uX < 22; ++uX) {
        ui_partycreation_portraits[uX] = assets->getImage_ColorKey(fmt::format("{}01", pPlayerPortraitsNames[uX]));
    }

    ui_partycreation_minus = assets->getImage_ColorKey("buttminu");
    ui_partycreation_plus = assets->getImage_ColorKey("buttplus");
    ui_partycreation_right = assets->getImage_ColorKey("presrigh");
    ui_partycreation_left = assets->getImage_ColorKey("presleft");

    // sprites number go from (1 to 19)
    assert(ui_partycreation_arrow_l.size() == 19);
    assert(ui_partycreation_arrow_r.size() == 19);
    for (int i = 0; i < ui_partycreation_arrow_l.size(); ++i) {
        ui_partycreation_arrow_l[i] = assets->getImage_Alpha(fmt::format("arrowl{}", i + 1));
        ui_partycreation_arrow_r[i] = assets->getImage_Alpha(fmt::format("arrowr{}", i + 1));
    }

    int uX = 8;
    for (int characterIndex = 0; characterIndex < 4; characterIndex++) {
        CreateButton({uX, 120}, {145, 25}, BUTTON_TYPE_NORMAL, 0, UIMSG_PlayerCreationChangeName, characterIndex);
        uX += 158;
    }

    pCreationUI_BtnPressLeft[0] = CreateButton({10, 32}, {11, 13}, BUTTON_TYPE_NORMAL, 0, UIMSG_PlayerCreation_FacePrev, 0, INPUT_ACTION_INVALID, "", {ui_partycreation_left});
    pCreationUI_BtnPressLeft[1] = CreateButton({169, 32}, {11, 13}, BUTTON_TYPE_NORMAL, 0, UIMSG_PlayerCreation_FacePrev, 1, INPUT_ACTION_INVALID, "", {ui_partycreation_left});
    pCreationUI_BtnPressLeft[2] = CreateButton({327, 32}, {11, 13}, BUTTON_TYPE_NORMAL, 0, UIMSG_PlayerCreation_FacePrev, 2, INPUT_ACTION_INVALID, "", {ui_partycreation_left});
    pCreationUI_BtnPressLeft[3] = CreateButton({486, 32}, {11, 13}, BUTTON_TYPE_NORMAL, 0, UIMSG_PlayerCreation_FacePrev, 3, INPUT_ACTION_INVALID, "", {ui_partycreation_left});

    pCreationUI_BtnPressRight[0] = CreateButton({74, 32}, {11, 13}, BUTTON_TYPE_NORMAL, 0, UIMSG_PlayerCreation_FaceNext, 0, INPUT_ACTION_INVALID, "", {ui_partycreation_right});
    pCreationUI_BtnPressRight[1] = CreateButton({233, 32}, {11, 13}, BUTTON_TYPE_NORMAL, 0, UIMSG_PlayerCreation_FaceNext, 1, INPUT_ACTION_INVALID, "", {ui_partycreation_right});
    pCreationUI_BtnPressRight[2] = CreateButton({391, 32}, {11, 13}, BUTTON_TYPE_NORMAL, 0, UIMSG_PlayerCreation_FaceNext, 2, INPUT_ACTION_INVALID, "", {ui_partycreation_right});
    pCreationUI_BtnPressRight[3] = CreateButton({549, 32}, {11, 13}, BUTTON_TYPE_NORMAL, 0, UIMSG_PlayerCreation_FaceNext, 3, INPUT_ACTION_INVALID, "", {ui_partycreation_right});

    pCreationUI_BtnPressLeft2[0] = CreateButton({10, 103}, {11, 13}, BUTTON_TYPE_NORMAL, 0, UIMSG_PlayerCreation_VoicePrev, 0, INPUT_ACTION_INVALID, "", {ui_partycreation_left});
    pCreationUI_BtnPressLeft2[1] = CreateButton({169, 103}, {11, 13}, BUTTON_TYPE_NORMAL, 0, UIMSG_PlayerCreation_VoicePrev, 1, INPUT_ACTION_INVALID, "", {ui_partycreation_left});
    pCreationUI_BtnPressLeft2[2] = CreateButton({327, 103}, {11, 13}, BUTTON_TYPE_NORMAL, 0, UIMSG_PlayerCreation_VoicePrev, 2, INPUT_ACTION_INVALID, "", {ui_partycreation_left});
    pCreationUI_BtnPressLeft2[3] = CreateButton({486, 103}, {11, 13}, BUTTON_TYPE_NORMAL, 0, UIMSG_PlayerCreation_VoicePrev, 3, INPUT_ACTION_INVALID, "", {ui_partycreation_left});

    pCreationUI_BtnPressRight2[0] = CreateButton({74, 103}, {11, 13}, BUTTON_TYPE_NORMAL, 0, UIMSG_PlayerCreation_VoiceNext, 0, INPUT_ACTION_INVALID, "", {ui_partycreation_right});
    pCreationUI_BtnPressRight2[1] = CreateButton({233, 103}, {11, 13}, BUTTON_TYPE_NORMAL, 0, UIMSG_PlayerCreation_VoiceNext, 1, INPUT_ACTION_INVALID, "", {ui_partycreation_right});
    pCreationUI_BtnPressRight2[2] = CreateButton({391, 103}, {11, 13}, BUTTON_TYPE_NORMAL, 0, UIMSG_PlayerCreation_VoiceNext, 2, INPUT_ACTION_INVALID, "", {ui_partycreation_right});
    pCreationUI_BtnPressRight2[3] = CreateButton({549, 103}, {11, 13}, BUTTON_TYPE_NORMAL, 0, UIMSG_PlayerCreation_VoiceNext, 3, INPUT_ACTION_INVALID, "", {ui_partycreation_right});

    uX = 8;
    for (int characterIndex = 0 ; characterIndex < 4; characterIndex++) {
        CreateButton({uX, 308}, {150, v0}, BUTTON_TYPE_NORMAL, 0, UIMSG_48, characterIndex);
        CreateButton({uX, v0 + 308}, {150, v0}, BUTTON_TYPE_NORMAL, 0, UIMSG_49, characterIndex);
        CreateButton(fmt::format("PartyCreation_RemoveSkill3_{}", characterIndex), {uX, 2 * v0 + 308}, {150, v0}, BUTTON_TYPE_NORMAL, 0, UIMSG_PlayerCreationRemoveUpSkill, characterIndex);
        CreateButton(fmt::format("PartyCreation_RemoveSkill4_{}", characterIndex), {uX, 3 * v0 + 308}, {150, v0}, BUTTON_TYPE_NORMAL, 0, UIMSG_PlayerCreationRemoveDownSkill, characterIndex);
        uX += 158;
    }

    CreateButton({5, 21}, {153, 365}, BUTTON_TYPE_NORMAL, 0, UIMSG_PlayerCreation_SelectAttribute, 0, INPUT_ACTION_SELECT_CHAR_1);
    CreateButton({163, 21}, {153, 365}, BUTTON_TYPE_NORMAL, 0, UIMSG_PlayerCreation_SelectAttribute, 1, INPUT_ACTION_SELECT_CHAR_2);
    CreateButton({321, 21}, {153, 365}, BUTTON_TYPE_NORMAL, 0, UIMSG_PlayerCreation_SelectAttribute, 2, INPUT_ACTION_SELECT_CHAR_3);
    CreateButton({479, 21}, {153, 365}, BUTTON_TYPE_NORMAL, 0, UIMSG_PlayerCreation_SelectAttribute, 3, INPUT_ACTION_SELECT_CHAR_4);

    uX = 23;
    int uControlParam = 2;
    do {
        CreateButton({uX, 169}, {120, 20}, BUTTON_TYPE_NORMAL, 0, UIMSG_0, uControlParam - 2);
        CreateButton({uX, v0 + 169}, {120, 20}, BUTTON_TYPE_NORMAL, 0, UIMSG_0, uControlParam - 1);
        CreateButton({uX, 2 * v0 + 169}, {120, 20}, BUTTON_TYPE_NORMAL, 0, UIMSG_0, uControlParam);
        CreateButton({uX, 3 * v0 + 169}, {120, 20}, BUTTON_TYPE_NORMAL, 0, UIMSG_0, uControlParam + 1);
        CreateButton({uX, 4 * v0 + 169}, {120, 20}, BUTTON_TYPE_NORMAL, 0, UIMSG_0, uControlParam + 2);
        CreateButton({uX, 5 * v0 + 169}, {120, 20}, BUTTON_TYPE_NORMAL, 0, UIMSG_0, uControlParam + 3);
        CreateButton({uX, 6 * v0 + 169}, {120, 20}, BUTTON_TYPE_NORMAL, 0, UIMSG_0, uControlParam + 4);

        uControlParam += 7;
        uX += 158;
    } while ((signed int)uControlParam < 30);

    setKeyboardControlGroup(28, true, 7, 40);

    CreateButton({323, 417}, {65, v0}, BUTTON_TYPE_NORMAL, 0, UIMSG_PlayerCreationSelectClass, 0);
    CreateButton({323, v0 + 417}, {65, v0}, BUTTON_TYPE_NORMAL, 0, UIMSG_PlayerCreationSelectClass, 0xC);
    CreateButton({323, 2 * v0 + 417}, {65, v0}, BUTTON_TYPE_NORMAL, 0, UIMSG_PlayerCreationSelectClass, 0x14);
    CreateButton({388, 417}, {65, v0}, BUTTON_TYPE_NORMAL, 0, UIMSG_PlayerCreationSelectClass, 0x18);
    CreateButton({388, v0 + 417}, {65, v0}, BUTTON_TYPE_NORMAL, 0, UIMSG_PlayerCreationSelectClass, 0x1C);
    CreateButton({388, 2 * v0 + 417}, {65, v0}, BUTTON_TYPE_NORMAL, 0, UIMSG_PlayerCreationSelectClass, 0x20);
    CreateButton({453, 417}, {65, v0}, BUTTON_TYPE_NORMAL, 0, UIMSG_PlayerCreationSelectClass, 0x10);
    CreateButton({453, v0 + 417}, {65, v0}, BUTTON_TYPE_NORMAL, 0, UIMSG_PlayerCreationSelectClass, 8);
    CreateButton({453, 2 * v0 + 417}, {65, v0}, BUTTON_TYPE_NORMAL, 0, UIMSG_PlayerCreationSelectClass, 4);

    uControlParam = 0;
    do {
        uX = -5;
        if (uControlParam <= 3)
            uX = 0;
        CreateButton({100 * (uControlParam / 3) + uX + 17, v0 * (uControlParam % 3) + 417}, {100, v0}, BUTTON_TYPE_NORMAL, 0, UIMSG_PlayerCreationSelectActiveSkill, uControlParam);
        ++uControlParam;
    } while (uControlParam < 9);

    ui_partycreation_buttmake = assets->getImage_Solid("BUTTMAKE");
    ui_partycreation_buttmake2 = assets->getImage_Solid("BUTTMAKE2");

    pPlayerCreationUI_BtnOK = CreateButton("PartyCreation_OK", {580, 431}, {51, 39}, BUTTON_TYPE_NORMAL, 0, UIMSG_PlayerCreationClickOK, 0, INPUT_ACTION_PARTY_CREATION_DONE, "", {ui_partycreation_buttmake});
    pPlayerCreationUI_BtnReset = CreateButton("PartyCreation_Clear", {527, 431}, {51, 39}, BUTTON_TYPE_NORMAL, 0, UIMSG_PlayerCreationClickReset, 0, INPUT_ACTION_PARTY_CREATION_CLEAR, "", {ui_partycreation_buttmake2});
    pPlayerCreationUI_BtnMinus = CreateButton({523, 393}, {20, 35}, BUTTON_TYPE_NORMAL, 0, UIMSG_PlayerCreationClickMinus, 0, INPUT_ACTION_PARTY_CREATION_DEC, "", {ui_partycreation_minus});
    pPlayerCreationUI_BtnPlus = CreateButton({613, 393}, {20, 35}, BUTTON_TYPE_NORMAL, 0, UIMSG_PlayerCreationClickPlus, 1, INPUT_ACTION_PARTY_CREATION_INC, "", {ui_partycreation_plus});

    ui_partycreation_font = GUIFont::LoadFont("cchar.fnt");
}

GUIWindow_PartyCreation::~GUIWindow_PartyCreation() {
    main_menu_background->release();
    main_menu_background = nullptr;
}

// Grants the MM6 starting inventory the way MM6.EXE does on leaving party creation
// (0x452820-0x452B2A): a random tier-2 ring rolled into the first slot, then one item per
// active skill in id order - a weapon or armor piece per equipment skill, the book of each
// granted school's SECOND spell with the FIRST spell learned, a potion bottle plus a random
// herb per miscellaneous skill - The Letter for character 0, everything identified, a Knight's
// 10 base magic resistance, and HP/SP topped up. The new.lod template party's gear is exactly
// this grant's output for the default skill sets (its named rings are captured random rolls);
// weapons and armor end up worn, matching the template's equipped state.
static void givePartyItemsMm6() {
    struct SkillEquipment {
        Skill skill;
        int itemId;
        ItemSlot slot;
    };
    static constexpr std::array<SkillEquipment, 11> kSkillEquipment = {{
        {SKILL_STAFF, 61, ITEM_SLOT_MAIN_HAND},  {SKILL_SWORD, 1, ITEM_SLOT_MAIN_HAND},
        {SKILL_DAGGER, 15, ITEM_SLOT_MAIN_HAND}, {SKILL_AXE, 23, ITEM_SLOT_MAIN_HAND},
        {SKILL_SPEAR, 31, ITEM_SLOT_MAIN_HAND},  {SKILL_BOW, 47, ITEM_SLOT_BOW},
        {SKILL_MACE, 50, ITEM_SLOT_MAIN_HAND},   {SKILL_SHIELD, 84, ITEM_SLOT_OFF_HAND},
        {SKILL_LEATHER, 66, ITEM_SLOT_ARMOUR},   {SKILL_CHAIN, 71, ITEM_SLOT_ARMOUR},
        {SKILL_PLATE, 76, ITEM_SLOT_ARMOUR},
    }};
    static constexpr std::array<Skill, 7> kHerbSkills = {
        SKILL_ITEM_ID, SKILL_REPAIR, SKILL_MEDITATION, SKILL_PERCEPTION,
        SKILL_DIPLOMACY, SKILL_TRAP_DISARM, SKILL_LEARNING};

    for (int i = 0; i < 4; i++) {
        Character &character = pParty->pCharacters[i];

        if (character.classType == CLASS_KNIGHT)
            character.sResMagicBase = 10;

        Item ring;
        pItemTable->generateItem(ITEM_TREASURE_LEVEL_2, RANDOM_ITEM_RING, &ring);
        character.inventory.add(ring);

        for (Skill skill : allVisibleSkills()) {
            if (!character.pActiveSkills[skill])
                continue;
            if (auto equipment = std::ranges::find(kSkillEquipment, skill, &SkillEquipment::skill);
                equipment != kSkillEquipment.end()) {
                Item item = Item(static_cast<ItemId>(equipment->itemId));
                if (!character.inventory.entry(equipment->slot))
                    character.inventory.equip(equipment->slot, item);
                else
                    character.inventory.add(item);
            } else if (skill >= SKILL_FIRE && skill <= SKILL_BODY) {
                int school = std::to_underlying(skill) - std::to_underlying(SKILL_FIRE);
                character.bHaveSpell[static_cast<SpellId>(1 + 11 * school)] = true;
                character.inventory.add(Item(static_cast<ItemId>(301 + 11 * school)));
            } else if (std::ranges::contains(kHerbSkills, skill)) {
                character.inventory.add(Item(static_cast<ItemId>(163)));                     // Potion Bottle.
                character.inventory.add(Item(static_cast<ItemId>(160 + grng->random(3))));   // A random herb.
            }
        }

        if (i == 0)
            character.inventory.add(Item(static_cast<ItemId>(505))); // The Letter - the opening delivery quest.

        for (InventoryEntry entry : character.inventory.entries())
            entry->SetIdentified();

        for (MagicSchool page : allMagicSchools()) {
            if (character.pActiveSkills[skillForMagicSchool(page)]) {
                character.lastOpenedSpellbookPage = page;
                break;
            }
        }

        character.health = character.GetMaxHealth();
        character.mana = character.GetMaxMana();
    }
}

//----- (00497526) --------------------------------------------------------
bool PartyCreationUI_LoopInternal() {
    Item item;
    bool party_not_creation_flag;

    party_not_creation_flag = false;

    pGUIWindow_CurrentMenu->keyboard_input_status = WINDOW_INPUT_NONE;
    SetCurrentMenuID(MENU_CREATEPARTY);
    while (GetCurrentMenuID() == MENU_CREATEPARTY) {
        MessageLoopWithWait();

        pMiscTimer->tick(); // This one is used for animations.

        // PlayerCreationUI_Draw();
        // MainMenu_EventLoop();
        CreateParty_EventLoop();
        render->BeginScene2D();
        GUI_UpdateWindows();
        render->Present();
        if (uGameState ==
            GAME_FINISHED) {  // if click Esc in PlayerCreation Window
            party_not_creation_flag = true;
            SetCurrentMenuID(MENU_MAIN);
            continue;
        }
        if (uGameState ==
            GAME_STATE_STARTING_NEW_GAME) {  // if click OK in PlayerCreation
                                             // Window
            uGameState = GAME_STATE_PLAYING;
            SetCurrentMenuID(MENU_NEWGAME);
            continue;
        }
    }

    pGUIWindow_CurrentMenu = nullptr;

    if (engine->gameVersion() == GAME_VERSION_MM6) {
        givePartyItemsMm6();
        pAudioPlayer->stopSounds();
        return party_not_creation_flag;
    }

    item.Reset();
    for (unsigned i = 0; i < 4; ++i) {
        if (pParty->pCharacters[i].classType == CLASS_KNIGHT)
            pParty->pCharacters[i].sResMagicBase = 10;
        // TODO(pskelton): why just CHARACTER_BUFF_RESIST_WATER?
        pParty->pCharacters[i].pCharacterBuffs[CHARACTER_BUFF_RESIST_WATER].Reset();
        for (MagicSchool page : allMagicSchools()) {
            if (pParty->pCharacters[i].pActiveSkills[skillForMagicSchool(page)]) {
                pParty->pCharacters[i].lastOpenedSpellbookPage = page;
                break;
            }
        }
        pItemTable->generateItem(ITEM_TREASURE_LEVEL_2, RANDOM_ITEM_RING, &item);
        pParty->pCharacters[i].inventory.add(item);

        pParty->pCharacters[i].health = pParty->pCharacters[i].GetMaxHealth();
        pParty->pCharacters[i].mana = pParty->pCharacters[i].GetMaxMana();
        for (Skill j : allSkills()) {
            if (!pParty->pCharacters[i].pActiveSkills[j]) continue;

            switch (j) {
            case SKILL_STAFF:
                pParty->pCharacters[i].inventory.add(Item(ITEM_STAFF));
                break;
            case SKILL_SWORD:
                pParty->pCharacters[i].inventory.add(Item(ITEM_CRUDE_LONGSWORD));
                break;
            case SKILL_DAGGER:
                pParty->pCharacters[i].inventory.add(Item(ITEM_DAGGER));
                break;
            case SKILL_AXE:
                pParty->pCharacters[i].inventory.add(Item(ITEM_CRUDE_AXE));
                break;
            case SKILL_SPEAR:
                pParty->pCharacters[i].inventory.add(Item(ITEM_CRUDE_SPEAR));
                break;
            case SKILL_BOW:
                pParty->pCharacters[i].inventory.add(Item(ITEM_CROSSBOW));
                break;
            case SKILL_MACE:
                pParty->pCharacters[i].inventory.add(Item(ITEM_MACE));
                break;
            case SKILL_BLASTER:
                logger->error("No blasters at startup :p");
                break;
            case SKILL_SHIELD:
                pParty->pCharacters[i].inventory.add(Item(ITEM_WOODEN_BUCKLER));
                break;
            case SKILL_LEATHER:
                pParty->pCharacters[i].inventory.add(Item(ITEM_LEATHER_ARMOR));
                break;
            case SKILL_CHAIN:
                pParty->pCharacters[i].inventory.add(Item(ITEM_CHAIN_MAIL));
                break;
            case SKILL_PLATE:
                pParty->pCharacters[i].inventory.add(Item(ITEM_PLATE_ARMOR));
                break;
            case SKILL_FIRE:
                pParty->pCharacters[i].inventory.add(Item(ITEM_SPELLBOOK_FIRE_BOLT));
                pParty->pCharacters[i].bHaveSpell[SPELL_FIRE_TORCH_LIGHT] = true;
                break;
            case SKILL_AIR:
                pParty->pCharacters[i].inventory.add(Item(ITEM_SPELLBOOK_FEATHER_FALL));
                pParty->pCharacters[i].bHaveSpell[SPELL_AIR_WIZARD_EYE] = true;
                break;
            case SKILL_WATER:
                pParty->pCharacters[i].inventory.add(Item(ITEM_SPELLBOOK_POISON_SPRAY));
                pParty->pCharacters[i].bHaveSpell[SPELL_WATER_AWAKEN] = true;
                break;
            case SKILL_EARTH:
                pParty->pCharacters[i].inventory.add(Item(ITEM_SPELLBOOK_SLOW));
                pParty->pCharacters[i].bHaveSpell[SPELL_EARTH_STUN] = true;
                break;
            case SKILL_SPIRIT:
                pParty->pCharacters[i].inventory.add(Item(ITEM_SPELLBOOK_BLESS));
                pParty->pCharacters[i].bHaveSpell[SPELL_SPIRIT_DETECT_LIFE] = true;
                break;
            case SKILL_MIND:
                pParty->pCharacters[i].inventory.add(Item(ITEM_SPELLBOOK_MIND_BLAST));
                pParty->pCharacters[i].bHaveSpell[SPELL_MIND_REMOVE_FEAR] = true;
                break;
            case SKILL_BODY:
                pParty->pCharacters[i].inventory.add(Item(ITEM_SPELLBOOK_HEAL));
                pParty->pCharacters[i].bHaveSpell[SPELL_BODY_CURE_WEAKNESS] = true;
                break;
            case SKILL_LIGHT:
            case SKILL_DARK:
                logger->error("No light/dark magic at startup");
                break;
            case SKILL_DIPLOMACY:
                logger->error("No diplomacy in mm7 (yet)");
                break;
            case SKILL_ITEM_ID:
            case SKILL_REPAIR:
            case SKILL_MEDITATION:
            case SKILL_PERCEPTION:
            case SKILL_TRAP_DISARM:
            case SKILL_LEARNING:
                pParty->pCharacters[i].inventory.add(Item(ITEM_POTION_BOTTLE));
                pParty->pCharacters[i].inventory.add(Item(grng->randomSample(allLevel1Reagents())));
                break;
            case SKILL_DODGE:
                pParty->pCharacters[i].inventory.add(Item(ITEM_LEATHER_BOOTS));
                break;
            case SKILL_UNARMED:
                pParty->pCharacters[i].inventory.add(Item(ITEM_GAUNTLETS));
                break;
            case SKILL_CLUB:
                // pParty->pCharacters[i].inventory.add(Item(ITEM_CLUB));
                break;
            default:
                break;
            }

            for (InventoryEntry entry : pParty->pCharacters[i].inventory.entries())
                entry->SetIdentified();
        }
    }

    pAudioPlayer->stopSounds();
    return party_not_creation_flag;
}

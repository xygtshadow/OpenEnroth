#include "MainMenuState.h"

#include <Media/Audio/AudioPlayer.h>
#include <Engine/Engine.h>
#include <Engine/SaveLoad.h>
#include <GUI/GUIMessageQueue.h>
#include <GUI/GUIWindow.h>
#include <GUI/UI/UIBranchlessDialogue.h>
#include <Library/Logger/Logger.h>
#include <Engine/Graphics/Renderer/Renderer.h>

#include <memory>

MainMenuState::MainMenuState() {
}

FsmAction MainMenuState::enter() {
    pAudioPlayer->stopSounds();
    if (engine->gameVersion() == GAME_VERSION_MM6) {
        // MM6's title sequence plays CD track 13, skipping its 18.5s lead-in section
        // (MM6.EXE @0x4a6c19: AIL_redbook_play(track_info(13).start + 18500, end)).
        pAudioPlayer->MusicPlayTrack(MUSIC_MM6_MAIN_MENU, 18.5f);
    } else {
        pAudioPlayer->MusicPlayTrack(MUSIC_MAIN_MENU);
    }

    current_screen_type = SCREEN_GAME;

    pGUIWindow_BranchlessDialogue = nullptr;

    _mainMenuUI = std::make_unique<GUIWindow_MainMenu>();

    // In the future we won't need a concept of "CurrentMenuID" if all the states are managed inside an Fsm
    SetCurrentMenuID(MENU_MAIN);

    return FsmAction::none();
}

FsmAction MainMenuState::update() {
    std::string_view transition;
    while (engine->_messageQueue->haveMessages()) {
        UIMessageType messageType;
        engine->_messageQueue->popMessage(&messageType, nullptr, nullptr);

        _mainMenuUI->processMessage(messageType);

        switch (messageType) {
        case UIMSG_MainMenu_ShowPartyCreationWnd:
            SetCurrentMenuID(MENU_NEWGAME);
            transition = "newGame";
            break;
        case UIMSG_MainMenu_ShowLoadWindow:
            SetCurrentMenuID(MENU_SAVELOAD);
            transition = "loadGame";
            break;
        case UIMSG_ShowCredits:
            SetCurrentMenuID(MENU_CREDITS);
            transition = "credits";
            break;
        case UIMSG_ExitToWindows:
            SetCurrentMenuID(MENU_EXIT_GAME);
            transition = "exitGame";
            break;
        case UIMSG_QuickLoad: {
            int slot = getQuickSaveSlot();
            if (slot != -1) {
                pAudioPlayer->playUISound(SOUND_StartMainChoice02);
                pSavegameList->selectedSlot = slot;
                SetCurrentMenuID(MENU_LoadingProcInMainMenu);
                transition = "quickLoadGame";
            } else {
                logger->debug("UIMSG_QuickLoad - No quick save could be found!");
                pAudioPlayer->playUISound(SOUND_error);
            }
            break;
        }
        default:
            break;
        }
    }

    if (!transition.empty()) {
        // TODO(Gerark) Remove this GUI_UpdateWindows once we have a proper Retained Mode UI system.
        // Right now we're forced to call this to cause the proper removal of temporary "buttons"
        GUI_UpdateWindows();
        return FsmAction::transition(std::exchange(transition, ""));
    }

    return FsmAction::none();
}

void MainMenuState::exit() {
    _mainMenuUI = nullptr;
}

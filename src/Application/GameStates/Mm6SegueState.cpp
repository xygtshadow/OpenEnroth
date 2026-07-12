#include "Mm6SegueState.h"

#include <Engine/Engine.h>
#include <GUI/GUIMessageQueue.h>
#include <GUI/GUIWindow.h>
#include <Media/Audio/AudioPlayer.h>

#include <memory>
#include <string_view>

Mm6SegueState::Mm6SegueState() {
}

FsmAction Mm6SegueState::enter() {
    pAudioPlayer->MusicStop();
    engine->_messageQueue->clear();

    // The prologue plays CD track 10 from the start of the track (MM6.EXE @0x452fdd:
    // AIL_redbook_play(track_info(10)), no lead-in skip - unlike the main menu's track 13).
    pAudioPlayer->MusicPlayTrack(MUSIC_MM6_PROLOGUE);

    _segueUI = std::make_unique<GUIWindow_Mm6Segue>();
    current_screen_type = SCREEN_GAME;

    // We inherit MENU_NEWGAME from MainMenuState, but set it here too, because it isn't inert: it's
    // a value > MENU_MAIN, and that's exactly what makes the SCREEN_GAME right-click popup path bail
    // out (UIPopup.cpp, `if (GetCurrentMenuID() > MENU_MAIN) break;`) instead of drawing character
    // popups over the prologue. Don't leave that depending on who ran before us.
    SetCurrentMenuID(MENU_NEWGAME);

    return FsmAction::none();
}

FsmAction Mm6SegueState::update() {
    std::string_view transition;
    while (engine->_messageQueue->haveMessages()) {
        UIMessageType messageType;
        engine->_messageQueue->popMessage(&messageType, nullptr, nullptr);

        _segueUI->processMessage(messageType);

        switch (messageType) {
        case UIMSG_Mm6Segue_CreateParty:
            SetCurrentMenuID(MENU_NEWGAME);
            transition = "createParty";
            break;
        case UIMSG_Mm6Segue_QuickStart:
            SetCurrentMenuID(MENU_QUICKSTART);
            transition = "quickStart";
            break;
        case UIMSG_Escape:
            transition = "back";
            break;
        default:
            break;
        }
    }

    if (!transition.empty()) {
        // processMessage() above spawns an OnButtonClick that keeps a raw pointer to a button owned
        // by _segueUI and dereferences it on the next GUI_UpdateWindows() pass - which would come
        // after exit() has already destroyed the window. So pump it here, before transitioning
        // away, exactly like MainMenuState::update() does.
        // TODO(Gerark) Remove this GUI_UpdateWindows once we have a proper Retained Mode UI system.
        GUI_UpdateWindows();
        return FsmAction::transition(transition);
    }

    return FsmAction::none();
}

void Mm6SegueState::exit() {
    _segueUI.reset();

    pAudioPlayer->MusicStop();

    // No stopSounds() here, deliberately - and MainMenuState doesn't have one either, for the same
    // reason. The transition that gets us here is a button click, and the pump in update() has just
    // played its click sound; stopping sounds on the way out would cut that off in the very same
    // frame. Sounds that do need stopping get stopped by whatever we're leaving for: MainMenuState
    // does it in enter(), and the creation screen does it on its way into the game.
}

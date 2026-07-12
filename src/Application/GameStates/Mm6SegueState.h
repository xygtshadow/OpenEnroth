#pragma once

#include <GUI/UI/UIMm6Segue.h>
#include <Library/Fsm/FsmState.h>

#include <memory>

/**
 * MM6's new-game prologue ("segue") screen - the state that owns `GUIWindow_Mm6Segue`. It offers
 * Create Party (run the creation screen) or Quick Start (take the default party as-is), plus Escape
 * to go back. MM6 only - MM7 goes from the main menu straight into party creation.
 *
 * Shown when New Game is chosen *from the main menu*. This is NOT every path into a new game, and
 * that's a known deviation: New Game from the *in-game* menu runs `Game_StartNewGameWhilePlaying()`,
 * which sets `GAME_STATE_NEWGAME_OUT_GAMEMENU`, and `Game::loop()` turns that straight into
 * `SetCurrentMenuID(MENU_NEWGAME); continue;` without ever re-entering the fsm - so that path drops
 * MM6 onto party creation, skipping the prologue. What MM6.EXE does there has not been reversed, so
 * the behavior is left alone rather than guessed at.
 *
 * @offset 0x452bd0
 */
class Mm6SegueState : public FsmState {
 public:
    Mm6SegueState();
    virtual FsmAction update() override;
    virtual FsmAction enter() override;
    virtual void exit() override;

 private:
    std::unique_ptr<GUIWindow_Mm6Segue> _segueUI;
};

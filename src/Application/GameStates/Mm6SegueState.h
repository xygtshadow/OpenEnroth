#pragma once

#include <GUI/UI/UIMm6Segue.h>
#include <Library/Fsm/FsmState.h>

#include <memory>

/**
 * MM6's new-game prologue ("segue") screen - the state that owns `GUIWindow_Mm6Segue`. It offers
 * Create Party (run the creation screen) or Quick Start (take the default party as-is), plus Escape
 * to go back. MM6 only - MM7 goes from the main menu straight into party creation.
 *
 * Shown on *every* path into a new game, which is both of them: New Game from the main menu
 * (`MainMenuState`'s "newGame" transition) and New Game from the in-game menu. The latter runs
 * `Game_StartNewGameWhilePlaying()` and never touches the fsm, so `Game::loop()` restarts the fsm
 * here for it - see `newGameOutOfGameMenuFsmState()` in Game.cpp for the RE that says MM6 shows the
 * prologue there too (@0x42b3ec sets exit reason 4, which @0x4536aa turns into screen id 1, which
 * the jump table @0x453854 sends to @0x4535e3, which calls the segue @0x452bd0).
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

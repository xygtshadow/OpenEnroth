#include "GameFsmBuilder.h"

#include <Engine/Engine.h>
#include <Library/Fsm/FsmBuilder.h>

#include <utility>
#include <memory>

#include "CreditsState.h"
#include "LoadSlotState.h"
#include "LoadStep2State.h"
#include "MainMenuState.h"
#include "Mm6SegueState.h"
#include "StartState.h"
#include "VideoState.h"

std::unique_ptr<Fsm> GameFsmBuilder::buildFsm(std::string_view startingState) {
    FsmBuilder fsmBuilder;
    _buildIntroVideoSequence(fsmBuilder);
    _buildMainMenu(fsmBuilder);
    auto fsm = fsmBuilder.build(startingState);
    return fsm;
}

void GameFsmBuilder::_buildIntroVideoSequence(FsmBuilder &builder) {
    // MM6's startup sequence is 3dologo -> jvc -> mm6intro (MM6.EXE 0x4A6AE0) - there is no
    // NWC logo movie in its vid archives.
    bool isMm6 = engine->gameVersion() == GAME_VERSION_MM6;

    builder
    .state<StartState>("Start")
        .on("proceed").jumpTo("3DOVideo")

    .state<VideoState>("3DOVideo", VideoState::VIDEO_LOGO, "3dologo")
        .on("videoEnd").jumpTo(isMm6 ? "JVCVideo" : "NWCVideo");

    if (!isMm6) {
        builder
        .state<VideoState>("NWCVideo", VideoState::VIDEO_LOGO, "new world logo")
            .on("videoEnd").jumpTo("JVCVideo");
    }

    builder
    .state<VideoState>("JVCVideo", VideoState::VIDEO_LOGO, "jvc")
        .on("videoEnd").jumpTo("IntroVideo")

    .state<VideoState>("IntroVideo", VideoState::VIDEO_INTRO, isMm6 ? "mm6intro" : "Intro")
        .on("videoEnd").jumpTo("LoadStep2")

    .state<LoadStep2State>("LoadStep2")
        .on("done").jumpTo("MainMenu");
}

void GameFsmBuilder::_buildMainMenu(FsmBuilder &builder) {
    bool isMm6 = engine->gameVersion() == GAME_VERSION_MM6;

    builder
    .state<MainMenuState>("MainMenu")
        // MM6 shows its new-game prologue ("segue") screen between the main menu and party creation
        // (MM6.EXE 0x452bd0). MM7 has no such screen, and its unconditional exitFsm() - the fallback
        // target, taken whenever the condition above it is false - leaves the fsm for party creation.
        .on("newGame").jumpTo([isMm6] { return isMm6; }, "Mm6Segue").exitFsm()
        .on("loadGame").jumpTo("LoadSlot")
        .on("quickLoadGame").exitFsm()
        .on("credits").jumpTo("Credits")
        .on("exitGame").exitFsm()

    .state<LoadSlotState>("LoadSlot")
        .on("slotConfirmed").exitFsm()
        .on("back").jumpTo("MainMenu");

    if (isMm6) {
        // Both of the prologue's buttons leave the fsm into Game::loop(), which then either runs the
        // creation screen (MENU_NEWGAME) or takes the default party as-is (MENU_QUICKSTART).
        builder
        .state<Mm6SegueState>("Mm6Segue")
            .on("createParty").exitFsm()
            .on("quickStart").exitFsm()
            .on("back").jumpTo("MainMenu")

        // MM6's credits are a movie clip, not MM7's scrolling text window (MM6.EXE 0x4A6C90).
        .state<VideoState>("Credits", VideoState::VIDEO_INTRO, "credits")
            .on("videoEnd").jumpTo("MainMenu");
    } else {
        builder
        .state<CreditsState>("Credits")
            .on("back").jumpTo("MainMenu");
    }
}

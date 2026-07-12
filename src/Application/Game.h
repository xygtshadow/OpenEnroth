#pragma once

#include <memory>
#include <string_view>

#include "Io/KeyboardInputHandler.h"
#include "Io/Mouse.h"

using Io::Mouse;

class Engine;
struct DecalBuilder;
class PlatformApplication;
class Platform;
class Menu;

class Game {
 public:
    Game(PlatformApplication *application, std::shared_ptr<GameConfig> config);
    ~Game();

    int run();

 private:
    /**
     * The legacy, pre-fsm game loop. Runs everything from party creation through to the game itself,
     * and returns when the game is over or when what comes next is an fsm state again.
     *
     * @return                      Name of the fsm state to restart the fsm at, or an empty string
     *                              to leave `run()`'s outer loop and shut the game down.
     */
    std::string_view loop();
    void processQueuedMessages();
    void gameLoop();
    void closeTargetedSpellWindow();
    void onEscape();
    void onPressSpace();

 private:
    PlatformApplication *_application = nullptr;
    std::shared_ptr<GameConfig> _config;
    std::unique_ptr<Menu> _menu;
    DecalBuilder *_decalBuilder = nullptr;
};

extern GraphicsImage *gamma_preview_image;  // 506E40

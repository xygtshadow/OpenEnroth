#pragma once

#include <string>
#include <optional>

#include "Application/Paths/GameVersion.h"

#include "Library/Logger/LogEnums.h"

struct GameStarterOptions {
    GameVersion gameVersion = GAME_VERSION_MM7; // Which game to run.
    std::string dataPath; // Path to game data.
    std::string userPath; // Path to user data.
    std::optional<LogLevel> logLevel; // Override log level.
    bool ramFsUserData = false; // Use in-memory file system for user data, don't read/write config & saves
                                // from/to disk. This also means that default config will be used.
    bool headless = false; // Run in headless mode.
    bool tracingRng = false; // Use tracing random engine?
    bool quickStart = false; // Skip whatever slow initialization that we have, including additional asset generation.
};

#pragma once

#include <vector>

#include "Library/Trace/EventTrace.h"

class GameConfig;
struct EventTraceGameState;

struct EngineTraceStateAccessor {
    static void prepareForRecording(GameConfig *config, ConfigPatch *patch);
    static void prepareForPlayback(GameConfig *config, const ConfigPatch &patch);

    /**
     * Applies the classic (original MM7) keybindings for the actions that the modern mm6-extra
     * scheme rebinds. Traces and scripted game tests replay raw keypresses that assume these
     * bindings; a trace that recorded custom bindings overrides them via its config patch.
     */
    static void applyClassicKeybindings(GameConfig *config);

    static EventTraceGameState makeGameState();
};

#pragma once

#include <vector>
#include <cstdint>

#include "Library/Geometry/Vec.h"

#include "DecorationEnums.h"

struct LevelDecoration {
    LevelDecoration();
    int GetGlobalEvent();
    bool IsInteractive();
    bool IsObeliskChestActive();

    DecorationId uDecorationDescID;
    LevelDecorationFlags uFlags;
    Vec3f vPosition;
    int32_t _yawAngle; // Only used for party spawn points, see `MapStartPoint`.
    uint16_t uCog;
    uint16_t uEventID;
    uint16_t uTriggerRange;
    int16_t eventVarId;
};

// Base offset between an interactive decoration's decorVars value and both the global.evt event
// it fires on click and the npctopic.txt row shown on hover. MM7 uses +380; MM6 uses +400
// (MM6.EXE click handlers add 0x190, and the hover handler reads npctopic row 400+value).
int decorationGlobalEventBase();

extern std::vector<LevelDecoration> pLevelDecorations;
extern std::vector<int> decorationsWithSound;
extern LevelDecoration *activeLevelDecoration;  // 5C3420

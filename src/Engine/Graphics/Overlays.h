#pragma once

#include <cstdint>
#include <array>
#include <vector>

#include "Engine/Pid.h"
#include "Engine/Time/Duration.h"

// Overlays are short sprite animations played either at a fixed screen position (spell fx over the character
// portraits, party-buff status icons) or attached to an actor in the 3D view (elemental impact sparks, monster
// self-buff fx). The mechanism is shared between MM6 and MM7, but MM7 shipped with every doverlay.bin entry
// pointing at the "null" sprite (and its screen-overlay spawn function stubbed out), so only MM6 data actually
// draws anything. All EXE address comments below refer to MM6.EXE unless marked otherwise.

// ActiveOverlay::flags bits, from the update loop at MM6.EXE 0x436797.
constexpr int16_t OVERLAY_FLAG_BUFF_OWNED = 0x1; // Slot outlives its animation and is freed by SpellBuff::Reset.
constexpr int16_t OVERLAY_FLAG_LOOPING = 0x2;    // Animation loops and the slot never expires on its own.

struct ActiveOverlay {
    ActiveOverlay();
    void Reset();

    int16_t target;  // Screen-anchor code (MM6 field_0): 100+char = cast fx over a portrait, 310/320/330/340/350+char
                     // = the five per-character persistent buff fx rows, 201/202/203 = fixed screen spots, 0 = none
                     // (actor-attached overlays are anchored via `pid` instead).
    int16_t indexToOverlayList;
    int16_t spriteFrameTime;
    int16_t animLength;
    int16_t screenSpaceX;
    int16_t screenSpaceY;
    Pid pid;
    int16_t flags;   // OVERLAY_FLAG_*. This is the field the MM7 sources called `projSize` - MM6.EXE reveals it's a
                     // flags word (0x4358F0 sets bit 0 for buff-owned anchors, 0x436797 branches on bits 0/1).
    int fpDamageMod; // Fixed-point (16.16) sprite scale multiplier; impact sparks scale with damage dealt.
};

struct ActiveOverlayList {
    void Reset();

    /**
     * Adds an overlay attached to a map object (in practice always an actor); it is drawn as a billboard tracking
     * the object each frame. MM7 0x4418B6 == MM6 0x435E00.
     *
     * @return                          1-based slot index (0 if the list is full), suitable for SpellBuff::Apply.
     */
    int addPidOverlay(int uOverlayID, Pid pid, Duration animLength, int fpDamageMod, int16_t flags);

    /**
     * Adds an overlay anchored at a fixed screen position selected by `target` (MM6 0x4358F0; stubbed out in
     * MM7.EXE, which is why MM7 has no portrait spell fx).
     *
     * @return                          1-based slot index (0 if the list is full), suitable for SpellBuff::Apply.
     */
    int addScreenOverlay(int uOverlayID, int target, Duration animLength, int fpDamageMod);

    /**
     * Advances animations and frees expired one-shot slots (the loop at MM6.EXE 0x436797).
     */
    void update(Duration dt);

    /**
     * Queues billboards for active actor-attached overlays. Call during 3D scene preparation, before billboards
     * are drawn (MM6.EXE 0x4361A0 pid branch).
     */
    void prepareBillboards();

    /**
     * Draws active screen-anchored overlays (2D phase; MM6.EXE 0x436010 and the pid==0 branch of 0x4361A0).
     */
    void drawScreenOverlays();

    std::array<ActiveOverlay, 50> pOverlays;
};

struct OverlayDesc {
    uint16_t uOverlayID = 0;
    uint16_t uOverlayType = 0; // 0 = "center" (anchor shifts down by half the sprite height), 2 = "transparent"
                               // (same shift), 1 = neither. All 96 MM6 entries are type 0.
    uint16_t uSpriteFramesetID = 0;
    int16_t spriteFramesetGroup = 0;
};

struct OverlayList {
    void InitializeSprites();

    std::vector<OverlayDesc> pOverlays;
};

/**
 * Draws the MM6 party-buff status icon row (y=254, right of the game viewport): one looping overlay sprite per
 * active party buff, from the table in MM6.EXE 0x4354E0. MM6 sessions only - the overlay ids involved are null
 * sprites in MM7 data.
 */
void drawMm6PartyBuffStatusOverlays();

extern ActiveOverlayList *pActiveOverlayList;
extern OverlayList *pOverlayList;

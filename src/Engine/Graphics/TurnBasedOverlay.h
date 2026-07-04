#pragma once

#include <array>

#include "Engine/Time/Duration.h"
#include "Engine/TurnEngine/TurnEngineEnums.h"

class GraphicsImage;

enum class TurnBasedOverlayState {
    TURN_BASED_OVERLAY_NONE, // No overlay.
    TURN_BASED_OVERLAY_INITIAL, // Initial phase - opening hand animation.
    TURN_BASED_OVERLAY_ATTACK, // Attack phase - open hand.
    TURN_BASED_OVERLAY_MOVEMENT, // Party movement - fist turning into an open hand, one finger at a time.
    TURN_BASED_OVERLAY_WAIT, // Monster turn - animated hourglass.
};
using enum TurnBasedOverlayState;

class TurnBasedOverlay {
 public:
    constexpr TurnBasedOverlay() = default;

    void loadIcons();

    void reset();

    void update(Duration dt, TurnEngineStep newStep);

    /**
     * @offset 0x00441964
     */
    void draw();

    TurnBasedOverlayState state() const {
        return _state;
    }

    /**
     * @return                              True when running on MM6 data, where the overlay is drawn from sprite
     *                                      framesets (hand / hourglass) instead of MM7's dift.bin icons.
     */
    bool usesMm6Sprites() const {
        return _mm6;
    }

 private:
    GraphicsImage *currentIcon() const;

 private:
    TurnBasedOverlayState _state = TURN_BASED_OVERLAY_NONE;
    Duration _currentTime; // Current animation progress.

    int _initialIconId = 0; // Opening hand animation.
    Duration _initialAnimationLength; // Duration of the opening hand animation.
    int _attackIconId = 0; // Open hand.
    std::array<int, 5> _movementIconIds = {{}}; // Fingers.
    int _waitIconId = 0; // Hourglass animation.

    // MM6 draws the indicator from two sprite framesets rather than dift.bin icons (MM6.EXE 0x435F03).
    bool _mm6 = false;
    int _mm6HandFramesetId = 0; // "newhand1" - shown while the party can act.
    int _mm6GlassFramesetId = 0; // "newglas1" - shown while monsters take their turn.
};

extern TurnBasedOverlay turnBasedOverlay;

#include "TurnBasedOverlay.h"

#include "Engine/Tables/IconFrameTable.h"
#include "Engine/Graphics/Image.h"
#include "Engine/Graphics/Renderer/Renderer.h"
#include "Engine/Graphics/Sprites.h"
#include "Engine/TurnEngine/TurnEngine.h"

#include "Library/Geometry/Rect.h"
#include "Library/Logger/Logger.h"

// Opening hand animation in vanilla was 320 ticks long (that's 2.5 seconds), but it was cut short and only the first
// 64 ticks were displayed. We fixed it, but then the animation ended up being way too slow, thus we've introduced the
// acceleration factor, resulting in an animation that's ~107 ticks long (5/6 of a second).
//
// Note that turn-based combat in vanilla starts with a 0.5s monster turn - supposedly to let the monsters end their
// current actions. The same timer was used for this turn and for the opening hand animation, and this is why the
// animation was cut short. We're using separate timers in OE.
static constexpr int TURN_BASED_INITIAL_ACCELERATION = 3;

TurnBasedOverlay turnBasedOverlay;

void TurnBasedOverlay::loadIcons() {
    _initialIconId = pIconsFrameTable->animationId("turnstart");
    if (_initialIconId == -1) {
        // MM6's icon frame table (dift.bin) has no turn-based combat animations under any name. MM6 instead
        // draws the indicator from two sprite framesets - a hand while the party can act and an hourglass
        // while monsters take their turn - at (444, 326) (MM6.EXE 0x435F03, framesets loaded at 0x42A610).
        _attackIconId = -1;
        _waitIconId = -1;
        _movementIconIds.fill(-1);
        _mm6HandFramesetId = pSpriteFrameTable->FastFindSprite("newhand1");
        _mm6GlassFramesetId = pSpriteFrameTable->FastFindSprite("newglas1");
        _mm6 = _mm6HandFramesetId > 0 && _mm6GlassFramesetId > 0;
        if (_mm6) {
            pSpriteFrameTable->InitializeSprite(_mm6HandFramesetId);
            pSpriteFrameTable->InitializeSprite(_mm6GlassFramesetId);
        } else {
            logger->warning("MM6 turn-based overlay sprites (newhand1/newglas1) are missing - the turn-based "
                            "overlay will not be drawn.");
        }
        return;
    }

    _initialAnimationLength = pIconsFrameTable->animationLength(_initialIconId);
    _attackIconId = pIconsFrameTable->animationId("turnstop");
    _waitIconId = pIconsFrameTable->animationId("turnhour");

    for (size_t i = 0; i < _movementIconIds.size(); ++i)
        _movementIconIds[i] = pIconsFrameTable->animationId(fmt::format("turn{}", i));
}

void TurnBasedOverlay::reset() {
    _state = TURN_BASED_OVERLAY_NONE;
}

void TurnBasedOverlay::update(Duration dt, TurnEngineStep newStep) {
    if (_mm6) {
        // MM6's indicator has no opening-hand phase; it toggles hand/hourglass directly with the turn stage.
        // Advance the freely-looping animation time (GetFrame wraps it modulo the frameset length, so it
        // never needs resetting), then map the stage: monsters' turn = hourglass, party's turn = hand.
        if (newStep == TE_NONE) {
            _state = TURN_BASED_OVERLAY_NONE;
            return;
        }
        _currentTime += dt;
        _state = newStep == TE_WAIT   ? TURN_BASED_OVERLAY_WAIT
               : newStep == TE_ATTACK ? TURN_BASED_OVERLAY_ATTACK
                                      : TURN_BASED_OVERLAY_MOVEMENT;
        return;
    }

    if (_initialIconId == -1)
        return; // No overlay icons at all, stay in TURN_BASED_OVERLAY_NONE so draw() never dereferences them.

    if (newStep == TE_NONE) {
        _state = TURN_BASED_OVERLAY_NONE;
        return;
    }

    if (_state == TURN_BASED_OVERLAY_NONE) {
        assert(newStep == TE_WAIT);
        _state = TURN_BASED_OVERLAY_INITIAL;
        _currentTime = 0_ticks;
        return;
    }

    if (_state == TURN_BASED_OVERLAY_INITIAL) {
        _currentTime += dt * TURN_BASED_INITIAL_ACCELERATION;
        if (_currentTime < _initialAnimationLength)
            return;
    }

    switch (newStep) {
    case TE_WAIT:
        if (_state != TURN_BASED_OVERLAY_WAIT) {
            _currentTime = 0_ticks;
        } else {
            _currentTime += dt;
        }
        _state = TURN_BASED_OVERLAY_WAIT;
        break;
    case TE_MOVEMENT:
        _state = TURN_BASED_OVERLAY_MOVEMENT;
        break;
    case TE_ATTACK:
        _state = TURN_BASED_OVERLAY_ATTACK;
        break;
    default:
        assert(false);
        break;
    }
}

void TurnBasedOverlay::draw() {
    if (_state == TURN_BASED_OVERLAY_NONE)
        return;

    if (_mm6) {
        // Draw the current frame of the hand (party's turn) or hourglass (monsters' turn) frameset at the
        // MM6 anchor (444, 326).
        int framesetId = _state == TURN_BASED_OVERLAY_WAIT ? _mm6GlassFramesetId : _mm6HandFramesetId;
        SpriteFrame *frame = pSpriteFrameTable->GetFrame(framesetId, _currentTime);
        if (!frame || !frame->sprites[0] || !frame->sprites[0]->texture)
            return;
        Sprite *sprite = frame->sprites[0];
        render->DrawImage(sprite->texture, Recti(444, 326, sprite->uWidth, sprite->uHeight), frame->paletteId);
        return;
    }

    render->DrawQuad2D(currentIcon(), {394, 288});
}

GraphicsImage *TurnBasedOverlay::currentIcon() const {
    switch (_state) {
    default:
        assert(false); // TURN_BASED_OVERLAY_NONE is expected to be checked up the stack.
        return nullptr;
    case TURN_BASED_OVERLAY_INITIAL:
        return pIconsFrameTable->animationFrame(_initialIconId, _currentTime);
    case TURN_BASED_OVERLAY_ATTACK:
        return pIconsFrameTable->animationFrame(_attackIconId, 0_ticks);
    case TURN_BASED_OVERLAY_MOVEMENT:
        return pIconsFrameTable->animationFrame(_movementIconIds[5 - pTurnEngine->uActionPointsLeft / 26], 0_ticks); // TODO(captainurist): get rid of this dependency.
    case TURN_BASED_OVERLAY_WAIT:
        return pIconsFrameTable->animationFrame(_waitIconId, _currentTime);
    }
}

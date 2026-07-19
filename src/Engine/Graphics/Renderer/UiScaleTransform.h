#pragma once

#include <algorithm>
#include <cmath>

#include "Library/Geometry/Point.h"
#include "Library/Geometry/Rect.h"
#include "Library/Geometry/Size.h"

/**
 * Maps the virtual 640x480 UI space onto a window of arbitrary size: uniform fractional scale,
 * aspect preserved, centered (pillarbox/letterbox). Scale never drops below 1 (undersized windows
 * crop around the center). Pure math, deliberately free of any renderer dependencies.
 *
 * Note that toDevice() and toVirtual() are intentionally NOT inverses at fractional scales — see
 * their individual docs for the rounding contracts.
 */
struct UiScaleTransform {
    Sizei virtualSize = {640, 480};
    Sizei deviceSize = {640, 480};
    float scale = 1.0f;
    Pointi offset = {0, 0}; // Device position of virtual (0,0); negative when cropping.

    /**
     * @param window        Device size of the target window.
     * @param virtualSize   Size of the virtual UI space.
     * @return              Transform with the largest aspect-preserving scale that fits the window,
     *                      clamped to a minimum of 1, and centered — pillarbox/letterbox bars when
     *                      the window is oversized, a centered crop (negative offset) when it's
     *                      undersized. The scaled frame size is rounded to the nearest pixel, see
     *                      deviceRect().
     */
    static UiScaleTransform forWindow(Sizei window, Sizei virtualSize = {640, 480}) {
        UiScaleTransform result;
        result.virtualSize = virtualSize;
        result.deviceSize = window;
        result.scale = std::max(1.0f, std::min(window.w / (float)virtualSize.w, window.h / (float)virtualSize.h));
        Sizei scaled = {(int)std::lround(virtualSize.w * result.scale), (int)std::lround(virtualSize.h * result.scale)};
        result.offset = {(window.w - scaled.w) / 2, (window.h - scaled.h) / 2};
        return result;
    }

    /**
     * Virtual to device, rounding to the nearest device pixel — the placement direction, for
     * positioning virtual points on screen.
     *
     * Intentionally NOT an exact inverse of toVirtual() at fractional scales: e.g. at 2.25x,
     * toVirtual(toDevice({1, 1})) == {0, 0}. The two directions serve different contracts
     * (placement vs span containment) rather than forming a bijection.
     */
    [[nodiscard]] Pointi toDevice(Pointi v) const {
        return {offset.x + (int)std::lround(v.x * scale), offset.y + (int)std::lround(v.y * scale)};
    }

    /**
     * Device to virtual: floors into the virtual pixel whose device span contains the point, then
     * clamps into virtual bounds — the mouse convention, so clicks inside the pillarbox/letterbox
     * bars resolve to the nearest edge pixel. See toDevice() for why the two directions are not
     * inverses at fractional scales.
     */
    [[nodiscard]] Pointi toVirtual(Pointi d) const {
        int x = (int)std::floor((d.x - offset.x) / scale);
        int y = (int)std::floor((d.y - offset.y) / scale);
        return {std::clamp(x, 0, virtualSize.w - 1), std::clamp(y, 0, virtualSize.h - 1)};
    }

    /**
     * Floors the origin and ceils the far edge: callers use this for the 3D viewport / scissor so
     * the scaled region always covers the exact edge (HUD frame art overdraws the seam).
     */
    [[nodiscard]] Recti toDeviceOutward(Recti v) const {
        int x0 = offset.x + (int)std::floor(v.x * scale);
        int y0 = offset.y + (int)std::floor(v.y * scale);
        int x1 = offset.x + (int)std::ceil((v.x + v.w) * scale);
        int y1 = offset.y + (int)std::ceil((v.y + v.h) * scale);
        return {x0, y0, x1 - x0, y1 - y0};
    }

    /**
     * @return  The exact scaled frame that forWindow() centered inside the window — offset plus
     *          the nearest-rounded scaled virtual size. Use this for the full-frame viewport;
     *          toDeviceOutward({0, 0, w, h}) ceils the far edge instead and can over-cover the
     *          frame by a pixel at fractional scales, stretching the UI into the pillarbox bars.
     */
    [[nodiscard]] Recti deviceRect() const {
        return Recti(offset, Sizei((int)std::lround(virtualSize.w * scale), (int)std::lround(virtualSize.h * scale)));
    }
};

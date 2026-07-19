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
 */
struct UiScaleTransform {
    Sizei virtualSize = {640, 480};
    Sizei deviceSize = {640, 480};
    float scale = 1.0f;
    Pointi offset = {0, 0}; // Device position of virtual (0,0); negative when cropping.

    static UiScaleTransform forWindow(Sizei window, Sizei virtualSize = {640, 480}) {
        UiScaleTransform result;
        result.virtualSize = virtualSize;
        result.deviceSize = window;
        result.scale = std::max(1.0f, std::min(window.w / (float)virtualSize.w, window.h / (float)virtualSize.h));
        Sizei scaled = {(int)std::lround(virtualSize.w * result.scale), (int)std::lround(virtualSize.h * result.scale)};
        result.offset = {(window.w - scaled.w) / 2, (window.h - scaled.h) / 2};
        return result;
    }

    [[nodiscard]] Pointi toDevice(Pointi v) const {
        return {offset.x + (int)std::lround(v.x * scale), offset.y + (int)std::lround(v.y * scale)};
    }

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
};

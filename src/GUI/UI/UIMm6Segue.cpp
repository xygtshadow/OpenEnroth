#include "GUI/UI/UIMm6Segue.h"

#include <algorithm>

// MM6's prologue crawl is two phases (menu loop @0x453193-0x453223). Both of the loop's deadlines
// are seeded with `start + 0x7148` (@0x452fba) and the pan is gated on a strict `jbe`, so nothing
// moves for the first 29s. The deadlines are compared against KERNEL32!GetTickCount (IAT slot
// 0x4b9080) - that's what pins the units to milliseconds: 0x7148 = 29000ms, 0x32 = 50ms.
static constexpr int64_t kMm6SegueHoldMs = 29000;
// After the hold the view pans down one pixel per step (@0x453217).
static constexpr int64_t kMm6SegueStepMs = 50;
// ...until it hits the bottom of the image (@0x4531fd). This bound is an asset property, not an EXE
// literal: the original computes it at runtime from the height of the loaded PCX (`movsx ecx,
// word [esp+0x32]; sub ecx, 0x140`). seg_scrl.pcx is 900 tall and the window it shows through is
// 320, so the pan stops at 580 - parking the gate scene at the bottom of seg_scrl.pcx in the
// window of the static segue_bg.pcx frame.
static constexpr int kMm6SegueMaxScrollY = 580;

// The original re-arms its deadline from the current time after each pixel (`now + 0x32`
// @0x453211) and moves at most one pixel per frame, so its pan actually drifts with frame time.
// This closed form is the idealization of that - frame-rate independent, and testable, which a
// 58-second crawl driven through the engine is not.
int mm6SegueScrollY(int64_t elapsedMs) {
    if (elapsedMs < kMm6SegueHoldMs)
        return 0;
    return static_cast<int>(std::min<int64_t>((elapsedMs - kMm6SegueHoldMs) / kMm6SegueStepMs, kMm6SegueMaxScrollY));
}

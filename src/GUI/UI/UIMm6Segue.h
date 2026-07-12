#pragma once

#include <cstdint>

/**
 * Vertical scroll of MM6's new-game prologue crawl - the narrative text that pans over the sky
 * down to the New Sorpigal gate, on the screen that offers Create Party / Quick Start.
 *
 * @param elapsedMs                 Milliseconds since the prologue screen opened.
 * @return                          Row of `seg_scrl.pcx` to show at the top of the viewport, in
 *                                  `[0, 580]`.
 */
int mm6SegueScrollY(int64_t elapsedMs);

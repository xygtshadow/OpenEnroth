#pragma once

#include "Image.h"
#include "Palette.h"

RgbaImage makeRgbaImage(GrayscaleImageView indexedImage, const Palette &palette);

RgbaImage flipVertically(RgbaImageView image);

/**
 * Samples the given region of the source image down (or up) to the requested size, picking the
 * nearest source pixel at each destination pixel's center (nearest-neighbor scaling).
 *
 * Sample points are clamped into the image bounds, so @p region may extend past the image edges -
 * e.g. an outward-rounded device rect that overshoots the window by a pixel, or the frame of a
 * centered crop that lies partly outside an undersized window.
 *
 * @param image                         Image to sample from.
 * @param region                        Region of @p image to sample, must be non-empty.
 * @param size                          Size of the resulting image.
 * @return                              Sampled image of the requested size.
 */
RgbaImage sampleRegion(RgbaImageView image, const Recti &region, Sizei size);

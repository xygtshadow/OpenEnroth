#include "ImageFunctions.h"

#include <algorithm>
#include <cassert>

RgbaImage makeRgbaImage(GrayscaleImageView indexedImage, const Palette &palette) {
    if (!indexedImage)
        return RgbaImage();

    RgbaImage result = RgbaImage::uninitialized(indexedImage.width(), indexedImage.height());
    auto srcPixels = indexedImage.pixels();
    auto dstPixels = result.pixels();
    assert(srcPixels.size() == dstPixels.size());

    for (size_t i = 0, size = srcPixels.size(); i < size; i++)
        dstPixels[i] = palette.colors[srcPixels[i]];

    return result;
}

RgbaImage flipVertically(RgbaImageView image) {
    if (!image)
        return RgbaImage();

    RgbaImage result = RgbaImage::uninitialized(image.width(), image.height());
    for (size_t y = 0, h = image.height(); y < h; y++)
        memcpy(result[h - y - 1].data(), image[y].data(), image[y].size_bytes());
    return result;
}

RgbaImage sampleRegion(RgbaImageView image, const Recti &region, Sizei size) {
    if (!image || size.w <= 0 || size.h <= 0)
        return RgbaImage();
    assert(region.w > 0 && region.h > 0);

    RgbaImage result = RgbaImage::uninitialized(size.w, size.h);
    for (int y = 0; y < size.h; y++) {
        int srcY = std::clamp(region.y + static_cast<int>((y + 0.5f) * region.h / size.h), 0, image.height() - 1);
        for (int x = 0; x < size.w; x++) {
            int srcX = std::clamp(region.x + static_cast<int>((x + 0.5f) * region.w / size.w), 0, image.width() - 1);
            result[y][x] = image[srcY][srcX];
        }
    }
    return result;
}

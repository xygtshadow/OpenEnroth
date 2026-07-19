#include "Testing/Unit/UnitTest.h"

#include "Library/Image/ImageFunctions.h"

UNIT_TEST(ImageFunctions, SampleRegionIdentity) {
    RgbaImage src = RgbaImage::uninitialized(4, 4);
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++)
            src[y][x] = Color(x * 10, y * 10, 0);

    RgbaImage result = sampleRegion(src, Recti(0, 0, 4, 4), Sizei(4, 4));
    ASSERT_EQ(result.size(), Sizei(4, 4));
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++)
            EXPECT_EQ(result[y][x], src[y][x]);
}

UNIT_TEST(ImageFunctions, SampleRegionDownsample) {
    // 4x4 image with distinct quadrant colors: nearest-neighbor center sampling must pick each
    // quadrant's own pixels, never blend or slip into a neighboring quadrant.
    Color quadrants[2][2] = {{Color(255, 0, 0), Color(0, 255, 0)},
                             {Color(0, 0, 255), Color(255, 255, 0)}};
    RgbaImage src = RgbaImage::uninitialized(4, 4);
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++)
            src[y][x] = quadrants[y / 2][x / 2];

    RgbaImage result = sampleRegion(src, Recti(0, 0, 4, 4), Sizei(2, 2));
    ASSERT_EQ(result.size(), Sizei(2, 2));
    for (int y = 0; y < 2; y++)
        for (int x = 0; x < 2; x++)
            EXPECT_EQ(result[y][x], quadrants[y][x]);
}

UNIT_TEST(ImageFunctions, SampleRegionSubRect) {
    // Sampling a sub-rect at 1:1 is a crop.
    RgbaImage src = RgbaImage::uninitialized(4, 4);
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++)
            src[y][x] = Color(x * 10, y * 10, 0);

    RgbaImage result = sampleRegion(src, Recti(1, 2, 2, 2), Sizei(2, 2));
    ASSERT_EQ(result.size(), Sizei(2, 2));
    for (int y = 0; y < 2; y++)
        for (int x = 0; x < 2; x++)
            EXPECT_EQ(result[y][x], src[y + 2][x + 1]);
}

UNIT_TEST(ImageFunctions, SampleRegionClampsOutOfBoundsRegion) {
    // The region may extend past the image edges - outward-rounded device rects overshoot the
    // window by a pixel, and an undersized window's centered crop puts parts of the frame outside
    // it. Out-of-bounds sample points clamp to the nearest edge pixel instead of crashing.
    RgbaImage src = RgbaImage::uninitialized(2, 2);
    src[0][0] = Color(1, 0, 0);
    src[0][1] = Color(2, 0, 0);
    src[1][0] = Color(3, 0, 0);
    src[1][1] = Color(4, 0, 0);

    RgbaImage result = sampleRegion(src, Recti(-2, -2, 6, 6), Sizei(3, 3));
    ASSERT_EQ(result.size(), Sizei(3, 3));
    EXPECT_EQ(result[0][0], src[0][0]); // Sample point (-1, -1) clamps to (0, 0).
    EXPECT_EQ(result[2][2], src[1][1]); // Sample point (3, 3) clamps to (1, 1).
    EXPECT_EQ(result[1][1], src[1][1]); // Sample point (1, 1) is in bounds.
}

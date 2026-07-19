#include "Testing/Unit/UnitTest.h"

#include "Engine/Graphics/Renderer/UiScaleTransform.h"

#include "Library/Geometry/Point.h"
#include "Library/Geometry/Rect.h"

UNIT_TEST(UiScaleTransform, ForWindow) {
    UiScaleTransform t = UiScaleTransform::forWindow({640, 480});
    EXPECT_EQ(t.scale, 1.0f);
    EXPECT_EQ(t.offset, Pointi(0, 0));

    t = UiScaleTransform::forWindow({1920, 1080}); // 2.25x, pillarbox.
    EXPECT_EQ(t.scale, 2.25f);
    EXPECT_EQ(t.offset, Pointi(240, 0));

    t = UiScaleTransform::forWindow({2560, 1440}); // Exact 3x.
    EXPECT_EQ(t.scale, 3.0f);
    EXPECT_EQ(t.offset, Pointi(320, 0));

    t = UiScaleTransform::forWindow({1280, 1024}); // 2x, letterbox: bars above and below.
    EXPECT_EQ(t.scale, 2.0f);
    EXPECT_EQ(t.offset, Pointi(0, 32));

    t = UiScaleTransform::forWindow({320, 240}); // Undersized: clamp to 1, centered crop.
    EXPECT_EQ(t.scale, 1.0f);
    EXPECT_EQ(t.offset, Pointi(-160, -120));
}

UNIT_TEST(UiScaleTransform, PointRoundtrip) {
    UiScaleTransform t = UiScaleTransform::forWindow({1920, 1080});

    // The roundtrip holds for these specific points: exact products (468 * 2.25 = 1053,
    // 8 * 2.25 = 18) and .75-fraction points whose lround lands inside the same virtual pixel's
    // device span (639 * 2.25 = 1437.75 -> 1438). It does NOT hold for arbitrary points at
    // fractional scales — toDevice rounds to nearest while toVirtual floors, so don't extend this
    // loop with arbitrary values (see the counterexample below).
    for (Pointi p : {Pointi(0, 0), Pointi(639, 479), Pointi(468, 352), Pointi(8, 8)})
        EXPECT_EQ(t.toVirtual(t.toDevice(p)), p);

    // The documented counterexample: toDevice({1, 1}) rounds 2.25 down to device pixel 2, which
    // lies in virtual pixel 0's span [0, 2.25).
    EXPECT_EQ(t.toVirtual(t.toDevice(Pointi(1, 1))), Pointi(0, 0));

    // Mouse clamps into virtual bounds:
    EXPECT_EQ(t.toVirtual(Pointi(0, 0)), Pointi(0, 0)); // Inside left bar.
    EXPECT_EQ(t.toVirtual(Pointi(1919, 1079)), Pointi(639, 479)); // Inside right bar.
}

UNIT_TEST(UiScaleTransform, RectOutward) {
    UiScaleTransform t = UiScaleTransform::forWindow({1920, 1080});

    // Classic 3D viewport hole at 2.25x / offset (240, 0). Everything is deterministic:
    // x0 = 240 + floor(8 * 2.25 = 18) = 258, y0 = floor(18) = 18,
    // x1 = 240 + ceil(469 * 2.25 = 1055.25) = 1296 -> w = 1038,
    // y1 = ceil(353 * 2.25 = 794.25) = 795 -> h = 777.
    EXPECT_EQ(t.toDeviceOutward(Recti(8, 8, 461, 345)), Recti(258, 18, 1038, 777));
}

UNIT_TEST(UiScaleTransform, DeviceRect) {
    UiScaleTransform t = UiScaleTransform::forWindow({1920, 1080});
    EXPECT_EQ(t.deviceRect(), Recti(240, 0, 1440, 1080));

    // 900x640: scale = 640/480 = 1.333..., scaled width = lround(853.33) = 853, but the outward
    // full-frame rect ceils to 854 — using toDeviceOutward for glViewport here would stretch the
    // UI and bleed a pixel into the pillarbox bar, which is why the full-frame case must use
    // deviceRect() instead.
    t = UiScaleTransform::forWindow({900, 640});
    EXPECT_EQ(t.deviceRect().w, 853);
    EXPECT_EQ(t.toDeviceOutward(Recti(0, 0, 640, 480)).w, 854);
}

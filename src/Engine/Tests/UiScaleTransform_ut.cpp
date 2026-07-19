#include <cmath>

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

    t = UiScaleTransform::forWindow({320, 240}); // Undersized: clamp to 1, centered crop.
    EXPECT_EQ(t.scale, 1.0f);
    EXPECT_EQ(t.offset, Pointi(-160, -120));
}

UNIT_TEST(UiScaleTransform, PointRoundtrip) {
    UiScaleTransform t = UiScaleTransform::forWindow({1920, 1080});
    for (Pointi p : {Pointi(0, 0), Pointi(639, 479), Pointi(468, 352), Pointi(8, 8)})
        EXPECT_EQ(t.toVirtual(t.toDevice(p)), p);

    // Mouse clamps into virtual bounds:
    EXPECT_EQ(t.toVirtual(Pointi(0, 0)), Pointi(0, 0)); // Inside left bar.
    EXPECT_EQ(t.toVirtual(Pointi(1919, 1079)), Pointi(639, 479)); // Inside right bar.
}

UNIT_TEST(UiScaleTransform, RectOutward) {
    UiScaleTransform t = UiScaleTransform::forWindow({1920, 1080});
    Recti vp = t.toDeviceOutward(Recti(8, 8, 461, 345)); // Classic 3D viewport hole.
    EXPECT_LE(vp.x, 240 + (int)(8 * 2.25f)); // Floor: never right of the exact edge.
    EXPECT_GE(vp.x + vp.w, 240 + (int)std::ceil((8 + 461) * 2.25f)); // Ceil: covers the far edge.
}

#include <cmath>
#include <cstdlib>
#include <limits>

#include "engine/visualization/Viewport2D.h"

namespace
{

constexpr float Tolerance = 0.0001f;

[[nodiscard]] bool NearlyEqual(float firstValue, float secondValue, float tolerance = Tolerance)
{
    return std::fabs(firstValue - secondValue) <= tolerance;
}

[[nodiscard]] bool PointsNearlyMatch(greenfield::Vec2 firstPoint, greenfield::Vec2 secondPoint, float tolerance = Tolerance)
{
    return NearlyEqual(firstPoint.x, secondPoint.x, tolerance) && NearlyEqual(firstPoint.y, secondPoint.y, tolerance);
}

[[nodiscard]] greenfield::Viewport2D MakeDefaultViewport()
{
    using namespace greenfield;

    return Viewport2D{
        Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{800.0f, 600.0f}},
        Vec2{10.0f, 20.0f},
        2.0f,
    };
}

[[nodiscard]] bool TestWorldCenterMapsToScreenCenter()
{
    const greenfield::Viewport2D viewport = MakeDefaultViewport();

    return PointsNearlyMatch(viewport.WorldToScreen(viewport.WorldCenter()), greenfield::Vec2{400.0f, 300.0f});
}

[[nodiscard]] bool TestPositiveWorldYMapsUpwardOnScreen()
{
    using namespace greenfield;

    const Viewport2D viewport = MakeDefaultViewport();
    const Vec2 centerScreenPoint = viewport.WorldToScreen(viewport.WorldCenter());
    const Vec2 higherWorldScreenPoint = viewport.WorldToScreen(Vec2{10.0f, 30.0f});

    return higherWorldScreenPoint.y < centerScreenPoint.y;
}

[[nodiscard]] bool TestRoundTrips()
{
    using namespace greenfield;

    const Viewport2D viewport = MakeDefaultViewport();
    const Vec2 worldPoint{123.5f, -45.25f};
    const Vec2 screenPoint{321.0f, 456.0f};

    return PointsNearlyMatch(viewport.ScreenToWorld(viewport.WorldToScreen(worldPoint)), worldPoint) &&
           PointsNearlyMatch(viewport.WorldToScreen(viewport.ScreenToWorld(screenPoint)), screenPoint);
}

[[nodiscard]] bool TestNonZeroScreenBoundsOrigin()
{
    using namespace greenfield;

    const Viewport2D viewport{
        Rect{.position = Vec2{100.0f, 50.0f}, .size = Vec2{320.0f, 240.0f}},
        Vec2{-4.0f, 6.0f},
        4.0f,
    };

    return PointsNearlyMatch(viewport.WorldToScreen(viewport.WorldCenter()), Vec2{260.0f, 170.0f}) &&
           PointsNearlyMatch(viewport.ScreenToWorld(Vec2{260.0f, 170.0f}), viewport.WorldCenter());
}

[[nodiscard]] bool TestDirectManipulationPan()
{
    using namespace greenfield;

    Viewport2D viewport = MakeDefaultViewport();
    const Vec2 worldPoint{15.0f, 25.0f};
    const Vec2 beforePan = viewport.WorldToScreen(worldPoint);

    viewport.PanByScreenDelta(Vec2{40.0f, 20.0f});

    return PointsNearlyMatch(viewport.WorldCenter(), Vec2{-10.0f, 30.0f}) &&
           PointsNearlyMatch(viewport.WorldToScreen(worldPoint), Vec2{beforePan.x + 40.0f, beforePan.y + 20.0f});
}

[[nodiscard]] bool TestAnchoredZoomAtViewportCenter()
{
    using namespace greenfield;

    Viewport2D viewport = MakeDefaultViewport();
    const Vec2 anchor{400.0f, 300.0f};
    const Vec2 worldUnderAnchor = viewport.ScreenToWorld(anchor);

    viewport.ZoomBy(2.0f, anchor);

    return NearlyEqual(viewport.PixelsPerWorldUnit(), 4.0f) && PointsNearlyMatch(viewport.WorldCenter(), worldUnderAnchor) &&
           PointsNearlyMatch(viewport.ScreenToWorld(anchor), worldUnderAnchor);
}

[[nodiscard]] bool TestAnchoredZoomAwayFromViewportCenter()
{
    using namespace greenfield;

    Viewport2D viewport = MakeDefaultViewport();
    const Vec2 anchor{500.0f, 350.0f};
    const Vec2 worldUnderAnchor = viewport.ScreenToWorld(anchor);

    viewport.ZoomBy(2.5f, anchor);

    return NearlyEqual(viewport.PixelsPerWorldUnit(), 5.0f) &&
           PointsNearlyMatch(viewport.ScreenToWorld(anchor), worldUnderAnchor) &&
           !PointsNearlyMatch(viewport.WorldCenter(), Vec2{10.0f, 20.0f});
}

[[nodiscard]] bool TestRepeatedPanAndZoomStayConsistent()
{
    using namespace greenfield;

    Viewport2D viewport = MakeDefaultViewport();

    viewport.PanByScreenDelta(Vec2{30.0f, -15.0f});
    viewport.ZoomBy(1.75f, Vec2{515.0f, 240.0f});
    viewport.PanByScreenDelta(Vec2{-10.0f, 55.0f});
    viewport.ZoomBy(0.5f, Vec2{120.0f, 500.0f});

    const Vec2 worldPoint{-30.0f, 42.5f};
    const Vec2 screenPoint = viewport.WorldToScreen(worldPoint);

    return PointsNearlyMatch(viewport.ScreenToWorld(screenPoint), worldPoint, 0.0002f);
}

[[nodiscard]] bool TestReversedScreenBoundsAreNormalized()
{
    using namespace greenfield;

    const Viewport2D viewport{
        Rect{.position = Vec2{500.0f, 400.0f}, .size = Vec2{-200.0f, -100.0f}},
        Vec2{},
        1.0f,
    };

    const Rect normalizedBounds = viewport.ScreenBounds();

    return PointsNearlyMatch(normalizedBounds.position, Vec2{300.0f, 300.0f}) &&
           PointsNearlyMatch(normalizedBounds.size, Vec2{200.0f, 100.0f}) &&
           PointsNearlyMatch(viewport.WorldToScreen(Vec2{}), Vec2{400.0f, 350.0f});
}

[[nodiscard]] bool TestInvalidScaleAndZoomCannotCorruptState()
{
    using namespace greenfield;

    Viewport2D viewport = MakeDefaultViewport();
    const Vec2 originalCenter = viewport.WorldCenter();
    const float originalScale = viewport.PixelsPerWorldUnit();

    viewport.SetPixelsPerWorldUnit(0.0f);
    viewport.SetPixelsPerWorldUnit(-2.0f);
    viewport.SetPixelsPerWorldUnit(std::numeric_limits<float>::quiet_NaN());
    viewport.ZoomBy(0.0f, Vec2{400.0f, 300.0f});
    viewport.ZoomBy(-1.0f, Vec2{400.0f, 300.0f});
    viewport.ZoomBy(std::numeric_limits<float>::infinity(), Vec2{400.0f, 300.0f});

    return NearlyEqual(viewport.PixelsPerWorldUnit(), originalScale) && PointsNearlyMatch(viewport.WorldCenter(), originalCenter) &&
           PointsNearlyMatch(viewport.WorldToScreen(originalCenter), Vec2{400.0f, 300.0f});
}

} // namespace

int main()
{
    if (!TestWorldCenterMapsToScreenCenter() || !TestPositiveWorldYMapsUpwardOnScreen() || !TestRoundTrips() ||
        !TestNonZeroScreenBoundsOrigin() || !TestDirectManipulationPan() || !TestAnchoredZoomAtViewportCenter() ||
        !TestAnchoredZoomAwayFromViewportCenter() || !TestRepeatedPanAndZoomStayConsistent() ||
        !TestReversedScreenBoundsAreNormalized() || !TestInvalidScaleAndZoomCannotCorruptState())
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

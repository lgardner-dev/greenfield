#include <cstdlib>

#include "engine/ui/Layout.h"
#include "engine/ui/Style.h"
#include "tests/TestHelpers.h"

namespace
{

[[nodiscard]] greenfield::Rect MakeResponsiveBounds()
{
    return greenfield::Rect{
        .position = greenfield::Vec2{10.0f, 20.0f},
        .size = greenfield::Vec2{300.0f, 180.0f},
    };
}

[[nodiscard]] bool TestInsetAndClampHelpers()
{
    using namespace greenfield;
    using greenfield::tests::RectanglesMatch;

    const Rect bounds = MakeResponsiveBounds();
    const Vec2 clampedSize = ClampSize(Vec2{80.0f, 260.0f}, Vec2{100.0f, 120.0f}, Vec2{240.0f, 220.0f});

    return RectanglesMatch(InsetRectangle(bounds, 10.0f), Rect{.position = Vec2{20.0f, 30.0f}, .size = Vec2{280.0f, 160.0f}}) &&
           RectanglesMatch(InsetRectangle(bounds, EdgeInsets{.left = 8.0f, .top = 12.0f, .right = 18.0f, .bottom = 22.0f}),
                           Rect{.position = Vec2{18.0f, 32.0f}, .size = Vec2{274.0f, 146.0f}}) &&
           clampedSize.x == 100.0f && clampedSize.y == 220.0f;
}

[[nodiscard]] bool TestSplitHelpers()
{
    using namespace greenfield;
    using greenfield::tests::RectanglesMatch;

    const Rect bounds = MakeResponsiveBounds();
    const LayoutSplit percentageSplit = SplitRectangleHorizontallyByPercentage(bounds, 0.25f, 12.0f);
    const LayoutSplit fixedFlexibleSplit = SplitRectangleHorizontallyFixedFlexible(bounds, 96.0f, 14.0f, FixedRegion::Second);

    return RectanglesMatch(percentageSplit.first, Rect{.position = Vec2{10.0f, 20.0f}, .size = Vec2{72.0f, 180.0f}}) &&
           RectanglesMatch(percentageSplit.second, Rect{.position = Vec2{94.0f, 20.0f}, .size = Vec2{216.0f, 180.0f}}) &&
           RectanglesMatch(fixedFlexibleSplit.first, Rect{.position = Vec2{10.0f, 20.0f}, .size = Vec2{190.0f, 180.0f}}) &&
           RectanglesMatch(fixedFlexibleSplit.second, Rect{.position = Vec2{214.0f, 20.0f}, .size = Vec2{96.0f, 180.0f}});
}

[[nodiscard]] bool TestGridRegionHelpers()
{
    using namespace greenfield;
    using greenfield::tests::RectanglesMatch;

    const Rect bounds = MakeResponsiveBounds();
    return RectanglesMatch(CalculateColumnRegion(bounds, 1U, 3U, 15.0f), Rect{.position = Vec2{115.0f, 20.0f}, .size = Vec2{90.0f, 180.0f}}) &&
           RectanglesMatch(CalculateRowRegion(bounds, 2U, 3U, 15.0f), Rect{.position = Vec2{10.0f, 150.0f}, .size = Vec2{300.0f, 50.0f}});
}

[[nodiscard]] bool TestDefaultStyleRemainsUsable()
{
    const greenfield::Style style{};
    return style.accent.alpha > 0.0f && style.textPrimary.alpha > 0.0f && style.panelCornerRadius > 0.0f &&
           style.panelBorderThickness > 0.0f && style.panelBackground.alpha < 1.0f;
}

} // namespace

int main()
{
    if (!TestInsetAndClampHelpers() || !TestSplitHelpers() || !TestGridRegionHelpers() || !TestDefaultStyleRemainsUsable())
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

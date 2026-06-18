#include <cstdlib>

#include "engine/render/fast2d/Fast2DRenderer.h"
#include "tests/TestHelpers.h"

namespace
{

[[nodiscard]] bool PixelMatches(const greenfield::Fast2DRenderer& renderer, std::size_t x, std::size_t y,
                                const greenfield::Color& color)
{
    return greenfield::tests::ColorsMatch(renderer.RasterPixelAt(x, y), color);
}

[[nodiscard]] bool TestConstructionSucceeds()
{
    const greenfield::Fast2DRenderer renderer;
    return renderer.SubmittedCommandCount() == 0U && renderer.CompletedFrameCommandCount() == 0U;
}

[[nodiscard]] bool TestBeginFrameClearsSubmittedCommands()
{
    using namespace greenfield;

    Fast2DRenderer renderer;
    RenderCommandList renderCommands;
    renderCommands.AddFillRectangle(Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{10.0f, 10.0f}}, Color{});

    renderer.Submit(renderCommands);
    if (renderer.SubmittedCommandCount() != 1U)
    {
        return false;
    }

    renderer.BeginFrame();
    return renderer.SubmittedCommandCount() == 0U && renderer.PreparedFillOperationCount() == 0U;
}

[[nodiscard]] bool TestSubmitRecordsAndAppendsCommands()
{
    using namespace greenfield;

    Fast2DRenderer renderer;
    RenderCommandList firstCommands;
    RenderCommandList secondCommands;
    firstCommands.AddFillRectangle(Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{20.0f, 20.0f}}, Color{});
    secondCommands.AddText("Fast2D", Rect{.position = Vec2{2.0f, 4.0f}, .size = Vec2{80.0f, 20.0f}}, 14.0f, Color{});

    renderer.Submit(firstCommands);
    renderer.Submit(secondCommands);

    const auto submittedCommands = renderer.SubmittedCommands();
    return renderer.SubmittedCommandCount() == 2U && submittedCommands[0].type == RenderCommandType::FillRectangle &&
           submittedCommands[1].type == RenderCommandType::DrawText && submittedCommands[1].text == "Fast2D";
}

[[nodiscard]] bool TestFillRectangleIsPrepared()
{
    using namespace greenfield;
    using greenfield::tests::ColorsMatch;
    using greenfield::tests::RectanglesMatch;

    const Rect rectangle{.position = Vec2{3.0f, 4.0f}, .size = Vec2{30.0f, 40.0f}};
    const Color fillColor{0.1f, 0.2f, 0.3f, 0.4f};
    const Color borderColor{0.5f, 0.6f, 0.7f, 0.8f};

    Fast2DRenderer renderer;
    RenderCommandList renderCommands;
    renderCommands.AddFillRectangle(rectangle, fillColor, 5.0f, borderColor, 2.0f);

    renderer.Submit(renderCommands);

    const auto fillOperations = renderer.PreparedFillOperations();
    return renderer.PreparedFillOperationCount() == 1U && RectanglesMatch(fillOperations[0].rectangle, rectangle) &&
           ColorsMatch(fillOperations[0].fillColor, fillColor) && fillOperations[0].cornerRadius == 5.0f &&
           ColorsMatch(fillOperations[0].borderColor, borderColor) && fillOperations[0].borderThickness == 2.0f &&
           !fillOperations[0].hasClip;
}

[[nodiscard]] bool TestMultipleFillRectanglesArePreparedInOrder()
{
    using namespace greenfield;
    using greenfield::tests::RectanglesMatch;

    const Rect firstRectangle{.position = Vec2{0.0f, 0.0f}, .size = Vec2{10.0f, 10.0f}};
    const Rect secondRectangle{.position = Vec2{20.0f, 0.0f}, .size = Vec2{30.0f, 10.0f}};

    Fast2DRenderer renderer;
    RenderCommandList renderCommands;
    renderCommands.AddFillRectangle(firstRectangle, Color{});
    renderCommands.AddFillRectangle(secondRectangle, Color{});

    renderer.Submit(renderCommands);

    const auto fillOperations = renderer.PreparedFillOperations();
    return renderer.PreparedFillOperationCount() == 2U &&
           RectanglesMatch(fillOperations[0].rectangle, firstRectangle) &&
           RectanglesMatch(fillOperations[1].rectangle, secondRectangle);
}

[[nodiscard]] bool TestClipStackAffectsPreparedFillOperations()
{
    using namespace greenfield;
    using greenfield::tests::RectanglesMatch;

    const Rect firstClip{.position = Vec2{1.0f, 2.0f}, .size = Vec2{30.0f, 40.0f}};
    const Rect secondClip{.position = Vec2{5.0f, 6.0f}, .size = Vec2{10.0f, 12.0f}};

    Fast2DRenderer renderer;
    RenderCommandList renderCommands;
    renderCommands.PushClip(firstClip);
    renderCommands.AddFillRectangle(Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{10.0f, 10.0f}}, Color{});
    renderCommands.PushClip(secondClip);
    renderCommands.AddFillRectangle(Rect{.position = Vec2{12.0f, 0.0f}, .size = Vec2{10.0f, 10.0f}}, Color{});
    renderCommands.PopClip();
    renderCommands.AddFillRectangle(Rect{.position = Vec2{24.0f, 0.0f}, .size = Vec2{10.0f, 10.0f}}, Color{});
    renderCommands.PopClip();
    renderCommands.AddFillRectangle(Rect{.position = Vec2{36.0f, 0.0f}, .size = Vec2{10.0f, 10.0f}}, Color{});

    renderer.Submit(renderCommands);

    const auto fillOperations = renderer.PreparedFillOperations();
    return renderer.PreparedFillOperationCount() == 4U && fillOperations[0].hasClip &&
           RectanglesMatch(fillOperations[0].clipRectangle, firstClip) && fillOperations[1].hasClip &&
           RectanglesMatch(fillOperations[1].clipRectangle, secondClip) && fillOperations[2].hasClip &&
           RectanglesMatch(fillOperations[2].clipRectangle, firstClip) && !fillOperations[3].hasClip;
}

[[nodiscard]] bool TestClipUnderflowIsSafeAndObservable()
{
    using namespace greenfield;

    Fast2DRenderer renderer;
    RenderCommandList renderCommands;
    renderCommands.PopClip();
    renderCommands.AddFillRectangle(Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{10.0f, 10.0f}}, Color{});

    renderer.Submit(renderCommands);

    const auto fillOperations = renderer.PreparedFillOperations();
    return renderer.ClipUnderflowCount() == 1U && renderer.PreparedFillOperationCount() == 1U &&
           !fillOperations[0].hasClip;
}

[[nodiscard]] bool TestDrawTextIsDeferred()
{
    using namespace greenfield;

    Fast2DRenderer renderer;
    RenderCommandList renderCommands;
    renderCommands.AddText("Deferred", Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{80.0f, 20.0f}}, 14.0f, Color{});

    renderer.Submit(renderCommands);

    return renderer.DeferredTextCommandCount() == 1U && renderer.PreparedFillOperationCount() == 0U;
}

[[nodiscard]] bool TestBeginFrameResetsPreparedState()
{
    using namespace greenfield;

    Fast2DRenderer renderer;
    RenderCommandList renderCommands;
    renderCommands.PopClip();
    renderCommands.AddText("Deferred", Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{80.0f, 20.0f}}, 14.0f, Color{});
    renderCommands.AddFillRectangle(Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{10.0f, 10.0f}}, Color{});

    renderer.Submit(renderCommands);
    renderer.BeginFrame();

    return renderer.PreparedFillOperationCount() == 0U && renderer.DeferredTextCommandCount() == 0U &&
           renderer.ClipUnderflowCount() == 0U;
}

[[nodiscard]] bool TestEndFrameCompletesWithoutPlatformDependencies()
{
    using namespace greenfield;

    Fast2DRenderer renderer;
    RenderCommandList renderCommands;
    renderCommands.PushClip(Rect{.position = Vec2{1.0f, 2.0f}, .size = Vec2{30.0f, 40.0f}});
    renderCommands.PopClip();

    renderer.BeginFrame();
    renderer.Submit(renderCommands);
    renderer.EndFrame();

    return renderer.CompletedFrameCommandCount() == 2U;
}

[[nodiscard]] bool TestFillRectangleWritesRasterPixels()
{
    using namespace greenfield;

    const Color fillColor{0.2f, 0.4f, 0.6f, 1.0f};
    Fast2DRenderer renderer{5U, 4U};
    RenderCommandList renderCommands;
    renderCommands.AddFillRectangle(Rect{.position = Vec2{1.0f, 1.0f}, .size = Vec2{2.0f, 2.0f}}, fillColor);

    renderer.BeginFrame();
    renderer.Submit(renderCommands);
    renderer.EndFrame();

    return PixelMatches(renderer, 1U, 1U, fillColor) && PixelMatches(renderer, 2U, 1U, fillColor) &&
           PixelMatches(renderer, 1U, 2U, fillColor) && PixelMatches(renderer, 2U, 2U, fillColor) &&
           PixelMatches(renderer, 0U, 0U, Color{}) && PixelMatches(renderer, 3U, 3U, Color{});
}

[[nodiscard]] bool TestOptionalFillFieldsAreDeferredByRasterPath()
{
    using namespace greenfield;
    using greenfield::tests::ColorsMatch;

    const Color fillColor{0.1f, 0.3f, 0.8f, 1.0f};
    const Color borderColor{0.9f, 0.1f, 0.1f, 1.0f};
    Fast2DRenderer renderer{4U, 4U};
    RenderCommandList renderCommands;
    renderCommands.AddFillRectangle(Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{4.0f, 4.0f}},
                                    fillColor,
                                    2.0f,
                                    borderColor,
                                    1.0f);

    renderer.BeginFrame();
    renderer.Submit(renderCommands);
    renderer.EndFrame();

    const auto fillOperations = renderer.PreparedFillOperations();
    return renderer.PreparedFillOperationCount() == 1U && fillOperations[0].cornerRadius == 2.0f &&
           ColorsMatch(fillOperations[0].borderColor, borderColor) && fillOperations[0].borderThickness == 1.0f &&
           PixelMatches(renderer, 0U, 0U, fillColor) && PixelMatches(renderer, 1U, 0U, fillColor) &&
           PixelMatches(renderer, 2U, 2U, fillColor) && PixelMatches(renderer, 3U, 3U, fillColor);
}

[[nodiscard]] bool TestLaterFillRectangleOverridesEarlierRasterPixels()
{
    using namespace greenfield;

    const Color firstColor{0.8f, 0.1f, 0.1f, 1.0f};
    const Color secondColor{0.1f, 0.7f, 0.2f, 1.0f};
    Fast2DRenderer renderer{4U, 4U};
    RenderCommandList renderCommands;
    renderCommands.AddFillRectangle(Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{3.0f, 3.0f}}, firstColor);
    renderCommands.AddFillRectangle(Rect{.position = Vec2{1.0f, 1.0f}, .size = Vec2{3.0f, 3.0f}}, secondColor);

    renderer.BeginFrame();
    renderer.Submit(renderCommands);
    renderer.EndFrame();

    return PixelMatches(renderer, 0U, 0U, firstColor) && PixelMatches(renderer, 1U, 1U, secondColor) &&
           PixelMatches(renderer, 2U, 2U, secondColor) && PixelMatches(renderer, 3U, 3U, secondColor);
}

[[nodiscard]] bool TestClipLimitsRasterPixels()
{
    using namespace greenfield;

    const Color fillColor{0.2f, 0.8f, 0.3f, 1.0f};
    Fast2DRenderer renderer{5U, 5U};
    RenderCommandList renderCommands;
    renderCommands.PushClip(Rect{.position = Vec2{2.0f, 1.0f}, .size = Vec2{2.0f, 3.0f}});
    renderCommands.AddFillRectangle(Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{5.0f, 5.0f}}, fillColor);

    renderer.BeginFrame();
    renderer.Submit(renderCommands);
    renderer.EndFrame();

    return PixelMatches(renderer, 2U, 1U, fillColor) && PixelMatches(renderer, 3U, 3U, fillColor) &&
           PixelMatches(renderer, 1U, 1U, Color{}) && PixelMatches(renderer, 4U, 3U, Color{}) &&
           PixelMatches(renderer, 2U, 4U, Color{});
}

[[nodiscard]] bool TestPopClipRestoresPreviousRasterClip()
{
    using namespace greenfield;

    const Color firstColor{0.1f, 0.2f, 0.9f, 1.0f};
    const Color secondColor{0.9f, 0.2f, 0.1f, 1.0f};
    Fast2DRenderer renderer{6U, 4U};
    RenderCommandList renderCommands;
    renderCommands.PushClip(Rect{.position = Vec2{1.0f, 0.0f}, .size = Vec2{4.0f, 4.0f}});
    renderCommands.PushClip(Rect{.position = Vec2{2.0f, 0.0f}, .size = Vec2{1.0f, 4.0f}});
    renderCommands.AddFillRectangle(Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{6.0f, 4.0f}}, firstColor);
    renderCommands.PopClip();
    renderCommands.AddFillRectangle(Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{6.0f, 4.0f}}, secondColor);

    renderer.BeginFrame();
    renderer.Submit(renderCommands);
    renderer.EndFrame();

    return PixelMatches(renderer, 0U, 1U, Color{}) && PixelMatches(renderer, 1U, 1U, secondColor) &&
           PixelMatches(renderer, 2U, 1U, secondColor) && PixelMatches(renderer, 4U, 1U, secondColor) &&
           PixelMatches(renderer, 5U, 1U, Color{});
}

[[nodiscard]] bool TestOutOfBoundsRectanglesAreClippedToRasterTarget()
{
    using namespace greenfield;

    const Color fillColor{0.6f, 0.5f, 0.2f, 1.0f};
    Fast2DRenderer renderer{4U, 3U};
    RenderCommandList renderCommands;
    renderCommands.AddFillRectangle(Rect{.position = Vec2{-2.0f, -1.0f}, .size = Vec2{4.0f, 3.0f}}, fillColor);
    renderCommands.AddFillRectangle(Rect{.position = Vec2{10.0f, 10.0f}, .size = Vec2{2.0f, 2.0f}},
                                    Color{0.9f, 0.0f, 0.0f, 1.0f});
    renderCommands.AddFillRectangle(Rect{.position = Vec2{1.0f, 1.0f}, .size = Vec2{0.0f, 2.0f}},
                                    Color{0.0f, 0.9f, 0.0f, 1.0f});

    renderer.BeginFrame();
    renderer.Submit(renderCommands);
    renderer.EndFrame();

    return PixelMatches(renderer, 0U, 0U, fillColor) && PixelMatches(renderer, 1U, 1U, fillColor) &&
           PixelMatches(renderer, 2U, 1U, Color{}) && PixelMatches(renderer, 3U, 2U, Color{});
}

[[nodiscard]] bool TestClipOutsideRasterTargetIsClippedSafely()
{
    using namespace greenfield;

    const Color fillColor{0.1f, 0.7f, 0.7f, 1.0f};
    Fast2DRenderer renderer{4U, 4U};
    RenderCommandList renderCommands;
    renderCommands.PushClip(Rect{.position = Vec2{-1.0f, 1.0f}, .size = Vec2{3.0f, 2.0f}});
    renderCommands.AddFillRectangle(Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{4.0f, 4.0f}}, fillColor);

    renderer.BeginFrame();
    renderer.Submit(renderCommands);
    renderer.EndFrame();

    return PixelMatches(renderer, 0U, 1U, fillColor) && PixelMatches(renderer, 1U, 2U, fillColor) &&
           PixelMatches(renderer, 2U, 2U, Color{}) && PixelMatches(renderer, 0U, 3U, Color{});
}

[[nodiscard]] bool TestDrawTextDoesNotAlterRasterPixels()
{
    using namespace greenfield;

    Fast2DRenderer renderer{3U, 3U};
    RenderCommandList renderCommands;
    renderCommands.AddText("Deferred", Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{3.0f, 3.0f}}, 14.0f, Color{0.9f, 0.9f, 0.2f, 1.0f});

    renderer.BeginFrame();
    renderer.Submit(renderCommands);
    renderer.EndFrame();

    return renderer.DeferredTextCommandCount() == 1U && PixelMatches(renderer, 0U, 0U, Color{}) &&
           PixelMatches(renderer, 1U, 1U, Color{}) && PixelMatches(renderer, 2U, 2U, Color{});
}

[[nodiscard]] bool TestBeginFrameResetsRasterPixels()
{
    using namespace greenfield;

    const Color fillColor{0.7f, 0.2f, 0.4f, 1.0f};
    Fast2DRenderer renderer{3U, 3U};
    RenderCommandList renderCommands;
    renderCommands.AddFillRectangle(Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{3.0f, 3.0f}}, fillColor);

    renderer.BeginFrame();
    renderer.Submit(renderCommands);
    renderer.EndFrame();
    if (!PixelMatches(renderer, 1U, 1U, fillColor))
    {
        return false;
    }

    renderer.BeginFrame();
    return renderer.PreparedFillOperationCount() == 0U && renderer.SubmittedCommandCount() == 0U &&
           PixelMatches(renderer, 1U, 1U, Color{});
}

} // namespace

int main()
{
    if (!TestConstructionSucceeds() || !TestBeginFrameClearsSubmittedCommands() ||
        !TestSubmitRecordsAndAppendsCommands() || !TestFillRectangleIsPrepared() ||
        !TestMultipleFillRectanglesArePreparedInOrder() || !TestClipStackAffectsPreparedFillOperations() ||
        !TestClipUnderflowIsSafeAndObservable() || !TestDrawTextIsDeferred() ||
        !TestBeginFrameResetsPreparedState() || !TestEndFrameCompletesWithoutPlatformDependencies() ||
        !TestFillRectangleWritesRasterPixels() || !TestOptionalFillFieldsAreDeferredByRasterPath() ||
        !TestLaterFillRectangleOverridesEarlierRasterPixels() || !TestClipLimitsRasterPixels() ||
        !TestPopClipRestoresPreviousRasterClip() || !TestOutOfBoundsRectanglesAreClippedToRasterTarget() ||
        !TestClipOutsideRasterTargetIsClippedSafely() || !TestDrawTextDoesNotAlterRasterPixels() ||
        !TestBeginFrameResetsRasterPixels())
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

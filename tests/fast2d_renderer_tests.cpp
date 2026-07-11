#include <cmath>
#include <cstdlib>
#include <vector>

#include "engine/render/fast2d/Fast2DRenderer.h"
#include "engine/visualization/VisualizationCommandList.h"
#include "tests/TestHelpers.h"

namespace
{

[[nodiscard]] bool PixelMatches(const greenfield::Fast2DRenderer& renderer, std::size_t x, std::size_t y,
                                const greenfield::Color& color)
{
    return greenfield::tests::ColorsMatch(renderer.RasterPixelAt(x, y), color);
}

[[nodiscard]] bool ColorsNearlyMatch(const greenfield::Color& firstColor, const greenfield::Color& secondColor)
{
    constexpr float Tolerance = 0.0001f;
    return std::abs(firstColor.red - secondColor.red) <= Tolerance &&
           std::abs(firstColor.green - secondColor.green) <= Tolerance &&
           std::abs(firstColor.blue - secondColor.blue) <= Tolerance &&
           std::abs(firstColor.alpha - secondColor.alpha) <= Tolerance;
}

[[nodiscard]] bool PixelNearlyMatches(const greenfield::Fast2DRenderer& renderer, std::size_t x, std::size_t y,
                                      const greenfield::Color& color)
{
    return ColorsNearlyMatch(renderer.RasterPixelAt(x, y), color);
}

[[nodiscard]] greenfield::Rect FullRasterClip(std::size_t width, std::size_t height)
{
    return greenfield::Rect{
        .position = greenfield::Vec2{0.0f, 0.0f},
        .size = greenfield::Vec2{static_cast<float>(width), static_cast<float>(height)},
    };
}

[[nodiscard]] greenfield::VisualizationCommandList MakeVisualizationCommandsWithClip(std::size_t width,
                                                                                     std::size_t height)
{
    greenfield::VisualizationCommandList visualizationCommands;
    if (!visualizationCommands.SetClipBounds(FullRasterClip(width, height)))
    {
        return greenfield::VisualizationCommandList{};
    }

    return visualizationCommands;
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
    const Rect nestedClipIntersection = IntersectRectangles(firstClip, secondClip);

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
           RectanglesMatch(fillOperations[1].clipRectangle, nestedClipIntersection) && fillOperations[2].hasClip &&
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

[[nodiscard]] bool TestZeroCornerRadiusPreservesPlainRectangleFill()
{
    using namespace greenfield;

    const Color fillColor{0.2f, 0.4f, 0.6f, 1.0f};
    Fast2DRenderer renderer{3U, 3U};
    RenderCommandList renderCommands;
    renderCommands.AddFillRectangle(Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{3.0f, 3.0f}},
                                    fillColor,
                                    0.0f);

    renderer.BeginFrame();
    renderer.Submit(renderCommands);
    renderer.EndFrame();

    return PixelMatches(renderer, 0U, 0U, fillColor) && PixelMatches(renderer, 1U, 1U, fillColor) &&
           PixelMatches(renderer, 2U, 2U, fillColor);
}

[[nodiscard]] bool TestPositiveCornerRadiusLeavesExtremeCornersUntouched()
{
    using namespace greenfield;

    const Color fillColor{0.9f, 0.1f, 0.2f, 1.0f};
    Fast2DRenderer renderer{5U, 5U};
    RenderCommandList renderCommands;
    renderCommands.AddFillRectangle(Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{5.0f, 5.0f}},
                                    fillColor,
                                    2.0f);

    renderer.BeginFrame();
    renderer.Submit(renderCommands);
    renderer.EndFrame();

    return PixelMatches(renderer, 0U, 0U, Color{}) && PixelMatches(renderer, 4U, 0U, Color{}) &&
           PixelMatches(renderer, 0U, 4U, Color{}) && PixelMatches(renderer, 4U, 4U, Color{});
}

[[nodiscard]] bool TestPositiveCornerRadiusFillsCenterAndBodyPixels()
{
    using namespace greenfield;

    const Color fillColor{0.2f, 0.8f, 0.3f, 1.0f};
    Fast2DRenderer renderer{5U, 5U};
    RenderCommandList renderCommands;
    renderCommands.AddFillRectangle(Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{5.0f, 5.0f}},
                                    fillColor,
                                    2.0f);

    renderer.BeginFrame();
    renderer.Submit(renderCommands);
    renderer.EndFrame();

    return PixelMatches(renderer, 2U, 2U, fillColor) && PixelMatches(renderer, 2U, 0U, fillColor) &&
           PixelMatches(renderer, 0U, 2U, fillColor) && PixelMatches(renderer, 1U, 0U, fillColor);
}

[[nodiscard]] bool TestRoundedFillUsesSourceOverAlphaBlending()
{
    using namespace greenfield;

    Fast2DRenderer renderer{5U, 5U};
    RenderCommandList renderCommands;
    renderCommands.AddFillRectangle(Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{5.0f, 5.0f}},
                                    Color{0.0f, 0.0f, 1.0f, 1.0f});
    renderCommands.AddFillRectangle(Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{5.0f, 5.0f}},
                                    Color{1.0f, 0.0f, 0.0f, 0.5f},
                                    2.0f);

    renderer.BeginFrame();
    renderer.Submit(renderCommands);
    renderer.EndFrame();

    return PixelNearlyMatches(renderer, 2U, 2U, Color{0.5f, 0.0f, 0.5f, 1.0f}) &&
           PixelMatches(renderer, 0U, 0U, Color{0.0f, 0.0f, 1.0f, 1.0f});
}

[[nodiscard]] bool TestRoundedFillRespectsClip()
{
    using namespace greenfield;

    const Color fillColor{0.2f, 0.7f, 0.9f, 1.0f};
    Fast2DRenderer renderer{5U, 5U};
    RenderCommandList renderCommands;
    renderCommands.PushClip(Rect{.position = Vec2{2.0f, 1.0f}, .size = Vec2{2.0f, 2.0f}});
    renderCommands.AddFillRectangle(Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{5.0f, 5.0f}},
                                    fillColor,
                                    2.0f);

    renderer.BeginFrame();
    renderer.Submit(renderCommands);
    renderer.EndFrame();

    return PixelMatches(renderer, 2U, 1U, fillColor) && PixelMatches(renderer, 3U, 2U, fillColor) &&
           PixelMatches(renderer, 1U, 1U, Color{}) && PixelMatches(renderer, 4U, 2U, Color{});
}

[[nodiscard]] bool TestLargeCornerRadiusOnSmallRectangleIsDeterministic()
{
    using namespace greenfield;

    const Color fillColor{0.6f, 0.4f, 0.1f, 1.0f};
    Fast2DRenderer renderer{3U, 3U};
    RenderCommandList renderCommands;
    renderCommands.AddFillRectangle(Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{2.0f, 1.0f}},
                                    fillColor,
                                    10.0f);
    renderCommands.AddFillRectangle(Rect{.position = Vec2{2.0f, 2.0f}, .size = Vec2{1.0f, 1.0f}},
                                    fillColor,
                                    10.0f);

    renderer.BeginFrame();
    renderer.Submit(renderCommands);
    renderer.EndFrame();

    return PixelMatches(renderer, 0U, 0U, fillColor) && PixelMatches(renderer, 1U, 0U, fillColor) &&
           PixelMatches(renderer, 2U, 2U, fillColor) && PixelMatches(renderer, 0U, 1U, Color{});
}

[[nodiscard]] bool TestRoundedBorderLeavesExtremeCornersUntouched()
{
    using namespace greenfield;

    const Color fillColor{0.1f, 0.3f, 0.8f, 1.0f};
    const Color borderColor{0.9f, 0.1f, 0.1f, 1.0f};
    Fast2DRenderer renderer{5U, 5U};
    RenderCommandList renderCommands;
    renderCommands.AddFillRectangle(Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{5.0f, 5.0f}},
                                    fillColor,
                                    2.0f,
                                    borderColor,
                                    1.0f);

    renderer.BeginFrame();
    renderer.Submit(renderCommands);
    renderer.EndFrame();

    return PixelMatches(renderer, 0U, 0U, Color{}) && PixelMatches(renderer, 4U, 4U, Color{}) &&
           PixelMatches(renderer, 2U, 0U, borderColor) && PixelMatches(renderer, 2U, 2U, fillColor);
}

[[nodiscard]] bool TestRoundedBorderDrawsEdgeAndBodyPixels()
{
    using namespace greenfield;

    const Color fillColor{0.1f, 0.3f, 0.8f, 1.0f};
    const Color borderColor{0.9f, 0.1f, 0.1f, 1.0f};
    Fast2DRenderer renderer{5U, 5U};
    RenderCommandList renderCommands;
    renderCommands.AddFillRectangle(Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{5.0f, 5.0f}},
                                    fillColor,
                                    2.0f,
                                    borderColor,
                                    1.0f);

    renderer.BeginFrame();
    renderer.Submit(renderCommands);
    renderer.EndFrame();

    return PixelMatches(renderer, 2U, 0U, borderColor) && PixelMatches(renderer, 0U, 2U, borderColor) &&
           PixelMatches(renderer, 1U, 0U, borderColor) && PixelMatches(renderer, 1U, 1U, fillColor);
}

[[nodiscard]] bool TestRoundedBorderDrawsAfterFill()
{
    using namespace greenfield;

    const Color fillColor{0.0f, 0.0f, 1.0f, 1.0f};
    const Color borderColor{1.0f, 0.0f, 0.0f, 1.0f};
    Fast2DRenderer renderer{5U, 5U};
    RenderCommandList renderCommands;
    renderCommands.AddFillRectangle(Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{5.0f, 5.0f}},
                                    fillColor,
                                    2.0f,
                                    borderColor,
                                    1.0f);

    renderer.BeginFrame();
    renderer.Submit(renderCommands);
    renderer.EndFrame();

    return PixelMatches(renderer, 2U, 0U, borderColor) && PixelMatches(renderer, 2U, 2U, fillColor) &&
           PixelMatches(renderer, 0U, 0U, Color{});
}

[[nodiscard]] bool TestPartialAlphaRoundedBorderBlendsOverFill()
{
    using namespace greenfield;

    Fast2DRenderer renderer{5U, 5U};
    RenderCommandList renderCommands;
    renderCommands.AddFillRectangle(Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{5.0f, 5.0f}},
                                    Color{0.0f, 0.0f, 1.0f, 1.0f},
                                    2.0f,
                                    Color{1.0f, 0.0f, 0.0f, 0.5f},
                                    1.0f);

    renderer.BeginFrame();
    renderer.Submit(renderCommands);
    renderer.EndFrame();

    return PixelNearlyMatches(renderer, 2U, 0U, Color{0.5f, 0.0f, 0.5f, 1.0f}) &&
           PixelMatches(renderer, 2U, 2U, Color{0.0f, 0.0f, 1.0f, 1.0f}) &&
           PixelMatches(renderer, 0U, 0U, Color{});
}

[[nodiscard]] bool TestRoundedBorderRespectsNestedClips()
{
    using namespace greenfield;

    const Color fillColor{0.0f, 0.0f, 1.0f, 1.0f};
    const Color borderColor{1.0f, 0.0f, 0.0f, 1.0f};
    Fast2DRenderer renderer{5U, 5U};
    RenderCommandList renderCommands;
    renderCommands.PushClip(Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{5.0f, 2.0f}});
    renderCommands.PushClip(Rect{.position = Vec2{2.0f, 0.0f}, .size = Vec2{3.0f, 5.0f}});
    renderCommands.AddFillRectangle(Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{5.0f, 5.0f}},
                                    fillColor,
                                    2.0f,
                                    borderColor,
                                    1.0f);

    renderer.BeginFrame();
    renderer.Submit(renderCommands);
    renderer.EndFrame();

    return PixelMatches(renderer, 2U, 0U, borderColor) && PixelMatches(renderer, 3U, 1U, fillColor) &&
           PixelMatches(renderer, 1U, 0U, Color{}) && PixelMatches(renderer, 3U, 2U, Color{});
}

[[nodiscard]] bool TestLargeRoundedBorderThicknessCoversSmallRectangleDeterministically()
{
    using namespace greenfield;

    const Color borderColor{1.0f, 0.0f, 0.0f, 1.0f};
    Fast2DRenderer renderer{4U, 4U};
    RenderCommandList renderCommands;
    renderCommands.AddFillRectangle(Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{4.0f, 4.0f}},
                                    Color{0.0f, 0.0f, 1.0f, 1.0f},
                                    10.0f,
                                    borderColor,
                                    10.0f);

    renderer.BeginFrame();
    renderer.Submit(renderCommands);
    renderer.EndFrame();

    return PixelMatches(renderer, 2U, 0U, borderColor) && PixelMatches(renderer, 2U, 2U, borderColor) &&
           PixelMatches(renderer, 0U, 0U, Color{});
}

[[nodiscard]] bool TestFillRectanglePreservesStyleMetadataAndRasterizesBorder()
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
           PixelMatches(renderer, 0U, 0U, Color{}) && PixelMatches(renderer, 1U, 0U, borderColor) &&
           PixelMatches(renderer, 2U, 2U, fillColor) && PixelMatches(renderer, 3U, 3U, Color{});
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

[[nodiscard]] bool TestOpaqueFillStillReplacesRasterPixels()
{
    using namespace greenfield;

    const Color firstColor{0.0f, 0.0f, 1.0f, 0.5f};
    const Color secondColor{0.25f, 0.5f, 0.75f, 1.0f};
    Fast2DRenderer renderer{2U, 2U};
    RenderCommandList renderCommands;
    renderCommands.AddFillRectangle(Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{2.0f, 2.0f}}, firstColor);
    renderCommands.AddFillRectangle(Rect{.position = Vec2{1.0f, 1.0f}, .size = Vec2{1.0f, 1.0f}}, secondColor);

    renderer.BeginFrame();
    renderer.Submit(renderCommands);
    renderer.EndFrame();

    return PixelMatches(renderer, 1U, 1U, secondColor);
}

[[nodiscard]] bool TestTransparentFillLeavesRasterPixelsUnchanged()
{
    using namespace greenfield;

    const Color destinationColor{0.25f, 0.5f, 0.75f, 1.0f};
    Fast2DRenderer renderer{2U, 2U};
    RenderCommandList renderCommands;
    renderCommands.AddFillRectangle(Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{2.0f, 2.0f}},
                                    destinationColor);
    renderCommands.AddFillRectangle(Rect{.position = Vec2{1.0f, 1.0f}, .size = Vec2{1.0f, 1.0f}},
                                    Color{1.0f, 0.0f, 0.0f, 0.0f});

    renderer.BeginFrame();
    renderer.Submit(renderCommands);
    renderer.EndFrame();

    return PixelMatches(renderer, 1U, 1U, destinationColor);
}

[[nodiscard]] bool TestPartialAlphaFillBlendsOverOpaqueDestination()
{
    using namespace greenfield;

    Fast2DRenderer renderer{2U, 2U};
    RenderCommandList renderCommands;
    renderCommands.AddFillRectangle(Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{2.0f, 2.0f}},
                                    Color{0.0f, 0.0f, 1.0f, 1.0f});
    renderCommands.AddFillRectangle(Rect{.position = Vec2{1.0f, 1.0f}, .size = Vec2{1.0f, 1.0f}},
                                    Color{1.0f, 0.0f, 0.0f, 0.5f});

    renderer.BeginFrame();
    renderer.Submit(renderCommands);
    renderer.EndFrame();

    return PixelNearlyMatches(renderer, 1U, 1U, Color{0.5f, 0.0f, 0.5f, 1.0f}) &&
           PixelMatches(renderer, 0U, 0U, Color{0.0f, 0.0f, 1.0f, 1.0f});
}

[[nodiscard]] bool TestPartialAlphaFillBlendsAgainstExistingRasterPixels()
{
    using namespace greenfield;

    Fast2DRenderer renderer{2U, 2U};
    RenderCommandList renderCommands;
    renderCommands.AddFillRectangle(Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{2.0f, 2.0f}},
                                    Color{0.0f, 0.0f, 1.0f, 0.5f});
    renderCommands.AddFillRectangle(Rect{.position = Vec2{1.0f, 1.0f}, .size = Vec2{1.0f, 1.0f}},
                                    Color{1.0f, 0.0f, 0.0f, 0.5f});

    renderer.BeginFrame();
    renderer.Submit(renderCommands);
    renderer.EndFrame();

    return PixelNearlyMatches(renderer, 1U, 1U, Color{0.5f, 0.0f, 0.25f, 1.0f});
}

[[nodiscard]] bool TestAlphaBlendedFillStillHonorsClip()
{
    using namespace greenfield;

    const Color destinationColor{0.0f, 0.0f, 1.0f, 1.0f};
    Fast2DRenderer renderer{3U, 3U};
    RenderCommandList renderCommands;
    renderCommands.AddFillRectangle(Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{3.0f, 3.0f}},
                                    destinationColor);
    renderCommands.PushClip(Rect{.position = Vec2{1.0f, 1.0f}, .size = Vec2{1.0f, 1.0f}});
    renderCommands.AddFillRectangle(Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{3.0f, 3.0f}},
                                    Color{1.0f, 0.0f, 0.0f, 0.5f});

    renderer.BeginFrame();
    renderer.Submit(renderCommands);
    renderer.EndFrame();

    return PixelNearlyMatches(renderer, 1U, 1U, Color{0.5f, 0.0f, 0.5f, 1.0f}) &&
           PixelMatches(renderer, 0U, 0U, destinationColor) &&
           PixelMatches(renderer, 2U, 2U, destinationColor);
}

[[nodiscard]] bool TestZeroBorderThicknessPreservesFillOnlyRasterPath()
{
    using namespace greenfield;

    const Color fillColor{0.1f, 0.3f, 0.8f, 1.0f};
    Fast2DRenderer renderer{3U, 3U};
    RenderCommandList renderCommands;
    renderCommands.AddFillRectangle(Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{3.0f, 3.0f}},
                                    fillColor,
                                    0.0f,
                                    Color{1.0f, 0.0f, 0.0f, 1.0f},
                                    0.0f);

    renderer.BeginFrame();
    renderer.Submit(renderCommands);
    renderer.EndFrame();

    return PixelMatches(renderer, 0U, 0U, fillColor) && PixelMatches(renderer, 1U, 1U, fillColor) &&
           PixelMatches(renderer, 2U, 2U, fillColor);
}

[[nodiscard]] bool TestNegativeBorderThicknessPreservesFillOnlyRasterPath()
{
    using namespace greenfield;

    const Color fillColor{0.1f, 0.3f, 0.8f, 1.0f};
    Fast2DRenderer renderer{3U, 3U};
    RenderCommandList renderCommands;
    renderCommands.AddFillRectangle(Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{3.0f, 3.0f}},
                                    fillColor,
                                    0.0f,
                                    Color{1.0f, 0.0f, 0.0f, 1.0f},
                                    -1.0f);

    renderer.BeginFrame();
    renderer.Submit(renderCommands);
    renderer.EndFrame();

    return PixelMatches(renderer, 0U, 0U, fillColor) && PixelMatches(renderer, 1U, 1U, fillColor) &&
           PixelMatches(renderer, 2U, 2U, fillColor);
}

[[nodiscard]] bool TestBorderDrawsAfterFill()
{
    using namespace greenfield;

    const Color fillColor{0.0f, 0.0f, 1.0f, 1.0f};
    const Color borderColor{1.0f, 0.0f, 0.0f, 1.0f};
    Fast2DRenderer renderer{4U, 4U};
    RenderCommandList renderCommands;
    renderCommands.AddFillRectangle(Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{4.0f, 4.0f}},
                                    fillColor,
                                    0.0f,
                                    borderColor,
                                    1.0f);

    renderer.BeginFrame();
    renderer.Submit(renderCommands);
    renderer.EndFrame();

    return PixelMatches(renderer, 0U, 0U, borderColor) && PixelMatches(renderer, 3U, 2U, borderColor) &&
           PixelMatches(renderer, 1U, 1U, fillColor);
}

[[nodiscard]] bool TestTransparentBorderLeavesFillUnchanged()
{
    using namespace greenfield;

    const Color fillColor{0.0f, 0.0f, 1.0f, 1.0f};
    Fast2DRenderer renderer{3U, 3U};
    RenderCommandList renderCommands;
    renderCommands.AddFillRectangle(Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{3.0f, 3.0f}},
                                    fillColor,
                                    0.0f,
                                    Color{1.0f, 0.0f, 0.0f, 0.0f},
                                    1.0f);

    renderer.BeginFrame();
    renderer.Submit(renderCommands);
    renderer.EndFrame();

    return PixelMatches(renderer, 0U, 0U, fillColor) && PixelMatches(renderer, 1U, 1U, fillColor) &&
           PixelMatches(renderer, 2U, 2U, fillColor);
}

[[nodiscard]] bool TestPartialAlphaBorderBlendsOverFill()
{
    using namespace greenfield;

    Fast2DRenderer renderer{3U, 3U};
    RenderCommandList renderCommands;
    renderCommands.AddFillRectangle(Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{3.0f, 3.0f}},
                                    Color{0.0f, 0.0f, 1.0f, 1.0f},
                                    0.0f,
                                    Color{1.0f, 0.0f, 0.0f, 0.5f},
                                    1.0f);

    renderer.BeginFrame();
    renderer.Submit(renderCommands);
    renderer.EndFrame();

    return PixelNearlyMatches(renderer, 0U, 0U, Color{0.5f, 0.0f, 0.5f, 1.0f}) &&
           PixelMatches(renderer, 1U, 1U, Color{0.0f, 0.0f, 1.0f, 1.0f});
}

[[nodiscard]] bool TestBorderRespectsClip()
{
    using namespace greenfield;

    const Color fillColor{0.0f, 0.0f, 1.0f, 1.0f};
    const Color borderColor{1.0f, 0.0f, 0.0f, 1.0f};
    Fast2DRenderer renderer{4U, 4U};
    RenderCommandList renderCommands;
    renderCommands.PushClip(Rect{.position = Vec2{1.0f, 0.0f}, .size = Vec2{3.0f, 4.0f}});
    renderCommands.AddFillRectangle(Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{4.0f, 4.0f}},
                                    fillColor,
                                    0.0f,
                                    borderColor,
                                    1.0f);

    renderer.BeginFrame();
    renderer.Submit(renderCommands);
    renderer.EndFrame();

    return PixelMatches(renderer, 0U, 0U, Color{}) && PixelMatches(renderer, 1U, 0U, borderColor) &&
           PixelMatches(renderer, 1U, 1U, fillColor) && PixelMatches(renderer, 3U, 1U, borderColor);
}

[[nodiscard]] bool TestLargeBorderThicknessCoversSmallRectangleDeterministically()
{
    using namespace greenfield;

    const Color borderColor{1.0f, 0.0f, 0.0f, 1.0f};
    Fast2DRenderer renderer{3U, 3U};
    RenderCommandList renderCommands;
    renderCommands.AddFillRectangle(Rect{.position = Vec2{1.0f, 1.0f}, .size = Vec2{1.0f, 1.0f}},
                                    Color{0.0f, 0.0f, 1.0f, 1.0f},
                                    0.0f,
                                    borderColor,
                                    4.0f);

    renderer.BeginFrame();
    renderer.Submit(renderCommands);
    renderer.EndFrame();

    return PixelMatches(renderer, 1U, 1U, borderColor) && PixelMatches(renderer, 0U, 0U, Color{}) &&
           PixelMatches(renderer, 2U, 2U, Color{});
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

[[nodiscard]] bool TestNestedOverlappingClipsLimitRasterPixelsToIntersection()
{
    using namespace greenfield;

    const Color fillColor{0.2f, 0.8f, 0.3f, 1.0f};
    Fast2DRenderer renderer{6U, 5U};
    RenderCommandList renderCommands;
    renderCommands.PushClip(Rect{.position = Vec2{1.0f, 1.0f}, .size = Vec2{4.0f, 4.0f}});
    renderCommands.PushClip(Rect{.position = Vec2{3.0f, 0.0f}, .size = Vec2{4.0f, 3.0f}});
    renderCommands.AddFillRectangle(Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{6.0f, 5.0f}}, fillColor);

    renderer.BeginFrame();
    renderer.Submit(renderCommands);
    renderer.EndFrame();

    return PixelMatches(renderer, 3U, 1U, fillColor) && PixelMatches(renderer, 4U, 2U, fillColor) &&
           PixelMatches(renderer, 2U, 1U, Color{}) && PixelMatches(renderer, 3U, 0U, Color{}) &&
           PixelMatches(renderer, 5U, 2U, Color{});
}

[[nodiscard]] bool TestNonOverlappingNestedClipsModifyNoRasterPixels()
{
    using namespace greenfield;

    const Color destinationColor{0.0f, 0.0f, 1.0f, 1.0f};
    Fast2DRenderer renderer{5U, 3U};
    RenderCommandList renderCommands;
    renderCommands.AddFillRectangle(Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{5.0f, 3.0f}},
                                    destinationColor);
    renderCommands.PushClip(Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{2.0f, 3.0f}});
    renderCommands.PushClip(Rect{.position = Vec2{3.0f, 0.0f}, .size = Vec2{2.0f, 3.0f}});
    renderCommands.AddFillRectangle(Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{5.0f, 3.0f}},
                                    Color{1.0f, 0.0f, 0.0f, 1.0f});

    renderer.BeginFrame();
    renderer.Submit(renderCommands);
    renderer.EndFrame();

    return PixelMatches(renderer, 0U, 1U, destinationColor) &&
           PixelMatches(renderer, 1U, 1U, destinationColor) &&
           PixelMatches(renderer, 3U, 1U, destinationColor) &&
           PixelMatches(renderer, 4U, 1U, destinationColor);
}

[[nodiscard]] bool TestNestedClipsLimitAlphaBlendedFillToIntersection()
{
    using namespace greenfield;

    const Color destinationColor{0.0f, 0.0f, 1.0f, 1.0f};
    Fast2DRenderer renderer{5U, 4U};
    RenderCommandList renderCommands;
    renderCommands.AddFillRectangle(Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{5.0f, 4.0f}},
                                    destinationColor);
    renderCommands.PushClip(Rect{.position = Vec2{1.0f, 1.0f}, .size = Vec2{3.0f, 3.0f}});
    renderCommands.PushClip(Rect{.position = Vec2{2.0f, 0.0f}, .size = Vec2{3.0f, 2.0f}});
    renderCommands.AddFillRectangle(Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{5.0f, 4.0f}},
                                    Color{1.0f, 0.0f, 0.0f, 0.5f});

    renderer.BeginFrame();
    renderer.Submit(renderCommands);
    renderer.EndFrame();

    return PixelNearlyMatches(renderer, 2U, 1U, Color{0.5f, 0.0f, 0.5f, 1.0f}) &&
           PixelNearlyMatches(renderer, 3U, 1U, Color{0.5f, 0.0f, 0.5f, 1.0f}) &&
           PixelMatches(renderer, 1U, 1U, destinationColor) &&
           PixelMatches(renderer, 2U, 2U, destinationColor);
}

[[nodiscard]] bool TestNestedClipsLimitBorderRasterizationToIntersection()
{
    using namespace greenfield;

    const Color fillColor{0.0f, 0.0f, 1.0f, 1.0f};
    const Color borderColor{1.0f, 0.0f, 0.0f, 1.0f};
    Fast2DRenderer renderer{5U, 5U};
    RenderCommandList renderCommands;
    renderCommands.PushClip(Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{5.0f, 2.0f}});
    renderCommands.PushClip(Rect{.position = Vec2{2.0f, 0.0f}, .size = Vec2{3.0f, 5.0f}});
    renderCommands.AddFillRectangle(Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{5.0f, 5.0f}},
                                    fillColor,
                                    0.0f,
                                    borderColor,
                                    1.0f);

    renderer.BeginFrame();
    renderer.Submit(renderCommands);
    renderer.EndFrame();

    return PixelMatches(renderer, 2U, 0U, borderColor) && PixelMatches(renderer, 4U, 1U, borderColor) &&
           PixelMatches(renderer, 2U, 1U, fillColor) && PixelMatches(renderer, 1U, 0U, Color{}) &&
           PixelMatches(renderer, 3U, 2U, Color{});
}

[[nodiscard]] bool TestNestedClipsLimitRoundedFillRasterizationToIntersection()
{
    using namespace greenfield;

    const Color fillColor{0.2f, 0.7f, 0.9f, 1.0f};
    Fast2DRenderer renderer{5U, 5U};
    RenderCommandList renderCommands;
    renderCommands.PushClip(Rect{.position = Vec2{1.0f, 1.0f}, .size = Vec2{3.0f, 3.0f}});
    renderCommands.PushClip(Rect{.position = Vec2{2.0f, 0.0f}, .size = Vec2{3.0f, 3.0f}});
    renderCommands.AddFillRectangle(Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{5.0f, 5.0f}},
                                    fillColor,
                                    2.0f);

    renderer.BeginFrame();
    renderer.Submit(renderCommands);
    renderer.EndFrame();

    return PixelMatches(renderer, 2U, 1U, fillColor) && PixelMatches(renderer, 3U, 2U, fillColor) &&
           PixelMatches(renderer, 1U, 1U, Color{}) && PixelMatches(renderer, 2U, 3U, Color{}) &&
           PixelMatches(renderer, 4U, 2U, Color{});
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

[[nodiscard]] bool TestEmptyVisualizationCommandListChangesNoPixels()
{
    using namespace greenfield;

    Fast2DRenderer renderer{3U, 3U};
    VisualizationCommandList visualizationCommands;
    if (!visualizationCommands.SetClipBounds(FullRasterClip(3U, 3U)))
    {
        return false;
    }

    renderer.BeginFrame();
    renderer.SubmitVisualization(visualizationCommands);
    renderer.EndFrame();

    return renderer.SubmittedVisualizationCommandCount() == 0U &&
           renderer.CompletedFrameVisualizationCommandCount() == 0U && PixelMatches(renderer, 1U, 1U, Color{});
}

[[nodiscard]] bool TestVisualizationHorizontalLineRasterizesExpectedPixels()
{
    using namespace greenfield;

    const Color lineColor{1.0f, 0.0f, 0.0f, 1.0f};
    Fast2DRenderer renderer{6U, 4U};
    VisualizationCommandList visualizationCommands = MakeVisualizationCommandsWithClip(6U, 4U);

    if (!visualizationCommands.AddLine(Vec2{1.5f, 2.5f}, Vec2{4.5f, 2.5f}, lineColor, 1.0f))
    {
        return false;
    }

    renderer.BeginFrame();
    renderer.SubmitVisualization(visualizationCommands);
    renderer.EndFrame();

    return PixelMatches(renderer, 1U, 2U, lineColor) && PixelMatches(renderer, 4U, 2U, lineColor) &&
           PixelMatches(renderer, 0U, 2U, Color{}) && PixelMatches(renderer, 5U, 2U, Color{}) &&
           PixelMatches(renderer, 2U, 1U, Color{});
}

[[nodiscard]] bool TestVisualizationVerticalLineRasterizesExpectedPixels()
{
    using namespace greenfield;

    const Color lineColor{0.0f, 1.0f, 0.0f, 1.0f};
    Fast2DRenderer renderer{5U, 6U};
    VisualizationCommandList visualizationCommands = MakeVisualizationCommandsWithClip(5U, 6U);

    if (!visualizationCommands.AddLine(Vec2{2.5f, 1.5f}, Vec2{2.5f, 4.5f}, lineColor, 1.0f))
    {
        return false;
    }

    renderer.BeginFrame();
    renderer.SubmitVisualization(visualizationCommands);
    renderer.EndFrame();

    return PixelMatches(renderer, 2U, 1U, lineColor) && PixelMatches(renderer, 2U, 4U, lineColor) &&
           PixelMatches(renderer, 2U, 0U, Color{}) && PixelMatches(renderer, 2U, 5U, Color{}) &&
           PixelMatches(renderer, 1U, 2U, Color{});
}

[[nodiscard]] bool TestVisualizationDiagonalLineRasterizesDeterministically()
{
    using namespace greenfield;

    const Color lineColor{0.1f, 0.8f, 0.9f, 1.0f};
    Fast2DRenderer renderer{5U, 5U};
    VisualizationCommandList visualizationCommands = MakeVisualizationCommandsWithClip(5U, 5U);

    if (!visualizationCommands.AddLine(Vec2{0.5f, 0.5f}, Vec2{4.5f, 4.5f}, lineColor, 1.0f))
    {
        return false;
    }

    renderer.BeginFrame();
    renderer.SubmitVisualization(visualizationCommands);
    renderer.EndFrame();

    return PixelMatches(renderer, 0U, 0U, lineColor) && PixelMatches(renderer, 2U, 2U, lineColor) &&
           PixelMatches(renderer, 4U, 4U, lineColor) && PixelMatches(renderer, 0U, 1U, Color{}) &&
           PixelMatches(renderer, 3U, 2U, Color{});
}

[[nodiscard]] bool TestVisualizationFractionalLineUsesPixelCenterSampling()
{
    using namespace greenfield;

    const Color lineColor{0.8f, 0.6f, 0.1f, 1.0f};
    Fast2DRenderer renderer{5U, 4U};
    VisualizationCommandList visualizationCommands = MakeVisualizationCommandsWithClip(5U, 4U);

    if (!visualizationCommands.AddLine(Vec2{0.5f, 1.75f}, Vec2{3.5f, 1.75f}, lineColor, 0.5f))
    {
        return false;
    }

    renderer.BeginFrame();
    renderer.SubmitVisualization(visualizationCommands);
    renderer.EndFrame();

    return PixelMatches(renderer, 1U, 1U, lineColor) && PixelMatches(renderer, 3U, 1U, lineColor) &&
           PixelMatches(renderer, 1U, 2U, Color{});
}

[[nodiscard]] bool TestVisualizationStrokeThicknessChangesCoverage()
{
    using namespace greenfield;

    const Color lineColor{0.4f, 0.2f, 0.9f, 1.0f};
    Fast2DRenderer thinRenderer{5U, 4U};
    Fast2DRenderer thickRenderer{5U, 4U};
    VisualizationCommandList thinCommands = MakeVisualizationCommandsWithClip(5U, 4U);
    VisualizationCommandList thickCommands = MakeVisualizationCommandsWithClip(5U, 4U);

    if (!thinCommands.AddLine(Vec2{1.5f, 1.5f}, Vec2{3.5f, 1.5f}, lineColor, 1.0f) ||
        !thickCommands.AddLine(Vec2{1.5f, 1.5f}, Vec2{3.5f, 1.5f}, lineColor, 3.0f))
    {
        return false;
    }

    thinRenderer.BeginFrame();
    thinRenderer.SubmitVisualization(thinCommands);
    thinRenderer.EndFrame();

    thickRenderer.BeginFrame();
    thickRenderer.SubmitVisualization(thickCommands);
    thickRenderer.EndFrame();

    return PixelMatches(thinRenderer, 2U, 0U, Color{}) && PixelMatches(thickRenderer, 2U, 0U, lineColor) &&
           PixelMatches(thickRenderer, 2U, 2U, lineColor) && PixelMatches(thickRenderer, 2U, 3U, Color{});
}

[[nodiscard]] bool TestVisualizationZeroLengthLineRendersRoundMark()
{
    using namespace greenfield;

    const Color lineColor{1.0f, 0.5f, 0.0f, 1.0f};
    Fast2DRenderer renderer{5U, 5U};
    VisualizationCommandList visualizationCommands = MakeVisualizationCommandsWithClip(5U, 5U);

    if (!visualizationCommands.AddLine(Vec2{2.5f, 2.5f}, Vec2{2.5f, 2.5f}, lineColor, 2.0f))
    {
        return false;
    }

    renderer.BeginFrame();
    renderer.SubmitVisualization(visualizationCommands);
    renderer.EndFrame();

    return PixelMatches(renderer, 2U, 2U, lineColor) && PixelMatches(renderer, 3U, 2U, lineColor) &&
           PixelMatches(renderer, 3U, 3U, Color{});
}

[[nodiscard]] bool TestVisualizationPartiallyOffscreenLineIsSafe()
{
    using namespace greenfield;

    const Color lineColor{0.7f, 0.1f, 0.2f, 1.0f};
    Fast2DRenderer renderer{4U, 3U};
    VisualizationCommandList visualizationCommands = MakeVisualizationCommandsWithClip(4U, 3U);

    if (!visualizationCommands.AddLine(Vec2{-2.5f, 1.5f}, Vec2{2.5f, 1.5f}, lineColor, 1.0f))
    {
        return false;
    }

    renderer.BeginFrame();
    renderer.SubmitVisualization(visualizationCommands);
    renderer.EndFrame();

    return PixelMatches(renderer, 0U, 1U, lineColor) && PixelMatches(renderer, 2U, 1U, lineColor) &&
           PixelMatches(renderer, 3U, 1U, Color{});
}

[[nodiscard]] bool TestVisualizationTwoPointPolylineMatchesEquivalentLine()
{
    using namespace greenfield;

    const Color lineColor{0.0f, 0.4f, 1.0f, 1.0f};
    Fast2DRenderer lineRenderer{5U, 4U};
    Fast2DRenderer polylineRenderer{5U, 4U};
    VisualizationCommandList lineCommands = MakeVisualizationCommandsWithClip(5U, 4U);
    VisualizationCommandList polylineCommands = MakeVisualizationCommandsWithClip(5U, 4U);
    const std::vector<Vec2> points{Vec2{1.5f, 2.5f}, Vec2{3.5f, 2.5f}};

    if (!lineCommands.AddLine(points[0], points[1], lineColor, 1.0f) ||
        !polylineCommands.AddPolyline(points, lineColor, 1.0f))
    {
        return false;
    }

    lineRenderer.BeginFrame();
    lineRenderer.SubmitVisualization(lineCommands);
    lineRenderer.EndFrame();

    polylineRenderer.BeginFrame();
    polylineRenderer.SubmitVisualization(polylineCommands);
    polylineRenderer.EndFrame();

    return ColorsNearlyMatch(lineRenderer.RasterPixelAt(1U, 2U), polylineRenderer.RasterPixelAt(1U, 2U)) &&
           ColorsNearlyMatch(lineRenderer.RasterPixelAt(3U, 2U), polylineRenderer.RasterPixelAt(3U, 2U)) &&
           ColorsNearlyMatch(lineRenderer.RasterPixelAt(4U, 2U), polylineRenderer.RasterPixelAt(4U, 2U));
}

[[nodiscard]] bool TestVisualizationOpenPolylineDoesNotClose()
{
    using namespace greenfield;

    const Color lineColor{0.0f, 0.9f, 0.4f, 1.0f};
    Fast2DRenderer renderer{5U, 5U};
    VisualizationCommandList visualizationCommands = MakeVisualizationCommandsWithClip(5U, 5U);
    const std::vector<Vec2> points{Vec2{1.5f, 1.5f}, Vec2{3.5f, 1.5f}, Vec2{3.5f, 3.5f}};

    if (!visualizationCommands.AddPolyline(points, lineColor, 1.0f))
    {
        return false;
    }

    renderer.BeginFrame();
    renderer.SubmitVisualization(visualizationCommands);
    renderer.EndFrame();

    return PixelMatches(renderer, 2U, 1U, lineColor) && PixelMatches(renderer, 3U, 2U, lineColor) &&
           PixelMatches(renderer, 2U, 2U, Color{});
}

[[nodiscard]] bool TestVisualizationMultiSegmentPolylineAndRepeatedPointsAreSafe()
{
    using namespace greenfield;

    const Color lineColor{0.9f, 0.9f, 0.1f, 1.0f};
    Fast2DRenderer renderer{5U, 5U};
    VisualizationCommandList visualizationCommands = MakeVisualizationCommandsWithClip(5U, 5U);
    const std::vector<Vec2> points{Vec2{1.5f, 1.5f}, Vec2{1.5f, 1.5f}, Vec2{3.5f, 1.5f}, Vec2{3.5f, 3.5f}};

    if (!visualizationCommands.AddPolyline(points, lineColor, 1.0f))
    {
        return false;
    }

    renderer.BeginFrame();
    renderer.SubmitVisualization(visualizationCommands);
    renderer.EndFrame();

    return PixelMatches(renderer, 1U, 1U, lineColor) && PixelMatches(renderer, 3U, 1U, lineColor) &&
           PixelMatches(renderer, 3U, 3U, lineColor) && PixelMatches(renderer, 0U, 1U, Color{});
}

[[nodiscard]] bool TestVisualizationPolylineJoinBlendsOnlyOnce()
{
    using namespace greenfield;

    const Color destinationColor{0.0f, 0.0f, 1.0f, 1.0f};
    const Color lineColor{1.0f, 0.0f, 0.0f, 0.5f};
    Fast2DRenderer renderer{5U, 4U};
    RenderCommandList renderCommands;
    VisualizationCommandList visualizationCommands = MakeVisualizationCommandsWithClip(5U, 4U);
    const std::vector<Vec2> points{Vec2{1.5f, 1.5f}, Vec2{3.5f, 1.5f}, Vec2{3.5f, 2.5f}};

    renderCommands.AddFillRectangle(FullRasterClip(5U, 4U), destinationColor);
    if (!visualizationCommands.AddPolyline(points, lineColor, 1.0f))
    {
        return false;
    }

    renderer.BeginFrame();
    renderer.Submit(renderCommands);
    renderer.SubmitVisualization(visualizationCommands);
    renderer.EndFrame();

    return PixelNearlyMatches(renderer, 3U, 1U, Color{0.5f, 0.0f, 0.5f, 1.0f});
}

[[nodiscard]] bool TestVisualizationPointMarkerRasterizesExpectedPixels()
{
    using namespace greenfield;

    const Color markerColor{1.0f, 0.0f, 1.0f, 1.0f};
    Fast2DRenderer renderer{5U, 5U};
    VisualizationCommandList visualizationCommands = MakeVisualizationCommandsWithClip(5U, 5U);

    if (!visualizationCommands.AddPointMarker(Vec2{2.5f, 2.5f}, 1.0f, markerColor))
    {
        return false;
    }

    renderer.BeginFrame();
    renderer.SubmitVisualization(visualizationCommands);
    renderer.EndFrame();

    return PixelMatches(renderer, 2U, 2U, markerColor) && PixelMatches(renderer, 3U, 2U, markerColor) &&
           PixelMatches(renderer, 3U, 3U, Color{});
}

[[nodiscard]] bool TestVisualizationFractionalPointMarkerUsesPixelCenterSampling()
{
    using namespace greenfield;

    const Color markerColor{0.4f, 1.0f, 0.2f, 1.0f};
    Fast2DRenderer renderer{4U, 4U};
    VisualizationCommandList visualizationCommands = MakeVisualizationCommandsWithClip(4U, 4U);

    if (!visualizationCommands.AddPointMarker(Vec2{2.25f, 2.5f}, 0.3f, markerColor))
    {
        return false;
    }

    renderer.BeginFrame();
    renderer.SubmitVisualization(visualizationCommands);
    renderer.EndFrame();

    return PixelMatches(renderer, 2U, 2U, markerColor) && PixelMatches(renderer, 1U, 2U, Color{});
}

[[nodiscard]] bool TestVisualizationPartiallyOffscreenMarkerIsSafe()
{
    using namespace greenfield;

    const Color markerColor{0.8f, 0.1f, 0.8f, 1.0f};
    Fast2DRenderer renderer{3U, 3U};
    VisualizationCommandList visualizationCommands = MakeVisualizationCommandsWithClip(3U, 3U);

    if (!visualizationCommands.AddPointMarker(Vec2{-0.5f, 1.5f}, 1.1f, markerColor))
    {
        return false;
    }

    renderer.BeginFrame();
    renderer.SubmitVisualization(visualizationCommands);
    renderer.EndFrame();

    return PixelMatches(renderer, 0U, 1U, markerColor) && PixelMatches(renderer, 1U, 1U, Color{});
}

[[nodiscard]] bool TestVisualizationClipRestrictsLinePolylineAndMarker()
{
    using namespace greenfield;

    const Color lineColor{1.0f, 0.0f, 0.0f, 1.0f};
    const Color polylineColor{0.0f, 1.0f, 0.0f, 1.0f};
    const Color markerColor{0.0f, 0.0f, 1.0f, 1.0f};
    Fast2DRenderer renderer{6U, 6U};
    VisualizationCommandList lineCommands;
    VisualizationCommandList polylineCommands;
    VisualizationCommandList markerCommands;
    const std::vector<Vec2> polylinePoints{Vec2{0.5f, 3.5f}, Vec2{5.5f, 3.5f}};

    if (!lineCommands.SetClipBounds(Rect{.position = Vec2{2.0f, 1.0f}, .size = Vec2{2.0f, 1.0f}}) ||
        !polylineCommands.SetClipBounds(Rect{.position = Vec2{1.0f, 3.0f}, .size = Vec2{2.0f, 1.0f}}) ||
        !markerCommands.SetClipBounds(Rect{.position = Vec2{4.0f, 4.0f}, .size = Vec2{1.0f, 1.0f}}) ||
        !lineCommands.AddLine(Vec2{0.5f, 1.5f}, Vec2{5.5f, 1.5f}, lineColor, 1.0f) ||
        !polylineCommands.AddPolyline(polylinePoints, polylineColor, 1.0f) ||
        !markerCommands.AddPointMarker(Vec2{4.5f, 4.5f}, 1.5f, markerColor))
    {
        return false;
    }

    renderer.BeginFrame();
    renderer.SubmitVisualization(lineCommands);
    renderer.SubmitVisualization(polylineCommands);
    renderer.SubmitVisualization(markerCommands);
    renderer.EndFrame();

    return PixelMatches(renderer, 2U, 1U, lineColor) && PixelMatches(renderer, 1U, 1U, Color{}) &&
           PixelMatches(renderer, 1U, 3U, polylineColor) && PixelMatches(renderer, 3U, 3U, Color{}) &&
           PixelMatches(renderer, 4U, 4U, markerColor) && PixelMatches(renderer, 5U, 4U, Color{});
}

[[nodiscard]] bool TestVisualizationEmptyAndOutsideClipsDrawNothing()
{
    using namespace greenfield;

    const Color lineColor{1.0f, 0.0f, 0.0f, 1.0f};
    Fast2DRenderer renderer{4U, 4U};
    VisualizationCommandList zeroSizeClipCommands;
    VisualizationCommandList outsideClipCommands;

    if (!zeroSizeClipCommands.SetClipBounds(Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{0.0f, 4.0f}}) ||
        !outsideClipCommands.SetClipBounds(Rect{.position = Vec2{10.0f, 10.0f}, .size = Vec2{2.0f, 2.0f}}) ||
        !zeroSizeClipCommands.AddLine(Vec2{0.5f, 1.5f}, Vec2{3.5f, 1.5f}, lineColor, 1.0f) ||
        !outsideClipCommands.AddLine(Vec2{0.5f, 2.5f}, Vec2{3.5f, 2.5f}, lineColor, 1.0f))
    {
        return false;
    }

    renderer.BeginFrame();
    renderer.SubmitVisualization(zeroSizeClipCommands);
    renderer.SubmitVisualization(outsideClipCommands);
    renderer.EndFrame();

    return PixelMatches(renderer, 1U, 1U, Color{}) && PixelMatches(renderer, 2U, 2U, Color{});
}

[[nodiscard]] bool TestVisualizationSourceOverAlphaBlending()
{
    using namespace greenfield;

    Fast2DRenderer renderer{3U, 3U};
    RenderCommandList renderCommands;
    VisualizationCommandList visualizationCommands = MakeVisualizationCommandsWithClip(3U, 3U);
    renderCommands.AddFillRectangle(FullRasterClip(3U, 3U), Color{0.0f, 0.0f, 1.0f, 1.0f});

    if (!visualizationCommands.AddPointMarker(Vec2{1.5f, 1.5f}, 1.0f, Color{1.0f, 0.0f, 0.0f, 0.5f}))
    {
        return false;
    }

    renderer.BeginFrame();
    renderer.Submit(renderCommands);
    renderer.SubmitVisualization(visualizationCommands);
    renderer.EndFrame();

    return PixelNearlyMatches(renderer, 1U, 1U, Color{0.5f, 0.0f, 0.5f, 1.0f});
}

[[nodiscard]] bool TestVisualizationSubmissionCopiesPendingData()
{
    using namespace greenfield;

    const Color lineColor{0.8f, 0.4f, 0.2f, 1.0f};
    Fast2DRenderer renderer{4U, 3U};
    VisualizationCommandList visualizationCommands = MakeVisualizationCommandsWithClip(4U, 3U);

    if (!visualizationCommands.AddLine(Vec2{0.5f, 1.5f}, Vec2{3.5f, 1.5f}, lineColor, 1.0f))
    {
        return false;
    }

    renderer.BeginFrame();
    renderer.SubmitVisualization(visualizationCommands);
    visualizationCommands.Clear();
    renderer.EndFrame();

    return PixelMatches(renderer, 2U, 1U, lineColor);
}

[[nodiscard]] bool TestVisualizationSubmissionOrderIsPreserved()
{
    using namespace greenfield;

    const Color firstColor{0.0f, 0.0f, 1.0f, 1.0f};
    const Color secondColor{1.0f, 0.0f, 0.0f, 1.0f};
    Fast2DRenderer renderer{3U, 3U};
    VisualizationCommandList firstCommands = MakeVisualizationCommandsWithClip(3U, 3U);
    VisualizationCommandList secondCommands = MakeVisualizationCommandsWithClip(3U, 3U);

    if (!firstCommands.AddLine(Vec2{0.5f, 1.5f}, Vec2{2.5f, 1.5f}, firstColor, 1.0f) ||
        !secondCommands.AddLine(Vec2{0.5f, 1.5f}, Vec2{2.5f, 1.5f}, secondColor, 1.0f))
    {
        return false;
    }

    renderer.BeginFrame();
    renderer.SubmitVisualization(firstCommands);
    renderer.SubmitVisualization(secondCommands);
    renderer.EndFrame();

    return PixelMatches(renderer, 1U, 1U, secondColor);
}

[[nodiscard]] bool TestUiThenVisualizationSubmissionOrderIsPreserved()
{
    using namespace greenfield;

    const Color uiColor{0.0f, 0.0f, 1.0f, 1.0f};
    const Color visualizationColor{1.0f, 0.0f, 0.0f, 1.0f};
    Fast2DRenderer renderer{3U, 3U};
    RenderCommandList renderCommands;
    VisualizationCommandList visualizationCommands = MakeVisualizationCommandsWithClip(3U, 3U);
    renderCommands.AddFillRectangle(FullRasterClip(3U, 3U), uiColor);

    if (!visualizationCommands.AddPointMarker(Vec2{1.5f, 1.5f}, 1.0f, visualizationColor))
    {
        return false;
    }

    renderer.BeginFrame();
    renderer.Submit(renderCommands);
    renderer.SubmitVisualization(visualizationCommands);
    renderer.EndFrame();

    return PixelMatches(renderer, 1U, 1U, visualizationColor);
}

[[nodiscard]] bool TestVisualizationThenUiSubmissionOrderIsPreserved()
{
    using namespace greenfield;

    const Color visualizationColor{1.0f, 0.0f, 0.0f, 1.0f};
    const Color uiColor{0.0f, 0.0f, 1.0f, 1.0f};
    Fast2DRenderer renderer{3U, 3U};
    VisualizationCommandList visualizationCommands = MakeVisualizationCommandsWithClip(3U, 3U);
    RenderCommandList renderCommands;
    renderCommands.AddFillRectangle(FullRasterClip(3U, 3U), uiColor);

    if (!visualizationCommands.AddPointMarker(Vec2{1.5f, 1.5f}, 1.0f, visualizationColor))
    {
        return false;
    }

    renderer.BeginFrame();
    renderer.SubmitVisualization(visualizationCommands);
    renderer.Submit(renderCommands);
    renderer.EndFrame();

    return PixelMatches(renderer, 1U, 1U, uiColor);
}

[[nodiscard]] bool TestMixedWorkloadMatchesReferenceBuffer()
{
    using namespace greenfield;

    const Color baseColor{0.0f, 0.0f, 1.0f, 1.0f};
    const Color clippedFillColor{1.0f, 0.0f, 0.0f, 0.5f};
    const Color clippedBlendColor{0.5f, 0.0f, 0.5f, 1.0f};
    const Color lineColor{0.0f, 1.0f, 0.0f, 1.0f};
    const Color overlayColor{1.0f, 1.0f, 0.0f, 1.0f};
    Fast2DRenderer renderer{6U, 5U};
    RenderCommandList renderCommands;
    VisualizationCommandList visualizationCommands = MakeVisualizationCommandsWithClip(6U, 5U);
    RenderCommandList overlayCommands;

    renderCommands.AddFillRectangle(FullRasterClip(6U, 5U), baseColor);
    renderCommands.PushClip(Rect{.position = Vec2{1.0f, 1.0f}, .size = Vec2{4.0f, 3.0f}});
    renderCommands.AddFillRectangle(Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{4.0f, 3.0f}},
                                    clippedFillColor);
    renderCommands.PopClip();
    if (!visualizationCommands.AddLine(Vec2{0.5f, 2.5f}, Vec2{5.5f, 2.5f}, lineColor, 1.0f))
    {
        return false;
    }
    overlayCommands.AddFillRectangle(Rect{.position = Vec2{4.0f, 1.0f}, .size = Vec2{1.0f, 3.0f}}, overlayColor);

    renderer.BeginFrame();
    renderer.Submit(renderCommands);
    renderer.SubmitVisualization(visualizationCommands);
    renderer.Submit(overlayCommands);
    renderer.EndFrame();

    std::vector<Color> expectedPixels(30U, baseColor);
    for (std::size_t y = 1U; y < 3U; ++y)
    {
        for (std::size_t x = 1U; x < 4U; ++x)
        {
            expectedPixels[y * 6U + x] = clippedBlendColor;
        }
    }

    for (std::size_t x = 0U; x < 6U; ++x)
    {
        expectedPixels[2U * 6U + x] = lineColor;
    }

    for (std::size_t y = 1U; y < 4U; ++y)
    {
        expectedPixels[y * 6U + 4U] = overlayColor;
    }

    const auto rasterPixels = renderer.RasterPixels();
    if (rasterPixels.size() != expectedPixels.size())
    {
        return false;
    }

    for (std::size_t pixelIndex = 0U; pixelIndex < expectedPixels.size(); ++pixelIndex)
    {
        if (!ColorsNearlyMatch(rasterPixels[pixelIndex], expectedPixels[pixelIndex]))
        {
            return false;
        }
    }

    return true;
}

[[nodiscard]] bool TestVisualizationClipStateDoesNotLeak()
{
    using namespace greenfield;

    const Color firstColor{1.0f, 0.0f, 0.0f, 1.0f};
    const Color secondColor{0.0f, 1.0f, 0.0f, 1.0f};
    Fast2DRenderer renderer{5U, 3U};
    VisualizationCommandList clippedCommands;
    VisualizationCommandList fullCommands = MakeVisualizationCommandsWithClip(5U, 3U);

    if (!clippedCommands.SetClipBounds(Rect{.position = Vec2{0.0f, 1.0f}, .size = Vec2{1.0f, 1.0f}}) ||
        !clippedCommands.AddLine(Vec2{0.5f, 1.5f}, Vec2{4.5f, 1.5f}, firstColor, 1.0f) ||
        !fullCommands.AddLine(Vec2{0.5f, 1.5f}, Vec2{4.5f, 1.5f}, secondColor, 1.0f))
    {
        return false;
    }

    renderer.BeginFrame();
    renderer.SubmitVisualization(clippedCommands);
    renderer.SubmitVisualization(fullCommands);
    renderer.EndFrame();

    return PixelMatches(renderer, 0U, 1U, secondColor) && PixelMatches(renderer, 3U, 1U, secondColor);
}

[[nodiscard]] bool TestBeginFrameClearsPendingVisualizationState()
{
    using namespace greenfield;

    const Color lineColor{1.0f, 0.0f, 0.0f, 1.0f};
    Fast2DRenderer renderer{3U, 3U};
    VisualizationCommandList visualizationCommands = MakeVisualizationCommandsWithClip(3U, 3U);

    if (!visualizationCommands.AddLine(Vec2{0.5f, 1.5f}, Vec2{2.5f, 1.5f}, lineColor, 1.0f))
    {
        return false;
    }

    renderer.BeginFrame();
    renderer.SubmitVisualization(visualizationCommands);
    renderer.BeginFrame();
    renderer.EndFrame();

    return renderer.SubmittedVisualizationCommandCount() == 0U && PixelMatches(renderer, 1U, 1U, Color{});
}

[[nodiscard]] bool TestVisualizationEmptyRasterTargetAndResizeAreSafe()
{
    using namespace greenfield;

    const Color lineColor{0.3f, 0.7f, 0.1f, 1.0f};
    Fast2DRenderer renderer{0U, 0U};
    VisualizationCommandList visualizationCommands = MakeVisualizationCommandsWithClip(3U, 3U);

    if (!visualizationCommands.AddLine(Vec2{0.5f, 1.5f}, Vec2{2.5f, 1.5f}, lineColor, 1.0f))
    {
        return false;
    }

    renderer.BeginFrame();
    renderer.SubmitVisualization(visualizationCommands);
    renderer.EndFrame();
    if (!renderer.RasterPixels().empty())
    {
        return false;
    }

    renderer.ResizeRasterTarget(3U, 3U);
    renderer.BeginFrame();
    renderer.SubmitVisualization(visualizationCommands);
    renderer.EndFrame();

    return PixelMatches(renderer, 1U, 1U, lineColor);
}

} // namespace

int main()
{
    if (!TestConstructionSucceeds() || !TestBeginFrameClearsSubmittedCommands() ||
        !TestSubmitRecordsAndAppendsCommands() || !TestFillRectangleIsPrepared() ||
        !TestMultipleFillRectanglesArePreparedInOrder() || !TestClipStackAffectsPreparedFillOperations() ||
        !TestClipUnderflowIsSafeAndObservable() || !TestDrawTextIsDeferred() ||
        !TestBeginFrameResetsPreparedState() || !TestEndFrameCompletesWithoutPlatformDependencies() ||
        !TestFillRectangleWritesRasterPixels() || !TestFillRectanglePreservesStyleMetadataAndRasterizesBorder() ||
        !TestZeroCornerRadiusPreservesPlainRectangleFill() ||
        !TestPositiveCornerRadiusLeavesExtremeCornersUntouched() ||
        !TestPositiveCornerRadiusFillsCenterAndBodyPixels() ||
        !TestRoundedFillUsesSourceOverAlphaBlending() || !TestRoundedFillRespectsClip() ||
        !TestLargeCornerRadiusOnSmallRectangleIsDeterministic() ||
        !TestRoundedBorderLeavesExtremeCornersUntouched() || !TestRoundedBorderDrawsEdgeAndBodyPixels() ||
        !TestRoundedBorderDrawsAfterFill() || !TestPartialAlphaRoundedBorderBlendsOverFill() ||
        !TestRoundedBorderRespectsNestedClips() ||
        !TestLargeRoundedBorderThicknessCoversSmallRectangleDeterministically() ||
        !TestLaterFillRectangleOverridesEarlierRasterPixels() || !TestClipLimitsRasterPixels() ||
        !TestNestedOverlappingClipsLimitRasterPixelsToIntersection() ||
        !TestNonOverlappingNestedClipsModifyNoRasterPixels() ||
        !TestOpaqueFillStillReplacesRasterPixels() || !TestTransparentFillLeavesRasterPixelsUnchanged() ||
        !TestPartialAlphaFillBlendsOverOpaqueDestination() ||
        !TestPartialAlphaFillBlendsAgainstExistingRasterPixels() ||
        !TestNestedClipsLimitAlphaBlendedFillToIntersection() ||
        !TestAlphaBlendedFillStillHonorsClip() || !TestZeroBorderThicknessPreservesFillOnlyRasterPath() ||
        !TestNegativeBorderThicknessPreservesFillOnlyRasterPath() || !TestBorderDrawsAfterFill() ||
        !TestTransparentBorderLeavesFillUnchanged() || !TestPartialAlphaBorderBlendsOverFill() ||
        !TestBorderRespectsClip() || !TestLargeBorderThicknessCoversSmallRectangleDeterministically() ||
        !TestNestedClipsLimitBorderRasterizationToIntersection() ||
        !TestNestedClipsLimitRoundedFillRasterizationToIntersection() ||
        !TestPopClipRestoresPreviousRasterClip() ||
        !TestOutOfBoundsRectanglesAreClippedToRasterTarget() ||
        !TestClipOutsideRasterTargetIsClippedSafely() || !TestDrawTextDoesNotAlterRasterPixels() ||
        !TestBeginFrameResetsRasterPixels() || !TestEmptyVisualizationCommandListChangesNoPixels() ||
        !TestVisualizationHorizontalLineRasterizesExpectedPixels() ||
        !TestVisualizationVerticalLineRasterizesExpectedPixels() ||
        !TestVisualizationDiagonalLineRasterizesDeterministically() ||
        !TestVisualizationFractionalLineUsesPixelCenterSampling() ||
        !TestVisualizationStrokeThicknessChangesCoverage() ||
        !TestVisualizationZeroLengthLineRendersRoundMark() ||
        !TestVisualizationPartiallyOffscreenLineIsSafe() ||
        !TestVisualizationTwoPointPolylineMatchesEquivalentLine() ||
        !TestVisualizationOpenPolylineDoesNotClose() ||
        !TestVisualizationMultiSegmentPolylineAndRepeatedPointsAreSafe() ||
        !TestVisualizationPolylineJoinBlendsOnlyOnce() ||
        !TestVisualizationPointMarkerRasterizesExpectedPixels() ||
        !TestVisualizationFractionalPointMarkerUsesPixelCenterSampling() ||
        !TestVisualizationPartiallyOffscreenMarkerIsSafe() ||
        !TestVisualizationClipRestrictsLinePolylineAndMarker() ||
        !TestVisualizationEmptyAndOutsideClipsDrawNothing() ||
        !TestVisualizationSourceOverAlphaBlending() ||
        !TestVisualizationSubmissionCopiesPendingData() ||
        !TestVisualizationSubmissionOrderIsPreserved() ||
        !TestUiThenVisualizationSubmissionOrderIsPreserved() ||
        !TestVisualizationThenUiSubmissionOrderIsPreserved() ||
        !TestMixedWorkloadMatchesReferenceBuffer() ||
        !TestVisualizationClipStateDoesNotLeak() ||
        !TestBeginFrameClearsPendingVisualizationState() ||
        !TestVisualizationEmptyRasterTargetAndResizeAreSafe())
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

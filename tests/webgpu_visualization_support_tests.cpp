#include <cmath>
#include <cstdlib>
#include <variant>
#include <vector>

#include "engine/render/webgpu/WebGpuVisualizationSupport.h"

namespace
{

[[nodiscard]] bool ScissorMatches(const greenfield::WebGpuScissorRectangle& scissorRectangle,
                                  std::uint32_t x,
                                  std::uint32_t y,
                                  std::uint32_t width,
                                  std::uint32_t height) noexcept
{
    return scissorRectangle.x == x && scissorRectangle.y == y && scissorRectangle.width == width &&
           scissorRectangle.height == height;
}

[[nodiscard]] greenfield::Rect FullClip(float width = 100.0f, float height = 100.0f)
{
    return greenfield::Rect{
        .position = greenfield::Vec2{0.0f, 0.0f},
        .size = greenfield::Vec2{width, height},
    };
}

[[nodiscard]] greenfield::VisualizationCommandList MakeVisualizationCommands()
{
    greenfield::VisualizationCommandList visualizationCommands;
    if (!visualizationCommands.SetClipBounds(FullClip()))
    {
        return {};
    }

    return visualizationCommands;
}

[[nodiscard]] bool RectanglesNearlyMatch(const greenfield::Rect& firstRectangle,
                                         const greenfield::Rect& secondRectangle) noexcept
{
    constexpr float Tolerance = 0.00001f;
    return std::abs(firstRectangle.position.x - secondRectangle.position.x) <= Tolerance &&
           std::abs(firstRectangle.position.y - secondRectangle.position.y) <= Tolerance &&
           std::abs(firstRectangle.size.x - secondRectangle.size.x) <= Tolerance &&
           std::abs(firstRectangle.size.y - secondRectangle.size.y) <= Tolerance;
}

[[nodiscard]] bool TestSubmissionQueuePreservesMixedOrder()
{
    using namespace greenfield;

    WebGpuSubmissionQueue submissionQueue;
    RenderCommandList firstRenderCommands;
    RenderCommandList secondRenderCommands;
    VisualizationCommandList visualizationCommands = MakeVisualizationCommands();

    firstRenderCommands.AddFillRectangle(Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{10.0f, 10.0f}}, Color{});
    if (!visualizationCommands.AddPointMarker(Vec2{5.0f, 6.0f}, 2.0f, Color{1.0f, 0.0f, 0.0f, 1.0f}))
    {
        return false;
    }
    secondRenderCommands.AddText("after", Rect{.position = Vec2{2.0f, 3.0f}, .size = Vec2{40.0f, 20.0f}}, 12.0f, Color{});

    submissionQueue.QueueRenderCommands(firstRenderCommands);
    submissionQueue.QueueVisualizationCommands(visualizationCommands);
    submissionQueue.QueueRenderCommands(secondRenderCommands);

    const auto submissions = submissionQueue.Submissions();
    return submissions.size() == 3U &&
           std::holds_alternative<WebGpuQueuedRenderSubmission>(submissions[0]) &&
           std::holds_alternative<WebGpuQueuedVisualizationSubmission>(submissions[1]) &&
           std::holds_alternative<WebGpuQueuedRenderSubmission>(submissions[2]) &&
           submissionQueue.SubmittedRenderCommandCount() == 2U &&
           submissionQueue.SubmittedVisualizationCommandCount() == 1U;
}

[[nodiscard]] bool TestVisualizationSubmissionOwnsCopiedCommands()
{
    using namespace greenfield;

    std::vector<Vec2> points{
        Vec2{1.0f, 1.0f},
        Vec2{2.0f, 2.0f},
        Vec2{3.0f, 1.0f},
    };
    VisualizationCommandList visualizationCommands = MakeVisualizationCommands();
    if (!visualizationCommands.AddPolyline(points, Color{0.2f, 0.3f, 0.4f, 0.5f}, 3.0f))
    {
        return false;
    }

    WebGpuSubmissionQueue submissionQueue;
    submissionQueue.QueueVisualizationCommands(visualizationCommands);

    points[1] = Vec2{80.0f, 80.0f};
    visualizationCommands.Clear();

    const auto submissions = submissionQueue.Submissions();
    const auto* queuedVisualization = std::get_if<WebGpuQueuedVisualizationSubmission>(&submissions[0]);
    if (queuedVisualization == nullptr || queuedVisualization->commands.size() != 1U)
    {
        return false;
    }

    const auto* queuedPolyline =
        std::get_if<VisualizationPolylineCommand>(&queuedVisualization->commands[0]);
    return queuedPolyline != nullptr && queuedPolyline->points.size() == 3U &&
           queuedPolyline->points[1].x == 2.0f && queuedPolyline->points[1].y == 2.0f;
}

[[nodiscard]] bool TestSubmissionQueueClearResetsPerFrameCounts()
{
    using namespace greenfield;

    WebGpuSubmissionQueue submissionQueue;
    RenderCommandList renderCommands;
    VisualizationCommandList visualizationCommands = MakeVisualizationCommands();
    renderCommands.AddFillRectangle(Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{1.0f, 1.0f}}, Color{});
    if (!visualizationCommands.AddPointMarker(Vec2{1.0f, 1.0f}, 1.0f, Color{}))
    {
        return false;
    }

    submissionQueue.QueueRenderCommands(renderCommands);
    submissionQueue.QueueVisualizationCommands(visualizationCommands);
    submissionQueue.Clear();

    return submissionQueue.Submissions().empty() && submissionQueue.SubmittedRenderCommandCount() == 0U &&
           submissionQueue.SubmittedVisualizationCommandCount() == 0U;
}

[[nodiscard]] bool TestVisualizationScissorUsesPixelCenterHalfOpenSemantics()
{
    using namespace greenfield;

    return ScissorMatches(MakeWebGpuVisualizationScissorRectangle(FullClip(10.0f, 8.0f), 20U, 20U),
                          0U,
                          0U,
                          10U,
                          8U) &&
           ScissorMatches(MakeWebGpuVisualizationScissorRectangle(
                              Rect{.position = Vec2{0.6f, 0.6f}, .size = Vec2{1.0f, 1.0f}}, 10U, 10U),
                          1U,
                          1U,
                          1U,
                          1U) &&
           ScissorMatches(MakeWebGpuVisualizationScissorRectangle(
                              Rect{.position = Vec2{2.2f, 3.7f}, .size = Vec2{5.0f, 4.6f}}, 20U, 20U),
                          2U,
                          4U,
                          5U,
                          4U);
}

[[nodiscard]] bool TestVisualizationScissorRejectsEmptyAndOutsideClips()
{
    using namespace greenfield;

    return ScissorMatches(MakeWebGpuVisualizationScissorRectangle(
                              Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{0.0f, 4.0f}}, 10U, 10U),
                          0U,
                          0U,
                          0U,
                          0U) &&
           ScissorMatches(MakeWebGpuVisualizationScissorRectangle(
                              Rect{.position = Vec2{20.0f, 20.0f}, .size = Vec2{4.0f, 4.0f}}, 10U, 10U),
                          0U,
                          0U,
                          0U,
                          0U) &&
           ScissorMatches(MakeWebGpuVisualizationScissorRectangle(FullClip(), 0U, 10U),
                          0U,
                          0U,
                          0U,
                          0U);
}

[[nodiscard]] bool TestLineCoverageBoundsHandleCommonShapes()
{
    using namespace greenfield;

    const VisualizationLineCommand horizontal{
        .start = Vec2{2.0f, 4.0f},
        .end = Vec2{8.0f, 4.0f},
        .strokeThickness = 4.0f,
    };
    const VisualizationLineCommand zeroLength{
        .start = Vec2{5.0f, 5.0f},
        .end = Vec2{5.0f, 5.0f},
        .strokeThickness = 2.0f,
    };
    const VisualizationLineCommand offscreen{
        .start = Vec2{-3.0f, -2.0f},
        .end = Vec2{1.0f, 6.0f},
        .strokeThickness = 2.0f,
    };

    return RectanglesNearlyMatch(MakeWebGpuLineCoverageBounds(horizontal),
                                 Rect{.position = Vec2{0.0f, 2.0f}, .size = Vec2{10.0001f, 4.0001f}}) &&
           RectanglesNearlyMatch(MakeWebGpuLineCoverageBounds(zeroLength),
                                 Rect{.position = Vec2{4.0f, 4.0f}, .size = Vec2{2.0001f, 2.0001f}}) &&
           RectanglesNearlyMatch(MakeWebGpuLineCoverageBounds(offscreen),
                                 Rect{.position = Vec2{-4.0f, -3.0f}, .size = Vec2{6.0001f, 10.0001f}});
}

[[nodiscard]] bool TestPointMarkerCoverageBounds()
{
    using namespace greenfield;

    const VisualizationPointMarkerCommand pointMarker{
        .center = Vec2{4.5f, 7.25f},
        .radius = 2.5f,
    };

    return RectanglesNearlyMatch(MakeWebGpuPointMarkerCoverageBounds(pointMarker),
                                 Rect{.position = Vec2{2.0f, 4.75f}, .size = Vec2{5.0001f, 5.0001f}});
}

[[nodiscard]] bool TestPolylinePreparationKeepsOpenCopiedPointStorage()
{
    using namespace greenfield;

    VisualizationCommandList visualizationCommands = MakeVisualizationCommands();
    const std::vector<Vec2> points{
        Vec2{1.0f, 1.0f},
        Vec2{4.0f, 1.0f},
        Vec2{4.0f, 5.0f},
        Vec2{4.0f, 5.0f},
    };
    if (!visualizationCommands.AddPolyline(points, Color{0.1f, 0.2f, 0.3f, 0.4f}, 2.0f))
    {
        return false;
    }

    WebGpuSubmissionQueue submissionQueue;
    submissionQueue.QueueVisualizationCommands(visualizationCommands);
    const auto* queuedVisualization =
        std::get_if<WebGpuQueuedVisualizationSubmission>(&submissionQueue.Submissions()[0]);
    const WebGpuPreparedVisualizationSubmission preparedSubmission =
        PrepareWebGpuVisualizationSubmission(*queuedVisualization, 100U, 100U);

    if (preparedSubmission.primitives.size() != 1U || preparedSubmission.polylinePoints.size() != 4U)
    {
        return false;
    }

    const WebGpuPreparedVisualizationPrimitive& primitive = preparedSubmission.primitives[0];
    return primitive.kind == WebGpuPreparedVisualizationPrimitiveKind::Polyline &&
           primitive.pointOffset == 0U && primitive.pointCount == 4U &&
           RectanglesNearlyMatch(primitive.coverageBounds,
                                 Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{5.0001f, 6.0001f}}) &&
           preparedSubmission.polylinePoints[3].x == 4.0f && preparedSubmission.polylinePoints[3].y == 5.0f;
}

[[nodiscard]] bool TestEmptyVisualizationPreparationIsSafe()
{
    using namespace greenfield;

    VisualizationCommandList visualizationCommands = MakeVisualizationCommands();
    WebGpuSubmissionQueue submissionQueue;
    submissionQueue.QueueVisualizationCommands(visualizationCommands);

    const auto* queuedVisualization =
        std::get_if<WebGpuQueuedVisualizationSubmission>(&submissionQueue.Submissions()[0]);
    const WebGpuPreparedVisualizationSubmission preparedSubmission =
        PrepareWebGpuVisualizationSubmission(*queuedVisualization, 100U, 100U);

    return preparedSubmission.primitives.empty() && preparedSubmission.polylinePoints.empty();
}

} // namespace

int main()
{
    const bool passed = TestSubmissionQueuePreservesMixedOrder() &&
                        TestVisualizationSubmissionOwnsCopiedCommands() &&
                        TestSubmissionQueueClearResetsPerFrameCounts() &&
                        TestVisualizationScissorUsesPixelCenterHalfOpenSemantics() &&
                        TestVisualizationScissorRejectsEmptyAndOutsideClips() &&
                        TestLineCoverageBoundsHandleCommonShapes() &&
                        TestPointMarkerCoverageBounds() &&
                        TestPolylinePreparationKeepsOpenCopiedPointStorage() &&
                        TestEmptyVisualizationPreparationIsSafe();

    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}

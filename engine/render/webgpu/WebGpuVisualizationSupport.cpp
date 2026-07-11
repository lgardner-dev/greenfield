#include "engine/render/webgpu/WebGpuVisualizationSupport.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace greenfield
{
namespace
{

constexpr float CoverageBoundsEpsilon = 0.0001f;

[[nodiscard]] bool HasPositiveArea(const Rect& rectangle) noexcept
{
    return rectangle.size.x > 0.0f && rectangle.size.y > 0.0f;
}

[[nodiscard]] Rect MakeRectangleFromEdges(float left, float top, float right, float bottom) noexcept
{
    return Rect{
        .position = Vec2{.x = left, .y = top},
        .size = Vec2{.x = std::max(0.0f, right - left), .y = std::max(0.0f, bottom - top)},
    };
}

[[nodiscard]] std::uint32_t ClampPixelBound(float value, std::uint32_t upperBound) noexcept
{
    if (value <= 0.0f)
    {
        return 0;
    }

    const float floatingUpperBound = static_cast<float>(upperBound);
    if (value >= floatingUpperBound)
    {
        return upperBound;
    }

    return static_cast<std::uint32_t>(value);
}

[[nodiscard]] std::uint32_t ToPointStorageIndex(std::size_t index)
{
    if (index > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()))
    {
        throw std::runtime_error("WebGPU visualization point storage exceeded 32-bit shader indexing.");
    }

    return static_cast<std::uint32_t>(index);
}

void AppendLinePrimitive(WebGpuPreparedVisualizationSubmission& preparedSubmission,
                         const VisualizationLineCommand& lineCommand)
{
    preparedSubmission.primitives.push_back(WebGpuPreparedVisualizationPrimitive{
        .kind = WebGpuPreparedVisualizationPrimitiveKind::Line,
        .coverageBounds = MakeWebGpuLineCoverageBounds(lineCommand),
        .firstPoint = lineCommand.start,
        .secondPoint = lineCommand.end,
        .color = lineCommand.strokeColor,
        .thicknessOrRadius = lineCommand.strokeThickness,
    });
}

void AppendPolylinePrimitive(WebGpuPreparedVisualizationSubmission& preparedSubmission,
                             const VisualizationPolylineCommand& polylineCommand)
{
    if (polylineCommand.points.size() < 2U)
    {
        return;
    }

    const std::uint32_t pointOffset = ToPointStorageIndex(preparedSubmission.polylinePoints.size());
    const std::uint32_t pointCount = ToPointStorageIndex(polylineCommand.points.size());
    preparedSubmission.polylinePoints.insert(preparedSubmission.polylinePoints.end(),
                                             polylineCommand.points.begin(),
                                             polylineCommand.points.end());

    preparedSubmission.primitives.push_back(WebGpuPreparedVisualizationPrimitive{
        .kind = WebGpuPreparedVisualizationPrimitiveKind::Polyline,
        .coverageBounds = MakeWebGpuPolylineCoverageBounds(polylineCommand),
        .color = polylineCommand.strokeColor,
        .thicknessOrRadius = polylineCommand.strokeThickness,
        .pointOffset = pointOffset,
        .pointCount = pointCount,
    });
}

void AppendPointMarkerPrimitive(WebGpuPreparedVisualizationSubmission& preparedSubmission,
                                const VisualizationPointMarkerCommand& pointMarkerCommand)
{
    preparedSubmission.primitives.push_back(WebGpuPreparedVisualizationPrimitive{
        .kind = WebGpuPreparedVisualizationPrimitiveKind::PointMarker,
        .coverageBounds = MakeWebGpuPointMarkerCoverageBounds(pointMarkerCommand),
        .firstPoint = pointMarkerCommand.center,
        .color = pointMarkerCommand.fillColor,
        .thicknessOrRadius = pointMarkerCommand.radius,
    });
}

} // namespace

void WebGpuSubmissionQueue::Clear() noexcept
{
    _submissions.clear();
    _submittedRenderCommandCount = 0;
    _submittedVisualizationCommandCount = 0;
}

void WebGpuSubmissionQueue::QueueRenderCommands(const RenderCommandList& renderCommands)
{
    _submissions.push_back(WebGpuQueuedRenderSubmission{
        .commands = std::vector<RenderCommand>(renderCommands.Commands().begin(), renderCommands.Commands().end()),
    });
    _submittedRenderCommandCount += renderCommands.Size();
}

void WebGpuSubmissionQueue::QueueVisualizationCommands(const VisualizationCommandList& visualizationCommands)
{
    _submissions.push_back(WebGpuQueuedVisualizationSubmission{
        .clipBounds = visualizationCommands.ClipBounds(),
        .commands = std::vector<VisualizationCommand>(visualizationCommands.Commands().begin(),
                                                      visualizationCommands.Commands().end()),
    });
    _submittedVisualizationCommandCount += visualizationCommands.Size();
}

std::size_t WebGpuSubmissionQueue::SubmittedRenderCommandCount() const noexcept
{
    return _submittedRenderCommandCount;
}

std::size_t WebGpuSubmissionQueue::SubmittedVisualizationCommandCount() const noexcept
{
    return _submittedVisualizationCommandCount;
}

std::span<const WebGpuQueuedSubmission> WebGpuSubmissionQueue::Submissions() const noexcept
{
    return std::span<const WebGpuQueuedSubmission>(_submissions);
}

Rect MakeWebGpuLineCoverageBounds(const VisualizationLineCommand& lineCommand) noexcept
{
    const float halfThickness = lineCommand.strokeThickness * 0.5f;
    return MakeRectangleFromEdges(std::min(lineCommand.start.x, lineCommand.end.x) - halfThickness,
                                  std::min(lineCommand.start.y, lineCommand.end.y) - halfThickness,
                                  std::max(lineCommand.start.x, lineCommand.end.x) + halfThickness +
                                      CoverageBoundsEpsilon,
                                  std::max(lineCommand.start.y, lineCommand.end.y) + halfThickness +
                                      CoverageBoundsEpsilon);
}

Rect MakeWebGpuPolylineCoverageBounds(const VisualizationPolylineCommand& polylineCommand) noexcept
{
    if (polylineCommand.points.empty())
    {
        return {};
    }

    float left = polylineCommand.points[0].x;
    float top = polylineCommand.points[0].y;
    float right = polylineCommand.points[0].x;
    float bottom = polylineCommand.points[0].y;

    for (Vec2 point : polylineCommand.points)
    {
        left = std::min(left, point.x);
        top = std::min(top, point.y);
        right = std::max(right, point.x);
        bottom = std::max(bottom, point.y);
    }

    const float halfThickness = polylineCommand.strokeThickness * 0.5f;
    return MakeRectangleFromEdges(left - halfThickness,
                                  top - halfThickness,
                                  right + halfThickness + CoverageBoundsEpsilon,
                                  bottom + halfThickness + CoverageBoundsEpsilon);
}

Rect MakeWebGpuPointMarkerCoverageBounds(const VisualizationPointMarkerCommand& pointMarkerCommand) noexcept
{
    return MakeRectangleFromEdges(pointMarkerCommand.center.x - pointMarkerCommand.radius,
                                  pointMarkerCommand.center.y - pointMarkerCommand.radius,
                                  pointMarkerCommand.center.x + pointMarkerCommand.radius + CoverageBoundsEpsilon,
                                  pointMarkerCommand.center.y + pointMarkerCommand.radius + CoverageBoundsEpsilon);
}

WebGpuScissorRectangle MakeWebGpuVisualizationScissorRectangle(const Rect& clipBounds,
                                                               std::uint32_t targetWidth,
                                                               std::uint32_t targetHeight) noexcept
{
    if (targetWidth == 0 || targetHeight == 0 || !HasPositiveArea(clipBounds))
    {
        return {};
    }

    const float left = std::ceil(clipBounds.position.x - 0.5f);
    const float top = std::ceil(clipBounds.position.y - 0.5f);
    const float right = std::ceil(clipBounds.position.x + clipBounds.size.x - 0.5f);
    const float bottom = std::ceil(clipBounds.position.y + clipBounds.size.y - 0.5f);

    const std::uint32_t x = ClampPixelBound(left, targetWidth);
    const std::uint32_t y = ClampPixelBound(top, targetHeight);
    const std::uint32_t rightEdge = ClampPixelBound(right, targetWidth);
    const std::uint32_t bottomEdge = ClampPixelBound(bottom, targetHeight);
    if (rightEdge <= x || bottomEdge <= y)
    {
        return {};
    }

    return WebGpuScissorRectangle{
        .x = x,
        .y = y,
        .width = rightEdge - x,
        .height = bottomEdge - y,
    };
}

WebGpuPreparedVisualizationSubmission PrepareWebGpuVisualizationSubmission(
    const WebGpuQueuedVisualizationSubmission& visualizationSubmission,
    std::uint32_t targetWidth,
    std::uint32_t targetHeight)
{
    WebGpuPreparedVisualizationSubmission preparedSubmission{
        .scissorRectangle =
            MakeWebGpuVisualizationScissorRectangle(visualizationSubmission.clipBounds, targetWidth, targetHeight),
        .primitives = {},
        .polylinePoints = {},
    };

    if (preparedSubmission.scissorRectangle.width == 0 || preparedSubmission.scissorRectangle.height == 0)
    {
        return preparedSubmission;
    }

    for (const VisualizationCommand& visualizationCommand : visualizationSubmission.commands)
    {
        if (const VisualizationLineCommand* lineCommand =
                std::get_if<VisualizationLineCommand>(&visualizationCommand))
        {
            AppendLinePrimitive(preparedSubmission, *lineCommand);
            continue;
        }

        if (const VisualizationPolylineCommand* polylineCommand =
                std::get_if<VisualizationPolylineCommand>(&visualizationCommand))
        {
            AppendPolylinePrimitive(preparedSubmission, *polylineCommand);
            continue;
        }

        if (const VisualizationPointMarkerCommand* pointMarkerCommand =
                std::get_if<VisualizationPointMarkerCommand>(&visualizationCommand))
        {
            AppendPointMarkerPrimitive(preparedSubmission, *pointMarkerCommand);
        }
    }

    return preparedSubmission;
}

} // namespace greenfield

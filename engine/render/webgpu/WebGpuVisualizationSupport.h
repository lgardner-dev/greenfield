#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <variant>
#include <vector>

#include "engine/core/Color.h"
#include "engine/core/Rect.h"
#include "engine/core/Vec2.h"
#include "engine/render/RenderCommand.h"
#include "engine/render/RenderCommandList.h"
#include "engine/visualization/VisualizationCommand.h"
#include "engine/visualization/VisualizationCommandList.h"

namespace greenfield
{

struct WebGpuScissorRectangle
{
    std::uint32_t x{0};
    std::uint32_t y{0};
    std::uint32_t width{0};
    std::uint32_t height{0};
};

struct WebGpuQueuedRenderSubmission
{
    std::vector<RenderCommand> commands;
};

struct WebGpuQueuedVisualizationSubmission
{
    Rect clipBounds{};
    std::vector<VisualizationCommand> commands;
};

using WebGpuQueuedSubmission =
    std::variant<WebGpuQueuedRenderSubmission, WebGpuQueuedVisualizationSubmission>;

class WebGpuSubmissionQueue
{
public:
    void Clear() noexcept;
    void QueueRenderCommands(const RenderCommandList& renderCommands);
    void QueueVisualizationCommands(const VisualizationCommandList& visualizationCommands);

    [[nodiscard]] std::size_t SubmittedRenderCommandCount() const noexcept;
    [[nodiscard]] std::size_t SubmittedVisualizationCommandCount() const noexcept;
    [[nodiscard]] std::span<const WebGpuQueuedSubmission> Submissions() const noexcept;

private:
    std::vector<WebGpuQueuedSubmission> _submissions;
    std::size_t _submittedRenderCommandCount{0};
    std::size_t _submittedVisualizationCommandCount{0};
};

enum class WebGpuPreparedVisualizationPrimitiveKind : std::uint32_t
{
    Line = 0,
    Polyline = 1,
    PointMarker = 2,
};

struct WebGpuPreparedVisualizationPrimitive
{
    WebGpuPreparedVisualizationPrimitiveKind kind{WebGpuPreparedVisualizationPrimitiveKind::Line};
    Rect coverageBounds{};
    Vec2 firstPoint{};
    Vec2 secondPoint{};
    Color color{};
    float thicknessOrRadius{1.0f};
    std::uint32_t pointOffset{0};
    std::uint32_t pointCount{0};
};

struct WebGpuPreparedVisualizationSubmission
{
    WebGpuScissorRectangle scissorRectangle{};
    std::vector<WebGpuPreparedVisualizationPrimitive> primitives;
    std::vector<Vec2> polylinePoints;
};

[[nodiscard]] Rect MakeWebGpuLineCoverageBounds(const VisualizationLineCommand& lineCommand) noexcept;
[[nodiscard]] Rect MakeWebGpuPolylineCoverageBounds(const VisualizationPolylineCommand& polylineCommand) noexcept;
[[nodiscard]] Rect MakeWebGpuPointMarkerCoverageBounds(
    const VisualizationPointMarkerCommand& pointMarkerCommand) noexcept;
[[nodiscard]] WebGpuScissorRectangle MakeWebGpuVisualizationScissorRectangle(
    const Rect& clipBounds, std::uint32_t targetWidth, std::uint32_t targetHeight) noexcept;
[[nodiscard]] WebGpuPreparedVisualizationSubmission PrepareWebGpuVisualizationSubmission(
    const WebGpuQueuedVisualizationSubmission& visualizationSubmission,
    std::uint32_t targetWidth,
    std::uint32_t targetHeight);

} // namespace greenfield

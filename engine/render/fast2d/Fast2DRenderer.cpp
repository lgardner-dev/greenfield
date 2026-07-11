#include "engine/render/fast2d/Fast2DRenderer.h"

#include <algorithm>
#include <cmath>

namespace greenfield
{
namespace
{

constexpr float CoverageBoundsEpsilon = 0.0001f;

[[nodiscard]] bool HasPositiveArea(const Rect& rectangle) noexcept
{
    return rectangle.size.x > 0.0f && rectangle.size.y > 0.0f;
}

[[nodiscard]] std::size_t GetPixelIndex(std::size_t x, std::size_t y, std::size_t width) noexcept
{
    return y * width + x;
}

[[nodiscard]] Rect MakeRasterBounds(std::size_t width, std::size_t height) noexcept
{
    return Rect{
        .position = Vec2{0.0f, 0.0f},
        .size = Vec2{static_cast<float>(width), static_cast<float>(height)},
    };
}

[[nodiscard]] Rect MakeRectangleFromEdges(float left, float top, float right, float bottom) noexcept
{
    return Rect{
        .position = Vec2{.x = left, .y = top},
        .size = Vec2{.x = std::max(0.0f, right - left), .y = std::max(0.0f, bottom - top)},
    };
}

struct PixelBounds
{
    std::size_t left{0};
    std::size_t top{0};
    std::size_t right{0};
    std::size_t bottom{0};
};

[[nodiscard]] PixelBounds MakePixelBounds(const Rect& rectangle) noexcept
{
    return PixelBounds{
        .left = static_cast<std::size_t>(std::ceil(rectangle.position.x)),
        .top = static_cast<std::size_t>(std::ceil(rectangle.position.y)),
        .right = static_cast<std::size_t>(std::ceil(rectangle.position.x + rectangle.size.x)),
        .bottom = static_cast<std::size_t>(std::ceil(rectangle.position.y + rectangle.size.y)),
    };
}

[[nodiscard]] std::size_t ClampPixelBound(float value, std::size_t upperBound) noexcept
{
    if (value <= 0.0f)
    {
        return 0U;
    }

    const float floatingUpperBound = static_cast<float>(upperBound);
    if (value >= floatingUpperBound)
    {
        return upperBound;
    }

    return static_cast<std::size_t>(value);
}

[[nodiscard]] PixelBounds MakePixelCenterBounds(const Rect& rectangle,
                                                std::size_t rasterTargetWidth,
                                                std::size_t rasterTargetHeight) noexcept
{
    const float left = std::ceil(rectangle.position.x - 0.5f);
    const float top = std::ceil(rectangle.position.y - 0.5f);
    const float right = std::ceil(rectangle.position.x + rectangle.size.x - 0.5f);
    const float bottom = std::ceil(rectangle.position.y + rectangle.size.y - 0.5f);

    return PixelBounds{
        .left = ClampPixelBound(left, rasterTargetWidth),
        .top = ClampPixelBound(top, rasterTargetHeight),
        .right = ClampPixelBound(right, rasterTargetWidth),
        .bottom = ClampPixelBound(bottom, rasterTargetHeight),
    };
}

[[nodiscard]] PixelBounds MakeVisualizationPixelBounds(const Rect& coverageBounds,
                                                       const Rect& clipRectangle,
                                                       std::size_t rasterTargetWidth,
                                                       std::size_t rasterTargetHeight) noexcept
{
    Rect clippedBounds = IntersectRectangles(coverageBounds, MakeRasterBounds(rasterTargetWidth, rasterTargetHeight));
    clippedBounds = IntersectRectangles(clippedBounds, clipRectangle);
    if (!HasPositiveArea(clippedBounds))
    {
        return PixelBounds{};
    }

    return MakePixelCenterBounds(clippedBounds, rasterTargetWidth, rasterTargetHeight);
}

[[nodiscard]] Rect ClipToRasterAndOperationClip(const Rect& rectangle,
                                                const Fast2DPreparedFillOperation& fillOperation,
                                                std::size_t rasterTargetWidth,
                                                std::size_t rasterTargetHeight) noexcept
{
    Rect clippedRectangle = IntersectRectangles(rectangle, MakeRasterBounds(rasterTargetWidth, rasterTargetHeight));
    if (fillOperation.hasClip)
    {
        clippedRectangle = IntersectRectangles(clippedRectangle, fillOperation.clipRectangle);
    }

    return clippedRectangle;
}

[[nodiscard]] bool IsBorderPixel(std::size_t x, std::size_t y, const PixelBounds& outerBounds,
                                 std::size_t borderThickness) noexcept
{
    const std::size_t width = outerBounds.right - outerBounds.left;
    const std::size_t height = outerBounds.bottom - outerBounds.top;
    if (borderThickness * 2U >= width || borderThickness * 2U >= height)
    {
        return true;
    }

    return x < outerBounds.left + borderThickness || x >= outerBounds.right - borderThickness ||
           y < outerBounds.top + borderThickness || y >= outerBounds.bottom - borderThickness;
}

[[nodiscard]] Rect InsetRectangle(const Rect& rectangle, float inset) noexcept
{
    return Rect{
        .position = Vec2{
            .x = rectangle.position.x + inset,
            .y = rectangle.position.y + inset,
        },
        .size = Vec2{
            .x = std::max(0.0f, rectangle.size.x - inset * 2.0f),
            .y = std::max(0.0f, rectangle.size.y - inset * 2.0f),
        },
    };
}

[[nodiscard]] float ClampCornerRadius(const Rect& rectangle, float cornerRadius) noexcept
{
    if (!std::isfinite(cornerRadius) || cornerRadius <= 0.0f)
    {
        return 0.0f;
    }

    return std::min(cornerRadius, std::min(rectangle.size.x, rectangle.size.y) * 0.5f);
}

[[nodiscard]] bool IsPixelInsideRoundedRectangle(std::size_t x, std::size_t y, const Rect& rectangle,
                                                 float cornerRadius) noexcept
{
    const float radius = ClampCornerRadius(rectangle, cornerRadius);
    if (radius <= 0.0f)
    {
        return true;
    }

    const float pixelCenterX = static_cast<float>(x) + 0.5f;
    const float pixelCenterY = static_cast<float>(y) + 0.5f;
    const float left = rectangle.position.x;
    const float top = rectangle.position.y;
    const float right = rectangle.position.x + rectangle.size.x;
    const float bottom = rectangle.position.y + rectangle.size.y;
    const float leftCenter = left + radius;
    const float rightCenter = right - radius;
    const float topCenter = top + radius;
    const float bottomCenter = bottom - radius;
    float nearestCornerCenterX = pixelCenterX;
    float nearestCornerCenterY = pixelCenterY;

    if (pixelCenterX < leftCenter)
    {
        nearestCornerCenterX = leftCenter;
    }
    else if (pixelCenterX > rightCenter)
    {
        nearestCornerCenterX = rightCenter;
    }

    if (pixelCenterY < topCenter)
    {
        nearestCornerCenterY = topCenter;
    }
    else if (pixelCenterY > bottomCenter)
    {
        nearestCornerCenterY = bottomCenter;
    }

    const float distanceX = pixelCenterX - nearestCornerCenterX;
    const float distanceY = pixelCenterY - nearestCornerCenterY;
    return distanceX * distanceX + distanceY * distanceY <= radius * radius;
}

[[nodiscard]] bool IsPixelInsideRoundedBorder(std::size_t x, std::size_t y, const Rect& outerRectangle,
                                              float outerCornerRadius, float borderThickness) noexcept
{
    if (!IsPixelInsideRoundedRectangle(x, y, outerRectangle, outerCornerRadius))
    {
        return false;
    }

    const Rect innerRectangle = InsetRectangle(outerRectangle, borderThickness);
    if (!HasPositiveArea(innerRectangle))
    {
        return true;
    }

    const float innerCornerRadius = std::max(0.0f, outerCornerRadius - borderThickness);
    return !IsPixelInsideRoundedRectangle(x, y, innerRectangle, innerCornerRadius);
}

[[nodiscard]] float SquaredDistance(Vec2 firstPoint, Vec2 secondPoint) noexcept
{
    const float distanceX = firstPoint.x - secondPoint.x;
    const float distanceY = firstPoint.y - secondPoint.y;
    return distanceX * distanceX + distanceY * distanceY;
}

[[nodiscard]] float SquaredDistanceToSegment(Vec2 point, Vec2 segmentStart, Vec2 segmentEnd) noexcept
{
    const float segmentX = segmentEnd.x - segmentStart.x;
    const float segmentY = segmentEnd.y - segmentStart.y;
    const float segmentLengthSquared = segmentX * segmentX + segmentY * segmentY;

    if (segmentLengthSquared <= 0.0f)
    {
        return SquaredDistance(point, segmentStart);
    }

    const float pointX = point.x - segmentStart.x;
    const float pointY = point.y - segmentStart.y;
    const float segmentProjection = std::clamp((pointX * segmentX + pointY * segmentY) / segmentLengthSquared,
                                               0.0f,
                                               1.0f);

    return SquaredDistance(point,
                           Vec2{
                               .x = segmentStart.x + segmentX * segmentProjection,
                               .y = segmentStart.y + segmentY * segmentProjection,
                           });
}

[[nodiscard]] bool IsPixelCoveredByLineSegment(std::size_t x,
                                               std::size_t y,
                                               Vec2 segmentStart,
                                               Vec2 segmentEnd,
                                               float strokeThickness) noexcept
{
    const float halfThickness = strokeThickness * 0.5f;
    const Vec2 pixelCenter{
        .x = static_cast<float>(x) + 0.5f,
        .y = static_cast<float>(y) + 0.5f,
    };

    return SquaredDistanceToSegment(pixelCenter, segmentStart, segmentEnd) <= halfThickness * halfThickness;
}

[[nodiscard]] Rect MakeLineCoverageBounds(const VisualizationLineCommand& lineCommand) noexcept
{
    const float halfThickness = lineCommand.strokeThickness * 0.5f;
    return MakeRectangleFromEdges(std::min(lineCommand.start.x, lineCommand.end.x) - halfThickness,
                                  std::min(lineCommand.start.y, lineCommand.end.y) - halfThickness,
                                  std::max(lineCommand.start.x, lineCommand.end.x) + halfThickness + CoverageBoundsEpsilon,
                                  std::max(lineCommand.start.y, lineCommand.end.y) + halfThickness + CoverageBoundsEpsilon);
}

[[nodiscard]] Rect MakePolylineCoverageBounds(const VisualizationPolylineCommand& polylineCommand) noexcept
{
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

[[nodiscard]] Rect MakePointMarkerCoverageBounds(const VisualizationPointMarkerCommand& pointMarkerCommand) noexcept
{
    return MakeRectangleFromEdges(pointMarkerCommand.center.x - pointMarkerCommand.radius,
                                  pointMarkerCommand.center.y - pointMarkerCommand.radius,
                                  pointMarkerCommand.center.x + pointMarkerCommand.radius + CoverageBoundsEpsilon,
                                  pointMarkerCommand.center.y + pointMarkerCommand.radius + CoverageBoundsEpsilon);
}

[[nodiscard]] Color BlendSourceOver(Color sourceColor, Color destinationColor) noexcept
{
    const float sourceAlpha = std::clamp(sourceColor.alpha, 0.0f, 1.0f);
    if (sourceAlpha >= 1.0f)
    {
        return sourceColor;
    }

    if (sourceAlpha <= 0.0f)
    {
        return destinationColor;
    }

    const float destinationAlpha = std::clamp(destinationColor.alpha, 0.0f, 1.0f);
    const float inverseSourceAlpha = 1.0f - sourceAlpha;
    const float outputAlpha = sourceAlpha + destinationAlpha * inverseSourceAlpha;
    if (outputAlpha <= 0.0f)
    {
        return Color{};
    }

    return Color{
        .red = (sourceColor.red * sourceAlpha +
                destinationColor.red * destinationAlpha * inverseSourceAlpha) /
               outputAlpha,
        .green = (sourceColor.green * sourceAlpha +
                  destinationColor.green * destinationAlpha * inverseSourceAlpha) /
                 outputAlpha,
        .blue = (sourceColor.blue * sourceAlpha +
                 destinationColor.blue * destinationAlpha * inverseSourceAlpha) /
                outputAlpha,
        .alpha = outputAlpha,
    };
}

} // namespace

Fast2DRenderer::Fast2DRenderer(std::size_t rasterTargetWidth, std::size_t rasterTargetHeight)
{
    ResizeRasterTarget(rasterTargetWidth, rasterTargetHeight);
}

void Fast2DRenderer::BeginFrame()
{
    _submittedCommands.Clear();
    ClearFrameState();
    ClearRasterTarget();
}

void Fast2DRenderer::Submit(const RenderCommandList& renderCommands)
{
    _submittedCommands.Append(renderCommands);

    for (const RenderCommand& renderCommand : renderCommands.Commands())
    {
        Consume(renderCommand);
    }
}

void Fast2DRenderer::SubmitVisualization(const VisualizationCommandList& visualizationCommands)
{
    // Copy submitted visualization commands so callers can immediately clear or
    // destroy their command list while this frame remains pending.
    _preparedVisualizationOperations.push_back(PreparedVisualizationOperation{
        .clipRectangle = visualizationCommands.ClipBounds(),
        .commands = std::vector<VisualizationCommand>(visualizationCommands.Commands().begin(),
                                                      visualizationCommands.Commands().end()),
    });
    _queuedOperations.push_back(QueuedVisualizationOperation{.index = _preparedVisualizationOperations.size() - 1U});
    _submittedVisualizationCommandCount += visualizationCommands.Size();
}

void Fast2DRenderer::EndFrame()
{
    RasterizeQueuedOperations();
    _completedFrameCommandCount = _submittedCommands.Size();
    _completedFrameVisualizationCommandCount = _submittedVisualizationCommandCount;
}

void Fast2DRenderer::ResizeRasterTarget(std::size_t width, std::size_t height)
{
    _rasterTargetWidth = width;
    _rasterTargetHeight = height;
    _rasterPixels.resize(width * height);
    ClearRasterTarget();
}

std::size_t Fast2DRenderer::SubmittedCommandCount() const noexcept
{
    return _submittedCommands.Size();
}

std::size_t Fast2DRenderer::CompletedFrameCommandCount() const noexcept
{
    return _completedFrameCommandCount;
}

std::size_t Fast2DRenderer::SubmittedVisualizationCommandCount() const noexcept
{
    return _submittedVisualizationCommandCount;
}

std::size_t Fast2DRenderer::CompletedFrameVisualizationCommandCount() const noexcept
{
    return _completedFrameVisualizationCommandCount;
}

std::size_t Fast2DRenderer::PreparedFillOperationCount() const noexcept
{
    return _preparedFillOperations.size();
}

std::size_t Fast2DRenderer::DeferredTextCommandCount() const noexcept
{
    return _deferredTextCommandCount;
}

std::size_t Fast2DRenderer::ClipUnderflowCount() const noexcept
{
    return _clipUnderflowCount;
}

std::size_t Fast2DRenderer::RasterTargetWidth() const noexcept
{
    return _rasterTargetWidth;
}

std::size_t Fast2DRenderer::RasterTargetHeight() const noexcept
{
    return _rasterTargetHeight;
}

Color Fast2DRenderer::RasterPixelAt(std::size_t x, std::size_t y) const noexcept
{
    if (x >= _rasterTargetWidth || y >= _rasterTargetHeight)
    {
        return Color{};
    }

    return _rasterPixels[GetPixelIndex(x, y, _rasterTargetWidth)];
}

std::span<const Color> Fast2DRenderer::RasterPixels() const noexcept
{
    return std::span<const Color>(_rasterPixels);
}

std::span<const RenderCommand> Fast2DRenderer::SubmittedCommands() const noexcept
{
    return _submittedCommands.Commands();
}

std::span<const Fast2DPreparedFillOperation> Fast2DRenderer::PreparedFillOperations() const noexcept
{
    return std::span<const Fast2DPreparedFillOperation>(_preparedFillOperations);
}

void Fast2DRenderer::Consume(const RenderCommand& renderCommand)
{
    if (renderCommand.type == RenderCommandType::FillRectangle)
    {
        ConsumeFillRectangle(renderCommand);
        return;
    }

    if (renderCommand.type == RenderCommandType::PushClip)
    {
        ConsumePushClip(renderCommand);
        return;
    }

    if (renderCommand.type == RenderCommandType::PopClip)
    {
        ConsumePopClip();
        return;
    }

    if (renderCommand.type == RenderCommandType::DrawText)
    {
        ConsumeDrawText();
    }
}

void Fast2DRenderer::ConsumeFillRectangle(const RenderCommand& renderCommand)
{
    _preparedFillOperations.push_back(Fast2DPreparedFillOperation{
        .rectangle = renderCommand.rectangle,
        .fillColor = renderCommand.fillColor,
        .cornerRadius = renderCommand.cornerRadius,
        .borderColor = renderCommand.borderColor,
        .borderThickness = renderCommand.borderThickness,
        .hasClip = !_clipStack.empty(),
        .clipRectangle = _clipStack.empty() ? Rect{} : _clipStack.back(),
    });
    _queuedOperations.push_back(QueuedFillOperation{.index = _preparedFillOperations.size() - 1U});
}

void Fast2DRenderer::ConsumePushClip(const RenderCommand& renderCommand)
{
    const Rect clipRectangle = _clipStack.empty()
                                   ? renderCommand.rectangle
                                   : IntersectRectangles(_clipStack.back(), renderCommand.rectangle);
    _clipStack.push_back(clipRectangle);
}

void Fast2DRenderer::ConsumePopClip()
{
    if (_clipStack.empty())
    {
        ++_clipUnderflowCount;
        return;
    }

    _clipStack.pop_back();
}

void Fast2DRenderer::ConsumeDrawText()
{
    // Fast2D does not own text rasterization yet; keep text deferred inside this backend.
    ++_deferredTextCommandCount;
}

void Fast2DRenderer::RasterizeQueuedOperations()
{
    // UI fills and visualization submissions share this backend-local queue so
    // mixed Submit()/SubmitVisualization() calls draw in frame submission order.
    for (const QueuedOperation& queuedOperation : _queuedOperations)
    {
        if (const QueuedFillOperation* fillOperation = std::get_if<QueuedFillOperation>(&queuedOperation))
        {
            RasterizeFillRectangle(_preparedFillOperations[fillOperation->index]);
            continue;
        }

        if (const QueuedVisualizationOperation* visualizationOperation =
                std::get_if<QueuedVisualizationOperation>(&queuedOperation))
        {
            RasterizeVisualizationOperation(_preparedVisualizationOperations[visualizationOperation->index]);
        }
    }
}

void Fast2DRenderer::RasterizeFillRectangle(const Fast2DPreparedFillOperation& fillOperation)
{
    if (_rasterTargetWidth == 0U || _rasterTargetHeight == 0U)
    {
        return;
    }

    if (ClampCornerRadius(fillOperation.rectangle, fillOperation.cornerRadius) <= 0.0f)
    {
        RasterizeFillInterior(fillOperation);
    }
    else
    {
        RasterizeRoundedFillInterior(fillOperation);
    }

    RasterizeBorder(fillOperation);
}

void Fast2DRenderer::RasterizeFillInterior(const Fast2DPreparedFillOperation& fillOperation)
{
    const Rect rasterRectangle = ClipToRasterAndOperationClip(fillOperation.rectangle,
                                                              fillOperation,
                                                              _rasterTargetWidth,
                                                              _rasterTargetHeight);

    if (!HasPositiveArea(rasterRectangle))
    {
        return;
    }

    const PixelBounds bounds = MakePixelBounds(rasterRectangle);

    for (std::size_t y = bounds.top; y < bounds.bottom; ++y)
    {
        for (std::size_t x = bounds.left; x < bounds.right; ++x)
        {
            Color& destinationPixel = _rasterPixels[GetPixelIndex(x, y, _rasterTargetWidth)];
            destinationPixel = BlendSourceOver(fillOperation.fillColor, destinationPixel);
        }
    }
}

void Fast2DRenderer::RasterizeRoundedFillInterior(const Fast2DPreparedFillOperation& fillOperation)
{
    const Rect rasterRectangle = ClipToRasterAndOperationClip(fillOperation.rectangle,
                                                              fillOperation,
                                                              _rasterTargetWidth,
                                                              _rasterTargetHeight);

    if (!HasPositiveArea(rasterRectangle))
    {
        return;
    }

    const PixelBounds bounds = MakePixelBounds(rasterRectangle);

    for (std::size_t y = bounds.top; y < bounds.bottom; ++y)
    {
        for (std::size_t x = bounds.left; x < bounds.right; ++x)
        {
            if (!IsPixelInsideRoundedRectangle(x, y, fillOperation.rectangle, fillOperation.cornerRadius))
            {
                continue;
            }

            Color& destinationPixel = _rasterPixels[GetPixelIndex(x, y, _rasterTargetWidth)];
            destinationPixel = BlendSourceOver(fillOperation.fillColor, destinationPixel);
        }
    }
}

void Fast2DRenderer::RasterizeBorder(const Fast2DPreparedFillOperation& fillOperation)
{
    if (!std::isfinite(fillOperation.borderThickness) || fillOperation.borderThickness <= 0.0f)
    {
        return;
    }

    if (ClampCornerRadius(fillOperation.rectangle, fillOperation.cornerRadius) <= 0.0f)
    {
        RasterizeHardEdgedBorder(fillOperation);
        return;
    }

    RasterizeRoundedBorder(fillOperation);
}

void Fast2DRenderer::RasterizeHardEdgedBorder(const Fast2DPreparedFillOperation& fillOperation)
{
    const Rect outerRectangle = ClipToRasterAndOperationClip(fillOperation.rectangle,
                                                             fillOperation,
                                                             _rasterTargetWidth,
                                                             _rasterTargetHeight);
    if (!HasPositiveArea(outerRectangle))
    {
        return;
    }

    const Rect unclippedOuterRectangle =
        IntersectRectangles(fillOperation.rectangle, MakeRasterBounds(_rasterTargetWidth, _rasterTargetHeight));
    if (!HasPositiveArea(unclippedOuterRectangle))
    {
        return;
    }

    const PixelBounds clippedBounds = MakePixelBounds(outerRectangle);
    const PixelBounds outerBounds = MakePixelBounds(unclippedOuterRectangle);
    const auto borderThickness = static_cast<std::size_t>(std::ceil(fillOperation.borderThickness));

    for (std::size_t y = clippedBounds.top; y < clippedBounds.bottom; ++y)
    {
        for (std::size_t x = clippedBounds.left; x < clippedBounds.right; ++x)
        {
            if (!IsBorderPixel(x, y, outerBounds, borderThickness))
            {
                continue;
            }

            Color& destinationPixel = _rasterPixels[GetPixelIndex(x, y, _rasterTargetWidth)];
            destinationPixel = BlendSourceOver(fillOperation.borderColor, destinationPixel);
        }
    }
}

void Fast2DRenderer::RasterizeRoundedBorder(const Fast2DPreparedFillOperation& fillOperation)
{
    const Rect outerRectangle = ClipToRasterAndOperationClip(fillOperation.rectangle,
                                                             fillOperation,
                                                             _rasterTargetWidth,
                                                             _rasterTargetHeight);
    if (!HasPositiveArea(outerRectangle))
    {
        return;
    }

    const PixelBounds clippedBounds = MakePixelBounds(outerRectangle);
    const float borderThickness = static_cast<float>(std::ceil(fillOperation.borderThickness));

    for (std::size_t y = clippedBounds.top; y < clippedBounds.bottom; ++y)
    {
        for (std::size_t x = clippedBounds.left; x < clippedBounds.right; ++x)
        {
            if (!IsPixelInsideRoundedBorder(x,
                                            y,
                                            fillOperation.rectangle,
                                            fillOperation.cornerRadius,
                                            borderThickness))
            {
                continue;
            }

            Color& destinationPixel = _rasterPixels[GetPixelIndex(x, y, _rasterTargetWidth)];
            destinationPixel = BlendSourceOver(fillOperation.borderColor, destinationPixel);
        }
    }
}

void Fast2DRenderer::RasterizeVisualizationOperation(const PreparedVisualizationOperation& visualizationOperation)
{
    if (_rasterTargetWidth == 0U || _rasterTargetHeight == 0U)
    {
        return;
    }

    if (!HasPositiveArea(visualizationOperation.clipRectangle))
    {
        return;
    }

    for (const VisualizationCommand& visualizationCommand : visualizationOperation.commands)
    {
        RasterizeVisualizationCommand(visualizationCommand, visualizationOperation.clipRectangle);
    }
}

void Fast2DRenderer::RasterizeVisualizationCommand(const VisualizationCommand& visualizationCommand,
                                                   const Rect& clipRectangle)
{
    if (const VisualizationLineCommand* lineCommand = std::get_if<VisualizationLineCommand>(&visualizationCommand))
    {
        RasterizeVisualizationLine(*lineCommand, clipRectangle);
        return;
    }

    if (const VisualizationPolylineCommand* polylineCommand =
            std::get_if<VisualizationPolylineCommand>(&visualizationCommand))
    {
        RasterizeVisualizationPolyline(*polylineCommand, clipRectangle);
        return;
    }

    if (const VisualizationPointMarkerCommand* pointMarkerCommand =
            std::get_if<VisualizationPointMarkerCommand>(&visualizationCommand))
    {
        RasterizeVisualizationPointMarker(*pointMarkerCommand, clipRectangle);
    }
}

void Fast2DRenderer::RasterizeVisualizationLine(const VisualizationLineCommand& lineCommand, const Rect& clipRectangle)
{
    const PixelBounds bounds =
        MakeVisualizationPixelBounds(MakeLineCoverageBounds(lineCommand),
                                     clipRectangle,
                                     _rasterTargetWidth,
                                     _rasterTargetHeight);

    for (std::size_t y = bounds.top; y < bounds.bottom; ++y)
    {
        for (std::size_t x = bounds.left; x < bounds.right; ++x)
        {
            if (!IsPixelCoveredByLineSegment(x, y, lineCommand.start, lineCommand.end, lineCommand.strokeThickness))
            {
                continue;
            }

            Color& destinationPixel = _rasterPixels[GetPixelIndex(x, y, _rasterTargetWidth)];
            destinationPixel = BlendSourceOver(lineCommand.strokeColor, destinationPixel);
        }
    }
}

void Fast2DRenderer::RasterizeVisualizationPolyline(const VisualizationPolylineCommand& polylineCommand,
                                                    const Rect& clipRectangle)
{
    if (polylineCommand.points.size() < 2U)
    {
        return;
    }

    const PixelBounds bounds =
        MakeVisualizationPixelBounds(MakePolylineCoverageBounds(polylineCommand),
                                     clipRectangle,
                                     _rasterTargetWidth,
                                     _rasterTargetHeight);

    for (std::size_t y = bounds.top; y < bounds.bottom; ++y)
    {
        for (std::size_t x = bounds.left; x < bounds.right; ++x)
        {
            bool isCovered = false;
            for (std::size_t pointIndex = 1U; pointIndex < polylineCommand.points.size(); ++pointIndex)
            {
                if (IsPixelCoveredByLineSegment(x,
                                                y,
                                                polylineCommand.points[pointIndex - 1U],
                                                polylineCommand.points[pointIndex],
                                                polylineCommand.strokeThickness))
                {
                    isCovered = true;
                    break;
                }
            }

            if (!isCovered)
            {
                continue;
            }

            Color& destinationPixel = _rasterPixels[GetPixelIndex(x, y, _rasterTargetWidth)];
            destinationPixel = BlendSourceOver(polylineCommand.strokeColor, destinationPixel);
        }
    }
}

void Fast2DRenderer::RasterizeVisualizationPointMarker(const VisualizationPointMarkerCommand& pointMarkerCommand,
                                                       const Rect& clipRectangle)
{
    const PixelBounds bounds =
        MakeVisualizationPixelBounds(MakePointMarkerCoverageBounds(pointMarkerCommand),
                                     clipRectangle,
                                     _rasterTargetWidth,
                                     _rasterTargetHeight);
    const float radiusSquared = pointMarkerCommand.radius * pointMarkerCommand.radius;

    for (std::size_t y = bounds.top; y < bounds.bottom; ++y)
    {
        for (std::size_t x = bounds.left; x < bounds.right; ++x)
        {
            const Vec2 pixelCenter{
                .x = static_cast<float>(x) + 0.5f,
                .y = static_cast<float>(y) + 0.5f,
            };

            if (SquaredDistance(pixelCenter, pointMarkerCommand.center) > radiusSquared)
            {
                continue;
            }

            Color& destinationPixel = _rasterPixels[GetPixelIndex(x, y, _rasterTargetWidth)];
            destinationPixel = BlendSourceOver(pointMarkerCommand.fillColor, destinationPixel);
        }
    }
}

void Fast2DRenderer::ClearRasterTarget()
{
    std::fill(_rasterPixels.begin(), _rasterPixels.end(), Color{});
}

void Fast2DRenderer::ClearFrameState()
{
    _preparedFillOperations.clear();
    _preparedVisualizationOperations.clear();
    _queuedOperations.clear();
    _clipStack.clear();
    _submittedVisualizationCommandCount = 0;
    _deferredTextCommandCount = 0;
    _clipUnderflowCount = 0;
}

} // namespace greenfield

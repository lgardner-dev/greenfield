#include "engine/render/fast2d/Fast2DRenderer.h"

#include <algorithm>
#include <cmath>

namespace greenfield
{
namespace
{

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

void Fast2DRenderer::EndFrame()
{
    RasterizePreparedFillOperations();
    _completedFrameCommandCount = _submittedCommands.Size();
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
}

void Fast2DRenderer::ConsumePushClip(const RenderCommand& renderCommand)
{
    _clipStack.push_back(renderCommand.rectangle);
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

void Fast2DRenderer::RasterizePreparedFillOperations()
{
    for (const Fast2DPreparedFillOperation& fillOperation : _preparedFillOperations)
    {
        RasterizeFillRectangle(fillOperation);
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

    RasterizeHardEdgedBorder(fillOperation);
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

void Fast2DRenderer::RasterizeHardEdgedBorder(const Fast2DPreparedFillOperation& fillOperation)
{
    if (!std::isfinite(fillOperation.borderThickness) || fillOperation.borderThickness <= 0.0f)
    {
        return;
    }

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

void Fast2DRenderer::ClearRasterTarget()
{
    std::fill(_rasterPixels.begin(), _rasterPixels.end(), Color{});
}

void Fast2DRenderer::ClearFrameState()
{
    _preparedFillOperations.clear();
    _clipStack.clear();
    _deferredTextCommandCount = 0;
    _clipUnderflowCount = 0;
}

} // namespace greenfield

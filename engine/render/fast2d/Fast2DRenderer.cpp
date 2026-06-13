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
    // M3 Fast2D preserves shape styling for later backend work, but rasterizes only the plain fill for now.
    if (_rasterTargetWidth == 0U || _rasterTargetHeight == 0U)
    {
        return;
    }

    Rect rasterRectangle = IntersectRectangles(fillOperation.rectangle,
                                               MakeRasterBounds(_rasterTargetWidth, _rasterTargetHeight));
    if (fillOperation.hasClip)
    {
        rasterRectangle = IntersectRectangles(rasterRectangle, fillOperation.clipRectangle);
    }

    if (!HasPositiveArea(rasterRectangle))
    {
        return;
    }

    const auto left = static_cast<std::size_t>(std::ceil(rasterRectangle.position.x));
    const auto top = static_cast<std::size_t>(std::ceil(rasterRectangle.position.y));
    const auto right =
        static_cast<std::size_t>(std::ceil(rasterRectangle.position.x + rasterRectangle.size.x));
    const auto bottom =
        static_cast<std::size_t>(std::ceil(rasterRectangle.position.y + rasterRectangle.size.y));

    for (std::size_t y = top; y < bottom; ++y)
    {
        for (std::size_t x = left; x < right; ++x)
        {
            _rasterPixels[GetPixelIndex(x, y, _rasterTargetWidth)] = fillOperation.fillColor;
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

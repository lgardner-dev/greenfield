#include "engine/render/fast2d/Fast2DRenderer.h"

namespace greenfield
{

void Fast2DRenderer::BeginFrame()
{
    _submittedCommands.Clear();
    ClearFrameState();
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
    _completedFrameCommandCount = _submittedCommands.Size();
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

void Fast2DRenderer::ClearFrameState()
{
    _preparedFillOperations.clear();
    _clipStack.clear();
    _deferredTextCommandCount = 0;
    _clipUnderflowCount = 0;
}

} // namespace greenfield

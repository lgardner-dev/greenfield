#include "engine/render/fast2d/Fast2DRenderer.h"

namespace greenfield
{

void Fast2DRenderer::BeginFrame()
{
    _submittedCommands.Clear();
}

void Fast2DRenderer::Submit(const RenderCommandList& renderCommands)
{
    _submittedCommands.Append(renderCommands);
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

std::span<const RenderCommand> Fast2DRenderer::SubmittedCommands() const noexcept
{
    return _submittedCommands.Commands();
}

} // namespace greenfield

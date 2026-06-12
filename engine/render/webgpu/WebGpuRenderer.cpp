#include "engine/render/webgpu/WebGpuRenderer.h"

#include "engine/render/webgpu/WebGpuContext.h"

namespace greenfield
{

WebGpuRenderer::WebGpuRenderer(WebGpuContext& context)
    : _context(&context)
{
}

void WebGpuRenderer::BeginFrame()
{
    _submittedCommands.Clear();
}

void WebGpuRenderer::Submit(const RenderCommandList& renderCommands)
{
    _submittedCommands.Append(renderCommands);
}

void WebGpuRenderer::EndFrame()
{
    if (_context == nullptr || !_context->IsInitialized())
    {
        return;
    }
}

std::size_t WebGpuRenderer::SubmittedCommandCount() const noexcept
{
    return _submittedCommands.Size();
}

} // namespace greenfield

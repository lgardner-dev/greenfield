#include "engine/render/webgpu/WebGpuRenderer.h"

#include <stdexcept>

#include "engine/render/webgpu/WebGpuContext.h"

namespace greenfield
{
namespace
{

bool IsUsableSurfaceTextureStatus(wgpu::SurfaceGetCurrentTextureStatus status)
{
    return status == wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal ||
           status == wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal;
}

wgpu::Color GetClearColor()
{
    return wgpu::Color{
        .r = 0.08,
        .g = 0.09,
        .b = 0.10,
        .a = 1.0,
    };
}

} // namespace

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

    _context->ReconfigureIfNeeded();

    wgpu::SurfaceTexture surfaceTexture{};
    _context->GetSurface().GetCurrentTexture(&surfaceTexture);
    if (!IsUsableSurfaceTextureStatus(surfaceTexture.status))
    {
        _context->ReconfigureIfNeeded();
        return;
    }

    if (surfaceTexture.texture == nullptr)
    {
        throw std::runtime_error("WebGPU surface returned a null frame texture.");
    }

    wgpu::TextureView textureView = surfaceTexture.texture.CreateView();
    wgpu::RenderPassColorAttachment colorAttachment{};
    colorAttachment.view = textureView;
    colorAttachment.loadOp = wgpu::LoadOp::Clear;
    colorAttachment.storeOp = wgpu::StoreOp::Store;
    colorAttachment.clearValue = GetClearColor();

    const wgpu::RenderPassDescriptor renderPassDescriptor{
        .colorAttachmentCount = 1,
        .colorAttachments = &colorAttachment,
    };

    wgpu::CommandEncoder commandEncoder = _context->GetDevice().CreateCommandEncoder();
    wgpu::RenderPassEncoder renderPass = commandEncoder.BeginRenderPass(&renderPassDescriptor);
    renderPass.End();

    wgpu::CommandBuffer commandBuffer = commandEncoder.Finish();
    _context->GetQueue().Submit(1, &commandBuffer);
    _context->GetSurface().Present();
}

std::size_t WebGpuRenderer::SubmittedCommandCount() const noexcept
{
    return _submittedCommands.Size();
}

} // namespace greenfield

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <dawn/webgpu_cpp.h>

#include "engine/render/IRenderer.h"

namespace greenfield
{

class WebGpuContext;

class WebGpuRenderer final : public IRenderer
{
public:
    explicit WebGpuRenderer(WebGpuContext& context);

    void BeginFrame() override;
    void Submit(const RenderCommandList& renderCommands) override;
    void EndFrame() override;

    [[nodiscard]] std::size_t SubmittedCommandCount() const noexcept;

private:
    struct RectangleVertex
    {
        float position[2]{};
        float color[4]{};
    };

    void EnsureRectanglePipeline();
    void BuildRectangleVertices();
    void EnsureVertexBuffer(std::size_t requiredSize);
    void DrawRectangles(wgpu::RenderPassEncoder& renderPass);

    WebGpuContext* _context{nullptr};
    RenderCommandList _submittedCommands;
    wgpu::RenderPipeline _rectanglePipeline;
    wgpu::Buffer _rectangleVertexBuffer;
    wgpu::TextureFormat _rectanglePipelineFormat{wgpu::TextureFormat::Undefined};
    std::vector<RectangleVertex> _rectangleVertices;
    std::uint64_t _rectangleVertexBufferSize{0};
};

} // namespace greenfield

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
        float pixelPosition[2]{};
        float rectanglePosition[2]{};
        float rectangleSize[2]{};
        float fillColor[4]{};
        float borderColor[4]{};
        float cornerRadius{0.0f};
        float borderThickness{0.0f};
    };

    void EnsureRectanglePipeline();
    [[nodiscard]] RectangleVertex MakeRectangleVertex(float clipPositionX, float clipPositionY, float pixelPositionX,
                                                      float pixelPositionY,
                                                      const RenderCommand& renderCommand) const noexcept;
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

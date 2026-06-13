#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <dawn/webgpu_cpp.h>

#include "engine/render/IRenderer.h"

namespace greenfield
{

class WebGpuContext;

class WebGpuRenderer final : public IRenderer
{
public:
    explicit WebGpuRenderer(WebGpuContext& context, std::string defaultFontPath = {});

    void BeginFrame() override;
    void Submit(const RenderCommandList& renderCommands) override;
    void EndFrame() override;
    void SetDefaultFontPath(std::string fontPath);

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

    struct TextVertex
    {
        float position[2]{};
        float textureCoordinate[2]{};
        float color[4]{};
    };

    struct GlyphInfo
    {
        float textureLeft{0.0f};
        float textureTop{0.0f};
        float textureRight{0.0f};
        float textureBottom{0.0f};
        float width{0.0f};
        float height{0.0f};
        float bearingX{0.0f};
        float bearingY{0.0f};
        float advance{0.0f};
    };

    struct FontAtlas
    {
        wgpu::Texture texture;
        wgpu::TextureView textureView;
        wgpu::Sampler sampler;
        wgpu::BindGroup bindGroup;
        std::unordered_map<char, GlyphInfo> glyphs;
        float lineHeight{0.0f};
    };

    struct TextBatch
    {
        wgpu::BindGroup bindGroup;
        std::uint32_t firstVertex{0};
        std::uint32_t vertexCount{0};
    };

    struct ScissorRectangle
    {
        std::uint32_t x{0};
        std::uint32_t y{0};
        std::uint32_t width{0};
        std::uint32_t height{0};
    };

    struct DrawBatch
    {
        RenderCommandType type{RenderCommandType::FillRectangle};
        bool hasClip{false};
        Rect clipRectangle{};
        wgpu::BindGroup bindGroup;
        std::uint32_t firstVertex{0};
        std::uint32_t vertexCount{0};
    };

    void EnsureRectanglePipeline();
    void EnsureTextPipeline();
    [[nodiscard]] FontAtlas* GetOrCreateFontAtlas(float fontSize);
    [[nodiscard]] RectangleVertex MakeRectangleVertex(float clipPositionX, float clipPositionY, float pixelPositionX,
                                                      float pixelPositionY,
                                                      const RenderCommand& renderCommand) const noexcept;
    [[nodiscard]] TextVertex MakeTextVertex(float pixelPositionX, float pixelPositionY, float textureCoordinateX,
                                            float textureCoordinateY, const Color& color) const noexcept;
    void BuildRectangleVertices();
    void AppendTextVertices(const RenderCommand& renderCommand, const FontAtlas& fontAtlas);
    void EnsureVertexBuffer(std::size_t requiredSize);
    void EnsureTextVertexBuffer(std::size_t requiredSize);
    void DrawRectangles(wgpu::RenderPassEncoder& renderPass);
    void DrawText(wgpu::RenderPassEncoder& renderPass);
    void DrawRenderCommands(wgpu::RenderPassEncoder& renderPass);
    void BuildRenderBatches();
    void AppendRectangleVertices(const RenderCommand& renderCommand);
    void WritePreparedVertexBuffers();
    void DrawPreparedBatch(wgpu::RenderPassEncoder& renderPass, const DrawBatch& drawBatch);
    void DrawRectangleCommand(wgpu::RenderPassEncoder& renderPass, const RenderCommand& renderCommand);
    void DrawTextCommand(wgpu::RenderPassEncoder& renderPass, const RenderCommand& renderCommand);
    void ApplyFullFrameScissor(wgpu::RenderPassEncoder& renderPass) const;
    void ApplyClipScissor(wgpu::RenderPassEncoder& renderPass, const Rect& clipRectangle) const;
    [[nodiscard]] ScissorRectangle MakeScissorRectangle(const Rect& clipRectangle) const noexcept;

    WebGpuContext* _context{nullptr};
    RenderCommandList _submittedCommands;
    std::string _defaultFontPath;
    wgpu::RenderPipeline _rectanglePipeline;
    wgpu::RenderPipeline _textPipeline;
    wgpu::BindGroupLayout _textBindGroupLayout;
    wgpu::Buffer _rectangleVertexBuffer;
    wgpu::Buffer _textVertexBuffer;
    wgpu::TextureFormat _rectanglePipelineFormat{wgpu::TextureFormat::Undefined};
    wgpu::TextureFormat _textPipelineFormat{wgpu::TextureFormat::Undefined};
    std::vector<RectangleVertex> _rectangleVertices;
    std::vector<TextVertex> _textVertices;
    std::vector<TextBatch> _textBatches;
    std::vector<DrawBatch> _drawBatches;
    std::unordered_map<int, FontAtlas> _fontAtlases;
    std::uint64_t _rectangleVertexBufferSize{0};
    std::uint64_t _textVertexBufferSize{0};
};

} // namespace greenfield

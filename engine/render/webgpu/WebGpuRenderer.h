#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include <dawn/webgpu_cpp.h>

#include "engine/render/IRenderer.h"
#include "engine/render/webgpu/WebGpuVisualizationSupport.h"
#include "engine/visualization/VisualizationCommandList.h"

namespace greenfield
{

class WebGpuContext;

struct WebGpuFrameDiagnostics
{
    bool enabled{false};
    bool surfaceTextureAcquired{false};
    wgpu::SurfaceGetCurrentTextureStatus surfaceTextureStatus{wgpu::SurfaceGetCurrentTextureStatus::Error};
    wgpu::Status presentStatus{wgpu::Status::Error};
    double totalMilliseconds{0.0};
    double reconfigureMilliseconds{0.0};
    double surfaceAcquireMilliseconds{0.0};
    double textureViewCreationMilliseconds{0.0};
    double renderCommandPreparationMilliseconds{0.0};
    double rectanglePipelineCreationMilliseconds{0.0};
    double textPipelineCreationMilliseconds{0.0};
    double visualizationPipelineCreationMilliseconds{0.0};
    double fontAtlasCreationMilliseconds{0.0};
    double rectangleVertexBufferGrowthMilliseconds{0.0};
    double textVertexBufferGrowthMilliseconds{0.0};
    double visualizationVertexBufferGrowthMilliseconds{0.0};
    double visualizationPointBufferGrowthMilliseconds{0.0};
    double vertexBufferWriteMilliseconds{0.0};
    double visualizationPointBufferUploadMilliseconds{0.0};
    double commandEncodingMilliseconds{0.0};
    double queueSubmissionMilliseconds{0.0};
    double surfacePresentMilliseconds{0.0};
};

class WebGpuRenderer final : public IRenderer
{
public:
    explicit WebGpuRenderer(WebGpuContext& context, std::string defaultFontPath = {});

    void BeginFrame() override;
    void Submit(const RenderCommandList& renderCommands) override;
    void SubmitVisualization(const VisualizationCommandList& visualizationCommands);
    void EndFrame() override;
    void SetDefaultFontPath(std::string fontPath);

    [[nodiscard]] std::size_t SubmittedCommandCount() const noexcept;
    [[nodiscard]] std::size_t CompletedFrameCommandCount() const noexcept;
    [[nodiscard]] std::size_t SubmittedVisualizationCommandCount() const noexcept;
    [[nodiscard]] std::size_t CompletedFrameVisualizationCommandCount() const noexcept;
    void SetFrameDiagnosticsEnabled(bool enabled) noexcept;
    [[nodiscard]] const WebGpuFrameDiagnostics& GetLastFrameDiagnostics() const noexcept;

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

    struct VisualizationVertex
    {
        float position[2]{};
        float color[4]{};
        float firstPoint[2]{};
        float secondPoint[2]{};
        float thicknessOrRadius{1.0f};
        std::uint32_t primitiveKind{0};
        std::uint32_t pointOffset{0};
        std::uint32_t pointCount{0};
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

    struct VisualizationDrawBatch
    {
        WebGpuScissorRectangle scissorRectangle{};
        std::uint32_t firstVertex{0};
        std::uint32_t vertexCount{0};
    };

    struct QueuedDrawBatch
    {
        std::size_t index{0};
    };

    struct QueuedVisualizationDrawBatch
    {
        std::size_t index{0};
    };

    using QueuedDrawOperation = std::variant<QueuedDrawBatch, QueuedVisualizationDrawBatch>;

    void EnsureRectanglePipeline();
    void EnsureTextPipeline();
    void EnsureVisualizationPipeline();
    [[nodiscard]] FontAtlas* GetOrCreateFontAtlas(float fontSize);
    [[nodiscard]] RectangleVertex MakeRectangleVertex(float clipPositionX, float clipPositionY, float pixelPositionX,
                                                      float pixelPositionY,
                                                      const RenderCommand& renderCommand) const noexcept;
    [[nodiscard]] TextVertex MakeTextVertex(float pixelPositionX, float pixelPositionY, float textureCoordinateX,
                                            float textureCoordinateY, const Color& color) const noexcept;
    [[nodiscard]] VisualizationVertex MakeVisualizationVertex(
        float pixelPositionX,
        float pixelPositionY,
        const WebGpuPreparedVisualizationPrimitive& primitive,
        std::uint32_t pointOffset) const noexcept;
    void AppendTextVertices(const RenderCommand& renderCommand, const FontAtlas& fontAtlas);
    void EnsureVertexBuffer(std::size_t requiredSize);
    void EnsureTextVertexBuffer(std::size_t requiredSize);
    void EnsureVisualizationVertexBuffer(std::size_t requiredSize);
    void EnsureVisualizationPointBuffer(std::size_t requiredSize);
    void EnsureVisualizationBindGroup();
    void DrawRenderCommands(wgpu::RenderPassEncoder& renderPass);
    void BuildRenderBatches();
    void BuildRenderCommandBatches(std::span<const RenderCommand> renderCommands, std::vector<Rect>& clipRectangles);
    void BuildVisualizationBatches(const WebGpuQueuedVisualizationSubmission& visualizationSubmission);
    void AppendRectangleVertices(const RenderCommand& renderCommand);
    void AppendVisualizationVertices(
        const WebGpuPreparedVisualizationPrimitive& primitive,
        const WebGpuScissorRectangle& scissorRectangle,
        std::uint32_t pointOffset);
    void WritePreparedVertexBuffers();
    void WriteVisualizationPointBuffer();
    void DrawQueuedOperation(wgpu::RenderPassEncoder& renderPass, const QueuedDrawOperation& queuedDrawOperation);
    void DrawPreparedBatch(wgpu::RenderPassEncoder& renderPass, const DrawBatch& drawBatch);
    void DrawPreparedVisualizationBatch(wgpu::RenderPassEncoder& renderPass,
                                        const VisualizationDrawBatch& visualizationDrawBatch);
    void ApplyFullFrameScissor(wgpu::RenderPassEncoder& renderPass) const;
    void ApplyClipScissor(wgpu::RenderPassEncoder& renderPass, const Rect& clipRectangle) const;
    void ApplyVisualizationScissor(wgpu::RenderPassEncoder& renderPass,
                                   const WebGpuScissorRectangle& scissorRectangle) const;
    [[nodiscard]] ScissorRectangle MakeScissorRectangle(const Rect& clipRectangle) const noexcept;

    WebGpuContext* _context{nullptr};
    WebGpuSubmissionQueue _submissionQueue;
    std::string _defaultFontPath;
    wgpu::RenderPipeline _rectanglePipeline;
    wgpu::RenderPipeline _textPipeline;
    wgpu::RenderPipeline _visualizationPipeline;
    wgpu::BindGroupLayout _textBindGroupLayout;
    wgpu::BindGroupLayout _visualizationBindGroupLayout;
    wgpu::BindGroup _visualizationBindGroup;
    wgpu::Buffer _rectangleVertexBuffer;
    wgpu::Buffer _textVertexBuffer;
    wgpu::Buffer _visualizationVertexBuffer;
    wgpu::Buffer _visualizationPointBuffer;
    wgpu::TextureFormat _rectanglePipelineFormat{wgpu::TextureFormat::Undefined};
    wgpu::TextureFormat _textPipelineFormat{wgpu::TextureFormat::Undefined};
    wgpu::TextureFormat _visualizationPipelineFormat{wgpu::TextureFormat::Undefined};
    std::vector<RectangleVertex> _rectangleVertices;
    std::vector<TextVertex> _textVertices;
    std::vector<VisualizationVertex> _visualizationVertices;
    std::vector<Vec2> _visualizationPoints;
    std::vector<DrawBatch> _drawBatches;
    std::vector<VisualizationDrawBatch> _visualizationDrawBatches;
    std::vector<QueuedDrawOperation> _queuedDrawOperations;
    std::unordered_map<int, FontAtlas> _fontAtlases;
    std::uint64_t _rectangleVertexBufferSize{0};
    std::uint64_t _textVertexBufferSize{0};
    std::uint64_t _visualizationVertexBufferSize{0};
    std::uint64_t _visualizationPointBufferSize{0};
    std::size_t _completedFrameCommandCount{0};
    std::size_t _completedFrameVisualizationCommandCount{0};
    bool _frameDiagnosticsEnabled{false};
    WebGpuFrameDiagnostics _lastFrameDiagnostics{};
};

} // namespace greenfield

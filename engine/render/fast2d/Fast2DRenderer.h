#pragma once

#include <cstddef>
#include <span>
#include <variant>
#include <vector>

#include "engine/render/IRenderer.h"
#include "engine/visualization/VisualizationCommand.h"
#include "engine/visualization/VisualizationCommandList.h"

namespace greenfield
{

struct Fast2DPreparedFillOperation
{
    Rect rectangle{};
    Color fillColor{};
    float cornerRadius{0.0f};
    Color borderColor{};
    float borderThickness{0.0f};
    bool hasClip{false};
    Rect clipRectangle{};
};

class Fast2DRenderer final : public IRenderer
{
public:
    Fast2DRenderer() = default;
    Fast2DRenderer(std::size_t rasterTargetWidth, std::size_t rasterTargetHeight);

    void BeginFrame() override;
    void Submit(const RenderCommandList& renderCommands) override;
    void SubmitVisualization(const VisualizationCommandList& visualizationCommands);
    void EndFrame() override;
    void ResizeRasterTarget(std::size_t width, std::size_t height);

    [[nodiscard]] std::size_t SubmittedCommandCount() const noexcept;
    [[nodiscard]] std::size_t CompletedFrameCommandCount() const noexcept;
    [[nodiscard]] std::size_t SubmittedVisualizationCommandCount() const noexcept;
    [[nodiscard]] std::size_t CompletedFrameVisualizationCommandCount() const noexcept;
    [[nodiscard]] std::size_t PreparedFillOperationCount() const noexcept;
    [[nodiscard]] std::size_t DeferredTextCommandCount() const noexcept;
    [[nodiscard]] std::size_t ClipUnderflowCount() const noexcept;
    [[nodiscard]] std::size_t RasterTargetWidth() const noexcept;
    [[nodiscard]] std::size_t RasterTargetHeight() const noexcept;
    [[nodiscard]] Color RasterPixelAt(std::size_t x, std::size_t y) const noexcept;
    [[nodiscard]] std::span<const Color> RasterPixels() const noexcept;
    [[nodiscard]] std::span<const RenderCommand> SubmittedCommands() const noexcept;
    [[nodiscard]] std::span<const Fast2DPreparedFillOperation> PreparedFillOperations() const noexcept;

private:
    struct QueuedFillOperation
    {
        std::size_t index{0};
    };

    struct QueuedVisualizationOperation
    {
        std::size_t index{0};
    };

    struct PreparedVisualizationOperation
    {
        Rect clipRectangle{};
        std::vector<VisualizationCommand> commands;
    };

    using QueuedOperation = std::variant<QueuedFillOperation, QueuedVisualizationOperation>;

    void Consume(const RenderCommand& renderCommand);
    void ConsumeFillRectangle(const RenderCommand& renderCommand);
    void ConsumePushClip(const RenderCommand& renderCommand);
    void ConsumePopClip();
    void ConsumeDrawText();
    void RasterizeQueuedOperations();
    void RasterizeFillRectangle(const Fast2DPreparedFillOperation& fillOperation);
    void RasterizeFillInterior(const Fast2DPreparedFillOperation& fillOperation);
    void RasterizeRoundedFillInterior(const Fast2DPreparedFillOperation& fillOperation);
    void RasterizeBorder(const Fast2DPreparedFillOperation& fillOperation);
    void RasterizeHardEdgedBorder(const Fast2DPreparedFillOperation& fillOperation);
    void RasterizeRoundedBorder(const Fast2DPreparedFillOperation& fillOperation);
    void RasterizeVisualizationOperation(const PreparedVisualizationOperation& visualizationOperation);
    void RasterizeVisualizationCommand(const VisualizationCommand& visualizationCommand, const Rect& clipRectangle);
    void RasterizeVisualizationLine(const VisualizationLineCommand& lineCommand, const Rect& clipRectangle);
    void RasterizeVisualizationPolyline(const VisualizationPolylineCommand& polylineCommand, const Rect& clipRectangle);
    void RasterizeVisualizationPointMarker(const VisualizationPointMarkerCommand& pointMarkerCommand,
                                           const Rect& clipRectangle);
    void ClearRasterTarget();
    void ClearFrameState();

    RenderCommandList _submittedCommands;
    std::vector<Fast2DPreparedFillOperation> _preparedFillOperations;
    std::vector<PreparedVisualizationOperation> _preparedVisualizationOperations;
    std::vector<QueuedOperation> _queuedOperations;
    std::vector<Rect> _clipStack;
    std::vector<Color> _rasterPixels;
    std::size_t _rasterTargetWidth{0};
    std::size_t _rasterTargetHeight{0};
    std::size_t _completedFrameCommandCount{0};
    std::size_t _submittedVisualizationCommandCount{0};
    std::size_t _completedFrameVisualizationCommandCount{0};
    std::size_t _deferredTextCommandCount{0};
    std::size_t _clipUnderflowCount{0};
};

} // namespace greenfield

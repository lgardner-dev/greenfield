#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "engine/render/IRenderer.h"

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
    void EndFrame() override;
    void ResizeRasterTarget(std::size_t width, std::size_t height);

    [[nodiscard]] std::size_t SubmittedCommandCount() const noexcept;
    [[nodiscard]] std::size_t CompletedFrameCommandCount() const noexcept;
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
    void Consume(const RenderCommand& renderCommand);
    void ConsumeFillRectangle(const RenderCommand& renderCommand);
    void ConsumePushClip(const RenderCommand& renderCommand);
    void ConsumePopClip();
    void ConsumeDrawText();
    void RasterizePreparedFillOperations();
    void RasterizeFillRectangle(const Fast2DPreparedFillOperation& fillOperation);
    void RasterizeFillInterior(const Fast2DPreparedFillOperation& fillOperation);
    void RasterizeRoundedFillInterior(const Fast2DPreparedFillOperation& fillOperation);
    void RasterizeBorder(const Fast2DPreparedFillOperation& fillOperation);
    void RasterizeHardEdgedBorder(const Fast2DPreparedFillOperation& fillOperation);
    void RasterizeRoundedBorder(const Fast2DPreparedFillOperation& fillOperation);
    void ClearRasterTarget();
    void ClearFrameState();

    RenderCommandList _submittedCommands;
    std::vector<Fast2DPreparedFillOperation> _preparedFillOperations;
    std::vector<Rect> _clipStack;
    std::vector<Color> _rasterPixels;
    std::size_t _rasterTargetWidth{0};
    std::size_t _rasterTargetHeight{0};
    std::size_t _completedFrameCommandCount{0};
    std::size_t _deferredTextCommandCount{0};
    std::size_t _clipUnderflowCount{0};
};

} // namespace greenfield

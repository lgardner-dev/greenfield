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

    void BeginFrame() override;
    void Submit(const RenderCommandList& renderCommands) override;
    void EndFrame() override;

    [[nodiscard]] std::size_t SubmittedCommandCount() const noexcept;
    [[nodiscard]] std::size_t CompletedFrameCommandCount() const noexcept;
    [[nodiscard]] std::size_t PreparedFillOperationCount() const noexcept;
    [[nodiscard]] std::size_t DeferredTextCommandCount() const noexcept;
    [[nodiscard]] std::size_t ClipUnderflowCount() const noexcept;
    [[nodiscard]] std::span<const RenderCommand> SubmittedCommands() const noexcept;
    [[nodiscard]] std::span<const Fast2DPreparedFillOperation> PreparedFillOperations() const noexcept;

private:
    void Consume(const RenderCommand& renderCommand);
    void ConsumeFillRectangle(const RenderCommand& renderCommand);
    void ConsumePushClip(const RenderCommand& renderCommand);
    void ConsumePopClip();
    void ConsumeDrawText();
    void ClearFrameState();

    RenderCommandList _submittedCommands;
    std::vector<Fast2DPreparedFillOperation> _preparedFillOperations;
    std::vector<Rect> _clipStack;
    std::size_t _completedFrameCommandCount{0};
    std::size_t _deferredTextCommandCount{0};
    std::size_t _clipUnderflowCount{0};
};

} // namespace greenfield

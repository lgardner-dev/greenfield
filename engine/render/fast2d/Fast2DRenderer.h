#pragma once

#include <cstddef>
#include <span>

#include "engine/render/IRenderer.h"

namespace greenfield
{

class Fast2DRenderer final : public IRenderer
{
public:
    Fast2DRenderer() = default;

    void BeginFrame() override;
    void Submit(const RenderCommandList& renderCommands) override;
    void EndFrame() override;

    [[nodiscard]] std::size_t SubmittedCommandCount() const noexcept;
    [[nodiscard]] std::size_t CompletedFrameCommandCount() const noexcept;
    [[nodiscard]] std::span<const RenderCommand> SubmittedCommands() const noexcept;

private:
    RenderCommandList _submittedCommands;
    std::size_t _completedFrameCommandCount{0};
};

} // namespace greenfield

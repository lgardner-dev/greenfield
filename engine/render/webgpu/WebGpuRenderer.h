#pragma once

#include <cstddef>

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
    WebGpuContext* _context{nullptr};
    RenderCommandList _submittedCommands;
};

} // namespace greenfield

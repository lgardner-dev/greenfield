#pragma once

#include "engine/render/RenderCommandList.h"
#include "engine/ui/Layout.h"
#include "engine/ui/Style.h"

namespace greenfield
{

class UiContext
{
public:
    void BeginFrame(const Layout& layout);
    void DrawFilledRectangle(const Rect& rectangle, const Color& color);
    [[nodiscard]] const RenderCommandList& EndFrame();

    void SetStyle(const Style& style);
    [[nodiscard]] const Style& GetStyle() const noexcept;
    [[nodiscard]] const Layout& GetLayout() const noexcept;

private:
    Style _style{};
    Layout _layout{};
    RenderCommandList _renderCommands{};
};

} // namespace greenfield

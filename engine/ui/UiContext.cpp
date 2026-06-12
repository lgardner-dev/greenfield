#include "engine/ui/UiContext.h"

namespace greenfield
{

void UiContext::BeginFrame(const Layout& layout)
{
    _layout = layout;
    _renderCommands.Clear();
}

void UiContext::DrawFilledRectangle(const Rect& rectangle, const Color& color, float cornerRadius)
{
    _renderCommands.AddFillRectangle(rectangle, color, cornerRadius);
}

const RenderCommandList& UiContext::EndFrame()
{
    return _renderCommands;
}

void UiContext::SetStyle(const Style& style)
{
    _style = style;
}

const Style& UiContext::GetStyle() const noexcept
{
    return _style;
}

const Layout& UiContext::GetLayout() const noexcept
{
    return _layout;
}

} // namespace greenfield

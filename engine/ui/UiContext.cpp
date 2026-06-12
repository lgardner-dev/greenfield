#include "engine/ui/UiContext.h"

namespace greenfield
{

void UiContext::BeginFrame(const Layout& layout)
{
    BeginFrame(layout, InputState{});
}

void UiContext::BeginFrame(const Layout& layout, const InputState& inputState)
{
    _layout = layout;
    _inputState = inputState;
    _renderCommands.Clear();

    if (!_inputState.leftMouseButtonDown && !_inputState.leftMouseButtonReleased)
    {
        _activeButtonName.clear();
    }
}

void UiContext::DrawFilledRectangle(const Rect& rectangle, const Color& color, float cornerRadius)
{
    _renderCommands.AddFillRectangle(rectangle, color, cornerRadius);
}

bool UiContext::Button(const std::string& name, const Rect& bounds)
{
    return Button(name, bounds, ButtonStyle{.normal = _style.accent});
}

bool UiContext::Button(const std::string& name, const Rect& bounds, const ButtonStyle& buttonStyle)
{
    const bool isHovered = ContainsPoint(bounds, _inputState.mousePosition);
    if (isHovered && _inputState.leftMouseButtonPressed)
    {
        _activeButtonName = name;
    }

    const bool isClicked = isHovered && IsButtonActive(name) && _inputState.leftMouseButtonReleased;
    DrawFilledRectangle(bounds, GetButtonColor(name, bounds, buttonStyle), buttonStyle.cornerRadius);

    if (IsButtonActive(name) && _inputState.leftMouseButtonReleased)
    {
        _activeButtonName.clear();
    }

    return isClicked;
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

bool UiContext::IsButtonActive(const std::string& name) const
{
    return _activeButtonName == name;
}

Color UiContext::GetButtonColor(
    const std::string& name,
    const Rect& bounds,
    const ButtonStyle& buttonStyle) const
{
    const bool isHovered = ContainsPoint(bounds, _inputState.mousePosition);
    if (IsButtonActive(name) && _inputState.leftMouseButtonDown)
    {
        return buttonStyle.pressed;
    }

    if (isHovered)
    {
        return buttonStyle.hovered;
    }

    return buttonStyle.normal;
}

} // namespace greenfield

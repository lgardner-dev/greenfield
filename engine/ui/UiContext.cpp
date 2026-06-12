#include "engine/ui/UiContext.h"

namespace greenfield
{

namespace
{

constexpr Vec2 DefaultLayoutItemSize{160.0f, 48.0f};

} // namespace

void UiContext::BeginFrame(const Layout& layout)
{
    BeginFrame(layout, InputState{});
}

void UiContext::BeginFrame(const Layout& layout, const InputState& inputState)
{
    _layout = layout;
    _inputState = inputState;
    _renderCommands.Clear();
    _layoutStack.clear();
    BeginColumn(LayoutContainer{
        .bounds = _layout.bounds,
        .padding = _layout.padding,
        .gap = _layout.gap,
        .itemSize = _layout.itemSize,
    });

    if (!_inputState.leftMouseButtonDown && !_inputState.leftMouseButtonReleased)
    {
        _activeButtonName.clear();
    }
}

void UiContext::BeginColumn(const LayoutContainer& container)
{
    BeginLayoutContainer(container, LayoutDirection::Column);
}

void UiContext::EndColumn()
{
    EndLayoutContainer(LayoutDirection::Column);
}

void UiContext::BeginRow(const LayoutContainer& container)
{
    BeginLayoutContainer(container, LayoutDirection::Row);
}

void UiContext::EndRow()
{
    EndLayoutContainer(LayoutDirection::Row);
}

void UiContext::AddSpacing(float spacing)
{
    if (_layoutStack.empty())
    {
        return;
    }

    LayoutFrame& frame = _layoutStack.back();
    if (frame.direction == LayoutDirection::Column)
    {
        frame.cursor.y += spacing;
        return;
    }

    frame.cursor.x += spacing;
}

void UiContext::DrawFilledRectangle(const Rect& rectangle, const Color& color, float cornerRadius)
{
    _renderCommands.AddFillRectangle(rectangle, color, cornerRadius);
}

void UiContext::Panel(const Rect& bounds, const Color& color, float cornerRadius)
{
    DrawFilledRectangle(bounds, color, cornerRadius);
}

Rect UiContext::Panel(const Color& color, float cornerRadius)
{
    const Rect bounds = GetNextLayoutBounds(Vec2{});
    Panel(bounds, color, cornerRadius);
    return bounds;
}

Rect UiContext::Panel(const Color& color, const Vec2& itemSize, float cornerRadius)
{
    const Rect bounds = GetNextLayoutBounds(itemSize);
    Panel(bounds, color, cornerRadius);
    return bounds;
}

bool UiContext::Button(const std::string& name)
{
    return Button(name, ButtonStyle{.normal = _style.accent});
}

bool UiContext::Button(const std::string& name, const ButtonStyle& buttonStyle)
{
    const Rect bounds = GetNextLayoutBounds(Vec2{});
    return Button(name, bounds, buttonStyle);
}

bool UiContext::Button(const std::string& name, const Vec2& itemSize, const ButtonStyle& buttonStyle)
{
    const Rect bounds = GetNextLayoutBounds(itemSize);
    return Button(name, bounds, buttonStyle);
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

void UiContext::BeginLayoutContainer(const LayoutContainer& container, LayoutDirection direction)
{
    const Rect contentBounds = GetContentBounds(container);
    _layoutStack.push_back(LayoutFrame{
        .direction = direction,
        .contentBounds = contentBounds,
        .cursor = contentBounds.position,
        .gap = container.gap,
        .itemSize = container.itemSize,
    });
}

void UiContext::EndLayoutContainer(LayoutDirection direction)
{
    if (_layoutStack.size() <= 1U || _layoutStack.back().direction != direction)
    {
        return;
    }

    _layoutStack.pop_back();
}

Rect UiContext::GetNextLayoutBounds(const Vec2& requestedItemSize)
{
    if (_layoutStack.empty())
    {
        return Rect{};
    }

    LayoutFrame& frame = _layoutStack.back();
    const Vec2 itemSize = ResolveItemSize(frame, requestedItemSize);
    const Rect bounds{
        .position = frame.cursor,
        .size = itemSize,
    };
    AdvanceLayoutCursor(frame, itemSize);
    return bounds;
}

Rect UiContext::GetContentBounds(const LayoutContainer& container) const noexcept
{
    const float doublePadding = container.padding * 2.0f;
    const Vec2 size{
        .x = container.bounds.size.x - doublePadding,
        .y = container.bounds.size.y - doublePadding,
    };

    return Rect{
        .position =
            Vec2{
                .x = container.bounds.position.x + container.padding,
                .y = container.bounds.position.y + container.padding,
            },
        .size =
            Vec2{
                .x = size.x > 0.0f ? size.x : 0.0f,
                .y = size.y > 0.0f ? size.y : 0.0f,
            },
    };
}

Vec2 UiContext::ResolveItemSize(const LayoutFrame& frame, const Vec2& requestedItemSize) const noexcept
{
    Vec2 itemSize = requestedItemSize;
    if (itemSize.x <= 0.0f)
    {
        itemSize.x = frame.itemSize.x;
    }

    if (itemSize.y <= 0.0f)
    {
        itemSize.y = frame.itemSize.y;
    }

    if (frame.direction == LayoutDirection::Column)
    {
        itemSize.x = itemSize.x > 0.0f ? itemSize.x : frame.contentBounds.size.x;
        itemSize.y = itemSize.y > 0.0f ? itemSize.y : DefaultLayoutItemSize.y;
        return itemSize;
    }

    itemSize.x = itemSize.x > 0.0f ? itemSize.x : DefaultLayoutItemSize.x;
    itemSize.y = itemSize.y > 0.0f ? itemSize.y : frame.contentBounds.size.y;
    return itemSize;
}

void UiContext::AdvanceLayoutCursor(LayoutFrame& frame, const Vec2& itemSize) noexcept
{
    if (frame.direction == LayoutDirection::Column)
    {
        frame.cursor.y += itemSize.y + frame.gap;
        return;
    }

    frame.cursor.x += itemSize.x + frame.gap;
}

bool UiContext::IsButtonActive(const std::string& name) const
{
    return _activeButtonName == name;
}

Color UiContext::GetButtonColor(const std::string& name, const Rect& bounds, const ButtonStyle& buttonStyle) const
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

#include "engine/ui/UiContext.h"

namespace greenfield
{

namespace
{

constexpr Vec2 DefaultLayoutItemSize{160.0f, 48.0f};
constexpr float DefaultTextFontSize{18.0f};
constexpr float ButtonHorizontalTextInset{16.0f};
constexpr float MouseWheelScrollPixels{42.0f};

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
    _scrollPanelStack.clear();
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

void UiContext::DrawRectangle(const Rect& rectangle, const RectangleStyle& rectangleStyle)
{
    _renderCommands.AddFillRectangle(rectangle, rectangleStyle.fillColor, rectangleStyle.cornerRadius, rectangleStyle.borderColor, rectangleStyle.borderThickness);
}

void UiContext::DrawText(const std::string& text, const Rect& bounds, float fontSize, const Color& color)
{
    if (text.empty() || bounds.size.x <= 0.0f || bounds.size.y <= 0.0f || fontSize <= 0.0f || color.alpha <= 0.0f)
    {
        return;
    }

    _renderCommands.AddText(text, bounds, fontSize, color);
}

Rect UiContext::BeginVerticalScrollPanel(const std::string& name, const Rect& bounds, float contentHeight)
{
    const float scrollOffset = GetClampedScrollOffset(name, bounds, contentHeight);
    _renderCommands.PushClip(bounds);
    _scrollPanelStack.push_back(ScrollPanelFrame{
        .name = name,
        .bounds = bounds,
    });

    return Rect{
        .position = Vec2{.x = bounds.position.x, .y = bounds.position.y - scrollOffset},
        .size = Vec2{.x = bounds.size.x, .y = contentHeight},
    };
}

void UiContext::EndVerticalScrollPanel()
{
    if (_scrollPanelStack.empty())
    {
        return;
    }

    _scrollPanelStack.pop_back();
    _renderCommands.PopClip();
}

Rect UiContext::Text(const std::string& text)
{
    return Text(text, DefaultTextFontSize, _style.textPrimary);
}

Rect UiContext::Text(const std::string& text, float fontSize, const Color& color)
{
    return Text(text, Vec2{0.0f, fontSize * 1.4f}, fontSize, color);
}

Rect UiContext::Text(const std::string& text, const Vec2& itemSize, float fontSize, const Color& color)
{
    const Rect bounds = GetNextLayoutBounds(itemSize);
    DrawText(text, bounds, fontSize, color);
    return bounds;
}

void UiContext::Panel(const Rect& bounds, const Color& color, float cornerRadius)
{
    Panel(bounds,
          RectangleStyle{
              .fillColor = color,
              .cornerRadius = cornerRadius,
              .borderColor = _style.panelBorder,
              .borderThickness = _style.panelBorderThickness,
          });
}

void UiContext::Panel(const Rect& bounds, const RectangleStyle& rectangleStyle)
{
    DrawRectangle(bounds, rectangleStyle);
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

Rect UiContext::Panel(const RectangleStyle& rectangleStyle)
{
    const Rect bounds = GetNextLayoutBounds(Vec2{});
    Panel(bounds, rectangleStyle);
    return bounds;
}

Rect UiContext::Panel(const RectangleStyle& rectangleStyle, const Vec2& itemSize)
{
    const Rect bounds = GetNextLayoutBounds(itemSize);
    Panel(bounds, rectangleStyle);
    return bounds;
}

bool UiContext::Button(const std::string& name)
{
    return Button(name, name);
}

bool UiContext::Button(const std::string& name, const std::string& label)
{
    return Button(name, label, ButtonStyle{.normal = _style.accent});
}

bool UiContext::Button(const std::string& name, const ButtonStyle& buttonStyle)
{
    return Button(name, name, buttonStyle);
}

bool UiContext::Button(const std::string& name, const std::string& label, const ButtonStyle& buttonStyle)
{
    const Rect bounds = GetNextLayoutBounds(Vec2{});
    return Button(name, label, bounds, buttonStyle);
}

bool UiContext::Button(const std::string& name, const Vec2& itemSize, const ButtonStyle& buttonStyle)
{
    return Button(name, name, itemSize, buttonStyle);
}

bool UiContext::Button(const std::string& name, const std::string& label, const Vec2& itemSize,
                       const ButtonStyle& buttonStyle)
{
    const Rect bounds = GetNextLayoutBounds(itemSize);
    return Button(name, label, bounds, buttonStyle);
}

bool UiContext::Button(const std::string& name, const Rect& bounds)
{
    return Button(name, name, bounds);
}

bool UiContext::Button(const std::string& name, const std::string& label, const Rect& bounds)
{
    return Button(name, label, bounds, ButtonStyle{.normal = _style.accent});
}

bool UiContext::Button(const std::string& name, const Rect& bounds, const ButtonStyle& buttonStyle)
{
    return Button(name, name, bounds, buttonStyle);
}

bool UiContext::Button(const std::string& name, const std::string& label, const Rect& bounds,
                       const ButtonStyle& buttonStyle)
{
    const bool isHovered = ContainsPoint(bounds, _inputState.mousePosition);
    if (isHovered && _inputState.leftMouseButtonPressed)
    {
        _activeButtonName = name;
    }

    const bool isClicked = isHovered && IsButtonActive(name) && _inputState.leftMouseButtonReleased;
    DrawRectangle(bounds,
                  RectangleStyle{
                      .fillColor = GetButtonColor(name, bounds, buttonStyle),
                      .cornerRadius = buttonStyle.cornerRadius,
                      .borderColor = buttonStyle.border,
                      .borderThickness = buttonStyle.borderThickness,
                  });
    DrawButtonLabel(label, bounds, buttonStyle);

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

float UiContext::GetVerticalScrollOffset(const std::string& name) const
{
    const auto scrollOffset = _verticalScrollOffsets.find(name);
    if (scrollOffset == _verticalScrollOffsets.end())
    {
        return 0.0f;
    }

    return scrollOffset->second;
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

float UiContext::GetClampedScrollOffset(const std::string& name, const Rect& bounds, float contentHeight)
{
    const float maximumScrollOffset = contentHeight > bounds.size.y ? contentHeight - bounds.size.y : 0.0f;
    float scrollOffset = GetVerticalScrollOffset(name);
    if (ContainsPoint(bounds, _inputState.mousePosition))
    {
        scrollOffset -= _inputState.verticalScrollDelta * MouseWheelScrollPixels;
    }

    scrollOffset = ClampLayoutValue(scrollOffset, 0.0f, maximumScrollOffset);
    _verticalScrollOffsets[name] = scrollOffset;
    return scrollOffset;
}

void UiContext::DrawButtonLabel(const std::string& label, const Rect& bounds, const ButtonStyle& buttonStyle)
{
    const float labelTop = bounds.position.y + (bounds.size.y - buttonStyle.fontSize) * 0.5f - 2.0f;
    const Rect labelBounds{
        .position =
            Vec2{
                .x = bounds.position.x + ButtonHorizontalTextInset,
                .y = labelTop,
            },
        .size =
            Vec2{
                .x = bounds.size.x - ButtonHorizontalTextInset * 2.0f,
                .y = buttonStyle.fontSize * 1.4f,
            },
    };
    DrawText(label, labelBounds, buttonStyle.fontSize, buttonStyle.textColor);
}

} // namespace greenfield

#include "engine/ui/UiContext.h"

#include <algorithm>
#include <iterator>

namespace greenfield
{

namespace
{

constexpr Vec2 DefaultLayoutItemSize{160.0f, 48.0f};
constexpr float DefaultTextFontSize{18.0f};
constexpr float ButtonHorizontalTextInset{16.0f};
constexpr float CheckboxHorizontalGap{10.0f};
constexpr float CheckboxIndicatorInset{5.0f};
constexpr float ToggleHorizontalGap{10.0f};
constexpr float SliderHorizontalGap{10.0f};
constexpr float TextInputVerticalTextInsetAdjustment{2.0f};
constexpr float MouseWheelScrollPixels{42.0f};
constexpr float SliderKeyboardStepDivisor{20.0f};

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
    _focusableControlIds.clear();
    _mousePressConsumed = false;
    _mouseReleaseConsumed = false;
    BeginColumn(LayoutContainer{
        .bounds = _layout.bounds,
        .padding = _layout.padding,
        .gap = _layout.gap,
        .itemSize = _layout.itemSize,
    });

    if (!_inputState.leftMouseButtonDown && !_inputState.leftMouseButtonReleased)
    {
        ReleaseActiveControl();
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
    const UiId panelId = MakeUiId(name);
    const float scrollOffset = GetClampedScrollOffset(panelId, bounds, contentHeight);
    _renderCommands.PushClip(bounds);
    _scrollPanelStack.push_back(ScrollPanelFrame{
        .id = panelId,
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
    const UiId buttonId = MakeUiId(name);
    RegisterFocusableControl(buttonId);
    const bool isHovered = ContainsPoint(bounds, _inputState.mousePosition);
    if (isHovered && _inputState.leftMouseButtonPressed && !HasActiveControl() && !IsMousePressConsumed())
    {
        CaptureControl(buttonId);
        ConsumeMousePress();
    }

    const bool isReleased = IsControlActive(buttonId) && _inputState.leftMouseButtonReleased;
    const bool isMouseClicked = isHovered && isReleased && !IsMouseReleaseConsumed();
    const bool isClicked = isMouseClicked || IsKeyboardActivationRequested(buttonId);
    if (HasFocus(buttonId))
    {
        DrawFocusRing(bounds, buttonStyle.cornerRadius, buttonStyle.focus);
    }

    DrawRectangle(bounds,
                  RectangleStyle{
                      .fillColor = GetButtonColor(buttonId, bounds, buttonStyle),
                      .cornerRadius = buttonStyle.cornerRadius,
                      .borderColor = buttonStyle.border,
                      .borderThickness = buttonStyle.borderThickness,
                  });
    DrawButtonLabel(label, bounds, buttonStyle);

    if (isReleased)
    {
        ConsumeMouseRelease();
        ReleaseActiveControl();
    }

    return isClicked;
}

bool UiContext::Checkbox(const std::string& name)
{
    return Checkbox(name, name);
}

bool UiContext::Checkbox(const std::string& name, const std::string& label)
{
    return Checkbox(name, label, CheckboxStyle{});
}

bool UiContext::Checkbox(const std::string& name, const CheckboxStyle& checkboxStyle)
{
    return Checkbox(name, name, checkboxStyle);
}

bool UiContext::Checkbox(const std::string& name, const std::string& label,
                         const CheckboxStyle& checkboxStyle)
{
    const Rect bounds = GetNextLayoutBounds(Vec2{});
    return Checkbox(name, label, bounds, checkboxStyle);
}

bool UiContext::Checkbox(const std::string& name, const Vec2& itemSize,
                         const CheckboxStyle& checkboxStyle)
{
    return Checkbox(name, name, itemSize, checkboxStyle);
}

bool UiContext::Checkbox(const std::string& name, const std::string& label, const Vec2& itemSize,
                         const CheckboxStyle& checkboxStyle)
{
    const Rect bounds = GetNextLayoutBounds(itemSize);
    return Checkbox(name, label, bounds, checkboxStyle);
}

bool UiContext::Checkbox(const std::string& name, const Rect& bounds)
{
    return Checkbox(name, name, bounds);
}

bool UiContext::Checkbox(const std::string& name, const std::string& label, const Rect& bounds)
{
    return Checkbox(name, label, bounds, CheckboxStyle{});
}

bool UiContext::Checkbox(const std::string& name, const Rect& bounds,
                         const CheckboxStyle& checkboxStyle)
{
    return Checkbox(name, name, bounds, checkboxStyle);
}

bool UiContext::Checkbox(const std::string& name, const std::string& label, const Rect& bounds,
                         const CheckboxStyle& checkboxStyle)
{
    const UiId checkboxId = MakeUiId(name);
    RegisterFocusableControl(checkboxId);
    const bool isHovered = ContainsPoint(bounds, _inputState.mousePosition);
    if (isHovered && _inputState.leftMouseButtonPressed && !HasActiveControl() && !IsMousePressConsumed())
    {
        CaptureControl(checkboxId);
        ConsumeMousePress();
    }

    const bool isReleased = IsControlActive(checkboxId) && _inputState.leftMouseButtonReleased;
    const bool changed =
        (isHovered && isReleased && !IsMouseReleaseConsumed()) || IsKeyboardActivationRequested(checkboxId);
    if (changed)
    {
        ToggleBooleanState(checkboxId);
    }

    DrawCheckbox(checkboxId, label, bounds, checkboxStyle);

    if (isReleased)
    {
        ConsumeMouseRelease();
        ReleaseActiveControl();
    }

    return changed;
}

bool UiContext::Toggle(const std::string& name)
{
    return Toggle(name, name);
}

bool UiContext::Toggle(const std::string& name, const std::string& label)
{
    return Toggle(name, label, ToggleStyle{});
}

bool UiContext::Toggle(const std::string& name, const ToggleStyle& toggleStyle)
{
    return Toggle(name, name, toggleStyle);
}

bool UiContext::Toggle(const std::string& name, const std::string& label,
                       const ToggleStyle& toggleStyle)
{
    const Rect bounds = GetNextLayoutBounds(Vec2{});
    return Toggle(name, label, bounds, toggleStyle);
}

bool UiContext::Toggle(const std::string& name, const Vec2& itemSize,
                       const ToggleStyle& toggleStyle)
{
    return Toggle(name, name, itemSize, toggleStyle);
}

bool UiContext::Toggle(const std::string& name, const std::string& label, const Vec2& itemSize,
                       const ToggleStyle& toggleStyle)
{
    const Rect bounds = GetNextLayoutBounds(itemSize);
    return Toggle(name, label, bounds, toggleStyle);
}

bool UiContext::Toggle(const std::string& name, const Rect& bounds)
{
    return Toggle(name, name, bounds);
}

bool UiContext::Toggle(const std::string& name, const std::string& label, const Rect& bounds)
{
    return Toggle(name, label, bounds, ToggleStyle{});
}

bool UiContext::Toggle(const std::string& name, const Rect& bounds,
                       const ToggleStyle& toggleStyle)
{
    return Toggle(name, name, bounds, toggleStyle);
}

bool UiContext::Toggle(const std::string& name, const std::string& label, const Rect& bounds,
                       const ToggleStyle& toggleStyle)
{
    const UiId toggleId = MakeUiId(name);
    RegisterFocusableControl(toggleId);
    const bool isHovered = ContainsPoint(bounds, _inputState.mousePosition);
    if (isHovered && _inputState.leftMouseButtonPressed && !HasActiveControl() && !IsMousePressConsumed())
    {
        CaptureControl(toggleId);
        ConsumeMousePress();
    }

    const bool isReleased = IsControlActive(toggleId) && _inputState.leftMouseButtonReleased;
    const bool changed =
        (isHovered && isReleased && !IsMouseReleaseConsumed()) || IsKeyboardActivationRequested(toggleId);
    if (changed)
    {
        ToggleBooleanState(toggleId);
    }

    DrawToggle(toggleId, label, bounds, toggleStyle);

    if (isReleased)
    {
        ConsumeMouseRelease();
        ReleaseActiveControl();
    }

    return changed;
}

bool UiContext::Slider(const std::string& name, float minimumValue, float maximumValue)
{
    return Slider(name, name, minimumValue, maximumValue);
}

bool UiContext::Slider(const std::string& name, const std::string& label, float minimumValue,
                       float maximumValue)
{
    return Slider(name, label, minimumValue, maximumValue, SliderStyle{});
}

bool UiContext::Slider(const std::string& name, float minimumValue, float maximumValue,
                       const SliderStyle& sliderStyle)
{
    return Slider(name, name, minimumValue, maximumValue, sliderStyle);
}

bool UiContext::Slider(const std::string& name, const std::string& label, float minimumValue,
                       float maximumValue, const SliderStyle& sliderStyle)
{
    const Rect bounds = GetNextLayoutBounds(Vec2{});
    return Slider(name, label, bounds, minimumValue, maximumValue, sliderStyle);
}

bool UiContext::Slider(const std::string& name, const Vec2& itemSize, float minimumValue,
                       float maximumValue, const SliderStyle& sliderStyle)
{
    return Slider(name, name, itemSize, minimumValue, maximumValue, sliderStyle);
}

bool UiContext::Slider(const std::string& name, const std::string& label, const Vec2& itemSize,
                       float minimumValue, float maximumValue, const SliderStyle& sliderStyle)
{
    const Rect bounds = GetNextLayoutBounds(itemSize);
    return Slider(name, label, bounds, minimumValue, maximumValue, sliderStyle);
}

bool UiContext::Slider(const std::string& name, const Rect& bounds, float minimumValue,
                       float maximumValue)
{
    return Slider(name, name, bounds, minimumValue, maximumValue);
}

bool UiContext::Slider(const std::string& name, const std::string& label, const Rect& bounds,
                       float minimumValue, float maximumValue)
{
    return Slider(name, label, bounds, minimumValue, maximumValue, SliderStyle{});
}

bool UiContext::Slider(const std::string& name, const Rect& bounds, float minimumValue,
                       float maximumValue, const SliderStyle& sliderStyle)
{
    return Slider(name, name, bounds, minimumValue, maximumValue, sliderStyle);
}

bool UiContext::Slider(const std::string& name, const std::string& label, const Rect& bounds,
                       float minimumValue, float maximumValue, const SliderStyle& sliderStyle)
{
    const UiId sliderId = MakeUiId(name);
    RegisterFocusableControl(sliderId);
    const bool isHovered = ContainsPoint(bounds, _inputState.mousePosition);
    const float normalizedMinimumValue = std::min(minimumValue, maximumValue);
    const float normalizedMaximumValue = std::max(minimumValue, maximumValue);
    const Rect trackBounds = GetSliderTrackBounds(bounds, sliderStyle);

    if (isHovered && _inputState.leftMouseButtonPressed && !HasActiveControl() && !IsMousePressConsumed())
    {
        CaptureControl(sliderId);
        ConsumeMousePress();
    }

    bool changed = false;
    float currentValue = GetClampedNumericState(sliderId, normalizedMinimumValue, normalizedMinimumValue, normalizedMaximumValue);
    if (IsSliderKeyboardAdjustmentAllowed(sliderId))
    {
        const float keyboardStep = GetSliderKeyboardStep(normalizedMinimumValue, normalizedMaximumValue);
        float nextValue = currentValue;
        if (_inputState.leftArrowPressed)
        {
            nextValue -= keyboardStep;
        }
        if (_inputState.rightArrowPressed)
        {
            nextValue += keyboardStep;
        }

        nextValue = ClampLayoutValue(nextValue, normalizedMinimumValue, normalizedMaximumValue);
        changed = nextValue != currentValue;
        if (changed)
        {
            SetNumericState(sliderId, nextValue);
            currentValue = nextValue;
        }
    }

    if (IsControlActive(sliderId) && _inputState.leftMouseButtonDown)
    {
        const float nextValue =
            GetSliderValueAtMousePosition(trackBounds, normalizedMinimumValue, normalizedMaximumValue);
        const bool mouseChanged = nextValue != currentValue;
        if (mouseChanged)
        {
            SetNumericState(sliderId, nextValue);
        }
        changed = changed || mouseChanged;
    }

    DrawSlider(sliderId, label, bounds, normalizedMinimumValue, normalizedMaximumValue, sliderStyle);

    if (IsControlActive(sliderId) && _inputState.leftMouseButtonReleased)
    {
        ConsumeMouseRelease();
        ReleaseActiveControl();
    }

    return changed;
}

bool UiContext::TextInput(const std::string& name)
{
    return TextInput(name, TextInputStyle{});
}

bool UiContext::TextInput(const std::string& name, const TextInputStyle& textInputStyle)
{
    const Rect bounds = GetNextLayoutBounds(Vec2{});
    return TextInput(name, bounds, textInputStyle);
}

bool UiContext::TextInput(const std::string& name, const Vec2& itemSize, const TextInputStyle& textInputStyle)
{
    const Rect bounds = GetNextLayoutBounds(itemSize);
    return TextInput(name, bounds, textInputStyle);
}

bool UiContext::TextInput(const std::string& name, const Rect& bounds)
{
    return TextInput(name, bounds, TextInputStyle{});
}

bool UiContext::TextInput(const std::string& name, const Rect& bounds, const TextInputStyle& textInputStyle)
{
    const UiId textInputId = MakeUiId(name);
    RegisterFocusableControl(textInputId);
    const bool isHovered = ContainsPoint(bounds, _inputState.mousePosition);
    if (isHovered && _inputState.leftMouseButtonPressed && !HasActiveControl() && !IsMousePressConsumed())
    {
        RequestFocus(textInputId);
        CaptureControl(textInputId);
        ConsumeMousePress();
    }

    const std::string previousText = GetTextState(textInputId);
    std::string currentText = previousText;
    if (IsTextInputEditingAllowed(textInputId))
    {
        if (!_inputState.committedText.empty())
        {
            currentText += _inputState.committedText;
        }

        if (_inputState.backspacePressed && !currentText.empty())
        {
            currentText.pop_back();
        }
    }

    const bool changed = currentText != previousText;
    if (changed)
    {
        SetTextState(textInputId, currentText);
    }

    DrawTextInput(textInputId, bounds, textInputStyle);

    if (IsControlActive(textInputId) && _inputState.leftMouseButtonReleased)
    {
        ConsumeMouseRelease();
        ReleaseActiveControl();
    }

    return changed;
}

const RenderCommandList& UiContext::EndFrame()
{
    ApplyFocusTraversal();
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

UiSurface UiContext::GetRootSurface() const noexcept
{
    return MakeRootUiSurface(_layout);
}

void UiContext::RequestFocus(const UiId& controlId)
{
    _focusedControlId = controlId;
}

void UiContext::RequestFocus(const std::string& name)
{
    RequestFocus(MakeUiId(name));
}

void UiContext::ClearFocus() noexcept
{
    _focusedControlId.reset();
}

bool UiContext::HasFocus(const UiId& controlId) const noexcept
{
    return _focusedControlId.has_value() && _focusedControlId.value() == controlId;
}

bool UiContext::HasFocus(const std::string& name) const
{
    return HasFocus(MakeUiId(name));
}

const std::optional<UiId>& UiContext::FocusedControlId() const noexcept
{
    return _focusedControlId;
}

float UiContext::GetVerticalScrollOffset(const std::string& name) const
{
    return GetVerticalScrollOffset(MakeUiId(name));
}

float UiContext::GetVerticalScrollOffset(const UiId& panelId) const
{
    const auto scrollOffset = _verticalScrollOffsets.find(panelId);
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

bool UiContext::HasActiveControl() const noexcept
{
    return _activeControlId.has_value();
}

bool UiContext::IsControlActive(const UiId& controlId) const noexcept
{
    return _activeControlId.has_value() && _activeControlId.value() == controlId;
}

void UiContext::CaptureControl(const UiId& controlId)
{
    _activeControlId = controlId;
}

void UiContext::ReleaseActiveControl() noexcept
{
    _activeControlId.reset();
}

bool UiContext::IsMousePressConsumed() const noexcept
{
    return _mousePressConsumed;
}

bool UiContext::IsMouseReleaseConsumed() const noexcept
{
    return _mouseReleaseConsumed;
}

void UiContext::ConsumeMousePress() noexcept
{
    _mousePressConsumed = true;
}

void UiContext::ConsumeMouseRelease() noexcept
{
    _mouseReleaseConsumed = true;
}

void UiContext::RegisterFocusableControl(const UiId& controlId)
{
    _focusableControlIds.push_back(controlId);
}

void UiContext::ApplyFocusTraversal()
{
    if (_inputState.shiftTabPressed)
    {
        MoveFocusBackward();
        return;
    }

    if (_inputState.tabPressed)
    {
        MoveFocusForward();
    }
}

void UiContext::MoveFocusForward()
{
    if (_focusableControlIds.empty())
    {
        return;
    }

    if (!_focusedControlId.has_value())
    {
        RequestFocus(_focusableControlIds.front());
        return;
    }

    const auto focusedControl = std::find(_focusableControlIds.begin(), _focusableControlIds.end(), _focusedControlId.value());
    if (focusedControl == _focusableControlIds.end())
    {
        RequestFocus(_focusableControlIds.front());
        return;
    }

    const auto nextControl = std::next(focusedControl);
    RequestFocus(nextControl == _focusableControlIds.end() ? _focusableControlIds.front() : *nextControl);
}

void UiContext::MoveFocusBackward()
{
    if (_focusableControlIds.empty())
    {
        return;
    }

    if (!_focusedControlId.has_value())
    {
        RequestFocus(_focusableControlIds.back());
        return;
    }

    const auto focusedControl = std::find(_focusableControlIds.begin(), _focusableControlIds.end(), _focusedControlId.value());
    if (focusedControl == _focusableControlIds.end())
    {
        RequestFocus(_focusableControlIds.back());
        return;
    }

    RequestFocus(focusedControl == _focusableControlIds.begin() ? _focusableControlIds.back()
                                                                : *std::prev(focusedControl));
}

bool UiContext::IsKeyboardActivationRequested(const UiId& controlId) const noexcept
{
    return HasFocus(controlId) && !HasActiveControl() && (_inputState.enterPressed || _inputState.spacePressed);
}

bool UiContext::IsSliderKeyboardAdjustmentAllowed(const UiId& controlId) const noexcept
{
    return HasFocus(controlId) && !HasActiveControl() && (_inputState.leftArrowPressed || _inputState.rightArrowPressed);
}

bool UiContext::IsTextInputEditingAllowed(const UiId& controlId) const noexcept
{
    return HasFocus(controlId) && (!HasActiveControl() || IsControlActive(controlId));
}

bool UiContext::GetBooleanState(const UiId& controlId) const
{
    const auto booleanState = _booleanStates.find(controlId);
    if (booleanState == _booleanStates.end())
    {
        return false;
    }

    return booleanState->second;
}

void UiContext::SetBooleanState(const UiId& controlId, bool value)
{
    _booleanStates[controlId] = value;
}

void UiContext::ToggleBooleanState(const UiId& controlId)
{
    SetBooleanState(controlId, !GetBooleanState(controlId));
}

float UiContext::GetNumericState(const UiId& controlId, float defaultValue) const
{
    const auto numericState = _numericStates.find(controlId);
    if (numericState == _numericStates.end())
    {
        return defaultValue;
    }

    return numericState->second;
}

void UiContext::SetNumericState(const UiId& controlId, float value)
{
    _numericStates[controlId] = value;
}

float UiContext::GetClampedNumericState(const UiId& controlId, float defaultValue, float minimum, float maximum)
{
    const float normalizedMinimum = std::min(minimum, maximum);
    const float normalizedMaximum = std::max(minimum, maximum);
    const float value = ClampLayoutValue(GetNumericState(controlId, defaultValue), normalizedMinimum, normalizedMaximum);
    SetNumericState(controlId, value);
    return value;
}

std::string UiContext::GetTextState(const UiId& controlId) const
{
    const auto textState = _textStates.find(controlId);
    if (textState == _textStates.end())
    {
        return {};
    }

    return textState->second;
}

void UiContext::SetTextState(const UiId& controlId, const std::string& value)
{
    _textStates[controlId] = value;
}

float UiContext::GetSliderKeyboardStep(float minimumValue, float maximumValue) const noexcept
{
    const float range = std::max(0.0f, maximumValue - minimumValue);
    return range / SliderKeyboardStepDivisor;
}

Rect UiContext::GetOutsetRectangle(const Rect& rectangle, float outset) const noexcept
{
    return Rect{
        .position = Vec2{
            .x = rectangle.position.x - outset,
            .y = rectangle.position.y - outset,
        },
        .size = Vec2{
            .x = std::max(0.0f, rectangle.size.x + outset * 2.0f),
            .y = std::max(0.0f, rectangle.size.y + outset * 2.0f),
        },
    };
}

void UiContext::DrawFocusRing(const Rect& rectangle, float cornerRadius, const FocusStyle& focusStyle)
{
    if (focusStyle.kind == FocusVisualKind::None || focusStyle.thickness <= 0.0f || focusStyle.color.alpha <= 0.0f)
    {
        return;
    }

    const Rect focusRectangle = GetOutsetRectangle(rectangle, focusStyle.outset);
    if (focusRectangle.size.x <= 0.0f || focusRectangle.size.y <= 0.0f)
    {
        return;
    }

    DrawRectangle(focusRectangle,
                  RectangleStyle{
                      .fillColor = Color{0.0f, 0.0f, 0.0f, 0.0f},
                      .cornerRadius = std::max(0.0f, cornerRadius + focusStyle.cornerRadiusOffset),
                      .borderColor = focusStyle.color,
                      .borderThickness = focusStyle.thickness,
                  });
}

Rect UiContext::GetCheckboxBoxBounds(const Rect& bounds, const CheckboxStyle& checkboxStyle) const noexcept
{
    const float safeBoxSize = std::max(0.0f, std::min(checkboxStyle.boxSize, bounds.size.y));
    const float boxTop = bounds.position.y + (bounds.size.y - safeBoxSize) * 0.5f;
    return Rect{
        .position = Vec2{.x = bounds.position.x, .y = boxTop},
        .size = Vec2{.x = safeBoxSize, .y = safeBoxSize},
    };
}

Rect UiContext::GetToggleTrackBounds(const Rect& bounds, const ToggleStyle& toggleStyle) const noexcept
{
    const float safeTrackWidth = std::max(0.0f, std::min(toggleStyle.trackWidth, bounds.size.x));
    const float safeTrackHeight = std::max(0.0f, std::min(toggleStyle.trackHeight, bounds.size.y));
    const float trackTop = bounds.position.y + (bounds.size.y - safeTrackHeight) * 0.5f;
    return Rect{
        .position = Vec2{.x = bounds.position.x, .y = trackTop},
        .size = Vec2{.x = safeTrackWidth, .y = safeTrackHeight},
    };
}

Rect UiContext::GetSliderTrackBounds(const Rect& bounds, const SliderStyle& sliderStyle) const noexcept
{
    const float safeTrackWidth = std::max(0.0f, std::min(sliderStyle.trackWidth, bounds.size.x));
    const float safeTrackHeight = std::max(0.0f, std::min(sliderStyle.trackHeight, bounds.size.y));
    const float trackTop = bounds.position.y + (bounds.size.y - safeTrackHeight) * 0.5f;
    return Rect{
        .position = Vec2{.x = bounds.position.x, .y = trackTop},
        .size = Vec2{.x = safeTrackWidth, .y = safeTrackHeight},
    };
}

Color UiContext::GetButtonColor(const UiId& buttonId, const Rect& bounds, const ButtonStyle& buttonStyle) const
{
    const bool isHovered = ContainsPoint(bounds, _inputState.mousePosition);
    if (IsControlActive(buttonId) && _inputState.leftMouseButtonDown)
    {
        return buttonStyle.pressed;
    }

    if (isHovered)
    {
        return buttonStyle.hovered;
    }

    return buttonStyle.normal;
}

Color UiContext::GetCheckboxBoxColor(const UiId& checkboxId, const Rect& bounds,
                                     const CheckboxStyle& checkboxStyle) const
{
    const bool isHovered = ContainsPoint(bounds, _inputState.mousePosition);
    if (IsControlActive(checkboxId) && _inputState.leftMouseButtonDown)
    {
        return checkboxStyle.boxPressed;
    }

    if (isHovered)
    {
        return checkboxStyle.boxHovered;
    }

    return checkboxStyle.boxFill;
}

Color UiContext::GetToggleTrackColor(const UiId& toggleId, const Rect& bounds,
                                     const ToggleStyle& toggleStyle) const
{
    const bool isHovered = ContainsPoint(bounds, _inputState.mousePosition);
    if (IsControlActive(toggleId) && _inputState.leftMouseButtonDown)
    {
        return toggleStyle.trackPressed;
    }

    if (GetBooleanState(toggleId))
    {
        return toggleStyle.trackOn;
    }

    if (isHovered)
    {
        return toggleStyle.trackHovered;
    }

    return toggleStyle.trackOff;
}

Color UiContext::GetSliderTrackColor(const UiId& sliderId, const Rect& bounds,
                                     const SliderStyle& sliderStyle) const
{
    const bool isHovered = ContainsPoint(bounds, _inputState.mousePosition);
    if (IsControlActive(sliderId) && _inputState.leftMouseButtonDown)
    {
        return sliderStyle.trackPressed;
    }

    if (isHovered)
    {
        return sliderStyle.trackHovered;
    }

    return sliderStyle.trackFill;
}

Color UiContext::GetTextInputColor(const UiId& textInputId, const Rect& bounds,
                                   const TextInputStyle& textInputStyle) const
{
    if (IsControlActive(textInputId) && _inputState.leftMouseButtonDown)
    {
        return textInputStyle.hoveredFillColor;
    }

    if (ContainsPoint(bounds, _inputState.mousePosition))
    {
        return textInputStyle.hoveredFillColor;
    }

    return textInputStyle.fillColor;
}

float UiContext::GetNormalizedSliderValue(float value, float minimumValue, float maximumValue) const noexcept
{
    if (minimumValue == maximumValue)
    {
        return 0.0f;
    }

    return ClampLayoutValue((value - minimumValue) / (maximumValue - minimumValue), 0.0f, 1.0f);
}

float UiContext::GetSliderValueAtMousePosition(const Rect& trackBounds, float minimumValue,
                                               float maximumValue) const noexcept
{
    if (minimumValue == maximumValue || trackBounds.size.x <= 0.0f)
    {
        return minimumValue;
    }

    const float normalizedPosition =
        ClampLayoutValue((_inputState.mousePosition.x - trackBounds.position.x) / trackBounds.size.x, 0.0f, 1.0f);
    return minimumValue + (maximumValue - minimumValue) * normalizedPosition;
}

float UiContext::GetClampedScrollOffset(const UiId& panelId, const Rect& bounds, float contentHeight)
{
    const float maximumScrollOffset = contentHeight > bounds.size.y ? contentHeight - bounds.size.y : 0.0f;
    float scrollOffset = GetVerticalScrollOffset(panelId);
    if (ContainsPoint(bounds, _inputState.mousePosition))
    {
        scrollOffset -= _inputState.verticalScrollDelta * MouseWheelScrollPixels;
    }

    scrollOffset = ClampLayoutValue(scrollOffset, 0.0f, maximumScrollOffset);
    _verticalScrollOffsets[panelId] = scrollOffset;
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

void UiContext::DrawCheckbox(const UiId& checkboxId, const std::string& label, const Rect& bounds,
                             const CheckboxStyle& checkboxStyle)
{
    const Rect boxBounds = GetCheckboxBoxBounds(bounds, checkboxStyle);
    const float safeBoxSize = boxBounds.size.x;
    if (HasFocus(checkboxId))
    {
        DrawFocusRing(boxBounds, checkboxStyle.cornerRadius, checkboxStyle.focus);
    }

    DrawRectangle(boxBounds,
                  RectangleStyle{
                      .fillColor = GetCheckboxBoxColor(checkboxId, bounds, checkboxStyle),
                      .cornerRadius = checkboxStyle.cornerRadius,
                      .borderColor = checkboxStyle.boxBorder,
                      .borderThickness = checkboxStyle.borderThickness,
                  });

    if (GetBooleanState(checkboxId))
    {
        DrawFilledRectangle(InsetRectangle(boxBounds, CheckboxIndicatorInset), checkboxStyle.checkFill, checkboxStyle.cornerRadius * 0.5f);
    }

    const float labelLeft = boxBounds.position.x + safeBoxSize + CheckboxHorizontalGap;
    const float labelTop = bounds.position.y + (bounds.size.y - checkboxStyle.fontSize) * 0.5f - 2.0f;
    const Rect labelBounds{
        .position = Vec2{.x = labelLeft, .y = labelTop},
        .size =
            Vec2{
                .x = std::max(0.0f, bounds.position.x + bounds.size.x - labelLeft),
                .y = checkboxStyle.fontSize * 1.4f,
            },
    };
    DrawText(label, labelBounds, checkboxStyle.fontSize, checkboxStyle.textColor);
}

void UiContext::DrawToggle(const UiId& toggleId, const std::string& label, const Rect& bounds,
                           const ToggleStyle& toggleStyle)
{
    const Rect trackBounds = GetToggleTrackBounds(bounds, toggleStyle);
    const float safeTrackWidth = trackBounds.size.x;
    const float safeTrackHeight = trackBounds.size.y;
    if (HasFocus(toggleId))
    {
        DrawFocusRing(trackBounds, toggleStyle.cornerRadius, toggleStyle.focus);
    }

    DrawRectangle(trackBounds,
                  RectangleStyle{
                      .fillColor = GetToggleTrackColor(toggleId, bounds, toggleStyle),
                      .cornerRadius = toggleStyle.cornerRadius,
                      .borderColor = toggleStyle.trackBorder,
                      .borderThickness = toggleStyle.borderThickness,
                  });

    const float safeInset = std::max(0.0f, toggleStyle.knobInset);
    const float maximumKnobSize = std::max(0.0f, safeTrackHeight - safeInset * 2.0f);
    const float safeKnobSize = std::min(toggleStyle.knobSize, maximumKnobSize);
    const float offKnobLeft = trackBounds.position.x + safeInset;
    const float onKnobLeft = trackBounds.position.x + safeTrackWidth - safeInset - safeKnobSize;
    const float knobLeft = GetBooleanState(toggleId) ? onKnobLeft : offKnobLeft;
    const Rect knobBounds{
        .position = Vec2{.x = knobLeft, .y = trackBounds.position.y + safeInset},
        .size = Vec2{.x = safeKnobSize, .y = safeKnobSize},
    };
    DrawFilledRectangle(knobBounds, toggleStyle.knobFill, toggleStyle.cornerRadius * 0.5f);

    const float labelLeft = trackBounds.position.x + safeTrackWidth + ToggleHorizontalGap;
    const float labelTop = bounds.position.y + (bounds.size.y - toggleStyle.fontSize) * 0.5f - 2.0f;
    const Rect labelBounds{
        .position = Vec2{.x = labelLeft, .y = labelTop},
        .size =
            Vec2{
                .x = std::max(0.0f, bounds.position.x + bounds.size.x - labelLeft),
                .y = toggleStyle.fontSize * 1.4f,
            },
    };
    DrawText(label, labelBounds, toggleStyle.fontSize, toggleStyle.textColor);
}

void UiContext::DrawSlider(const UiId& sliderId, const std::string& label, const Rect& bounds, float minimumValue,
                           float maximumValue, const SliderStyle& sliderStyle)
{
    const Rect trackBounds = GetSliderTrackBounds(bounds, sliderStyle);
    const float safeTrackWidth = trackBounds.size.x;
    if (HasFocus(sliderId))
    {
        DrawFocusRing(trackBounds, sliderStyle.cornerRadius, sliderStyle.focus);
    }

    DrawRectangle(trackBounds,
                  RectangleStyle{
                      .fillColor = GetSliderTrackColor(sliderId, bounds, sliderStyle),
                      .cornerRadius = sliderStyle.cornerRadius,
                      .borderColor = sliderStyle.trackBorder,
                      .borderThickness = sliderStyle.borderThickness,
                  });

    const float value = GetClampedNumericState(sliderId, minimumValue, minimumValue, maximumValue);
    const float normalizedValue = GetNormalizedSliderValue(value, minimumValue, maximumValue);
    const Rect progressBounds{
        .position = trackBounds.position,
        .size = Vec2{.x = trackBounds.size.x * normalizedValue, .y = trackBounds.size.y},
    };
    DrawFilledRectangle(progressBounds, sliderStyle.progressFill, sliderStyle.cornerRadius);

    const float safeThumbWidth = std::max(0.0f, std::min(sliderStyle.thumbWidth, bounds.size.x));
    const float safeThumbHeight = std::max(0.0f, std::min(sliderStyle.thumbHeight, bounds.size.y));
    const float thumbCenterLeft = trackBounds.position.x + trackBounds.size.x * normalizedValue - safeThumbWidth * 0.5f;
    const float maximumThumbLeft = trackBounds.position.x + std::max(0.0f, trackBounds.size.x - safeThumbWidth);
    const float thumbLeft = ClampLayoutValue(thumbCenterLeft, trackBounds.position.x, maximumThumbLeft);
    const Rect thumbBounds{
        .position =
            Vec2{
                .x = thumbLeft,
                .y = bounds.position.y + (bounds.size.y - safeThumbHeight) * 0.5f,
            },
        .size = Vec2{.x = safeThumbWidth, .y = safeThumbHeight},
    };
    DrawFilledRectangle(thumbBounds, sliderStyle.thumbFill, sliderStyle.cornerRadius);

    const float labelLeft = trackBounds.position.x + safeTrackWidth + SliderHorizontalGap;
    const float labelTop = bounds.position.y + (bounds.size.y - sliderStyle.fontSize) * 0.5f - 2.0f;
    const Rect labelBounds{
        .position = Vec2{.x = labelLeft, .y = labelTop},
        .size =
            Vec2{
                .x = std::max(0.0f, bounds.position.x + bounds.size.x - labelLeft),
                .y = sliderStyle.fontSize * 1.4f,
            },
    };
    DrawText(label, labelBounds, sliderStyle.fontSize, sliderStyle.textColor);
}

void UiContext::DrawTextInput(const UiId& textInputId, const Rect& bounds, const TextInputStyle& textInputStyle)
{
    if (HasFocus(textInputId))
    {
        DrawFocusRing(bounds, textInputStyle.cornerRadius, textInputStyle.focus);
    }

    DrawRectangle(bounds,
                  RectangleStyle{
                      .fillColor = GetTextInputColor(textInputId, bounds, textInputStyle),
                      .cornerRadius = textInputStyle.cornerRadius,
                      .borderColor = textInputStyle.borderColor,
                      .borderThickness = textInputStyle.borderThickness,
                  });

    const Rect textBounds{
        .position =
            Vec2{
                .x = bounds.position.x + textInputStyle.horizontalTextInset,
                .y = bounds.position.y + (bounds.size.y - textInputStyle.fontSize) * 0.5f -
                     TextInputVerticalTextInsetAdjustment,
            },
        .size =
            Vec2{
                .x = std::max(0.0f, bounds.size.x - textInputStyle.horizontalTextInset * 2.0f),
                .y = textInputStyle.fontSize * 1.4f,
            },
    };
    DrawText(GetTextState(textInputId), textBounds, textInputStyle.fontSize, textInputStyle.textColor);
}

} // namespace greenfield

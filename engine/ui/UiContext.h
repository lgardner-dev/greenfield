#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "engine/input/InputState.h"
#include "engine/render/RenderCommandList.h"
#include "engine/ui/Layout.h"
#include "engine/ui/Style.h"
#include "engine/ui/UiId.h"
#include "engine/ui/UiSurface.h"

namespace greenfield
{

struct ButtonStyle
{
    Color normal{0.34f, 0.60f, 0.95f, 1.0f};
    Color hovered{0.40f, 0.72f, 0.96f, 1.0f};
    Color pressed{0.18f, 0.42f, 0.72f, 1.0f};
    Color border{0.68f, 0.78f, 0.92f, 0.35f};
    Color textColor{0.94f, 0.98f, 1.0f, 1.0f};
    float fontSize{18.0f};
    float cornerRadius{8.0f};
    float borderThickness{1.0f};
};

struct CheckboxStyle
{
    Color boxFill{0.18f, 0.20f, 0.24f, 1.0f};
    Color boxHovered{0.24f, 0.28f, 0.34f, 1.0f};
    Color boxPressed{0.14f, 0.18f, 0.24f, 1.0f};
    Color boxBorder{0.68f, 0.78f, 0.92f, 0.45f};
    Color checkFill{0.34f, 0.60f, 0.95f, 1.0f};
    Color textColor{0.92f, 0.96f, 1.0f, 1.0f};
    float fontSize{18.0f};
    float boxSize{20.0f};
    float cornerRadius{4.0f};
    float borderThickness{1.0f};
};

struct ToggleStyle
{
    Color trackOff{0.18f, 0.20f, 0.24f, 1.0f};
    Color trackOn{0.34f, 0.60f, 0.95f, 1.0f};
    Color trackHovered{0.24f, 0.28f, 0.34f, 1.0f};
    Color trackPressed{0.14f, 0.18f, 0.24f, 1.0f};
    Color trackBorder{0.68f, 0.78f, 0.92f, 0.45f};
    Color knobFill{0.92f, 0.96f, 1.0f, 1.0f};
    Color textColor{0.92f, 0.96f, 1.0f, 1.0f};
    float fontSize{18.0f};
    float trackWidth{42.0f};
    float trackHeight{22.0f};
    float knobSize{16.0f};
    float knobInset{3.0f};
    float cornerRadius{11.0f};
    float borderThickness{1.0f};
};

struct SliderStyle
{
    Color trackFill{0.18f, 0.20f, 0.24f, 1.0f};
    Color trackHovered{0.24f, 0.28f, 0.34f, 1.0f};
    Color trackPressed{0.14f, 0.18f, 0.24f, 1.0f};
    Color trackBorder{0.68f, 0.78f, 0.92f, 0.45f};
    Color progressFill{0.34f, 0.60f, 0.95f, 1.0f};
    Color thumbFill{0.92f, 0.96f, 1.0f, 1.0f};
    Color textColor{0.92f, 0.96f, 1.0f, 1.0f};
    float fontSize{18.0f};
    float trackWidth{120.0f};
    float trackHeight{8.0f};
    float thumbWidth{14.0f};
    float thumbHeight{22.0f};
    float cornerRadius{4.0f};
    float borderThickness{1.0f};
};

struct RectangleStyle
{
    Color fillColor{};
    float cornerRadius{0.0f};
    Color borderColor{};
    float borderThickness{0.0f};
};

class UiContext
{
public:
    void BeginFrame(const Layout& layout);
    void BeginFrame(const Layout& layout, const InputState& inputState);
    void BeginColumn(const LayoutContainer& container);
    void EndColumn();
    void BeginRow(const LayoutContainer& container);
    void EndRow();
    void AddSpacing(float spacing);
    void DrawFilledRectangle(const Rect& rectangle, const Color& color, float cornerRadius = 0.0f);
    void DrawRectangle(const Rect& rectangle, const RectangleStyle& rectangleStyle);
    void DrawText(const std::string& text, const Rect& bounds, float fontSize, const Color& color);
    [[nodiscard]] Rect BeginVerticalScrollPanel(const std::string& name, const Rect& bounds, float contentHeight);
    void EndVerticalScrollPanel();
    Rect Text(const std::string& text);
    Rect Text(const std::string& text, float fontSize, const Color& color);
    Rect Text(const std::string& text, const Vec2& itemSize, float fontSize, const Color& color);
    void Panel(const Rect& bounds, const Color& color, float cornerRadius = 0.0f);
    void Panel(const Rect& bounds, const RectangleStyle& rectangleStyle);
    Rect Panel(const Color& color, float cornerRadius = 0.0f);
    Rect Panel(const Color& color, const Vec2& itemSize, float cornerRadius = 0.0f);
    Rect Panel(const RectangleStyle& rectangleStyle);
    Rect Panel(const RectangleStyle& rectangleStyle, const Vec2& itemSize);
    [[nodiscard]] bool Button(const std::string& name);
    [[nodiscard]] bool Button(const std::string& name, const std::string& label);
    [[nodiscard]] bool Button(const std::string& name, const ButtonStyle& buttonStyle);
    [[nodiscard]] bool Button(const std::string& name, const std::string& label, const ButtonStyle& buttonStyle);
    [[nodiscard]] bool Button(const std::string& name, const Vec2& itemSize, const ButtonStyle& buttonStyle);
    [[nodiscard]] bool Button(const std::string& name, const std::string& label, const Vec2& itemSize,
                              const ButtonStyle& buttonStyle);
    [[nodiscard]] bool Button(const std::string& name, const Rect& bounds);
    [[nodiscard]] bool Button(const std::string& name, const std::string& label, const Rect& bounds);
    [[nodiscard]] bool Button(const std::string& name, const Rect& bounds, const ButtonStyle& buttonStyle);
    [[nodiscard]] bool Button(const std::string& name, const std::string& label, const Rect& bounds,
                              const ButtonStyle& buttonStyle);
    [[nodiscard]] bool Checkbox(const std::string& name);
    [[nodiscard]] bool Checkbox(const std::string& name, const std::string& label);
    [[nodiscard]] bool Checkbox(const std::string& name, const CheckboxStyle& checkboxStyle);
    [[nodiscard]] bool Checkbox(const std::string& name, const std::string& label,
                                const CheckboxStyle& checkboxStyle);
    [[nodiscard]] bool Checkbox(const std::string& name, const Vec2& itemSize,
                                const CheckboxStyle& checkboxStyle);
    [[nodiscard]] bool Checkbox(const std::string& name, const std::string& label, const Vec2& itemSize,
                                const CheckboxStyle& checkboxStyle);
    [[nodiscard]] bool Checkbox(const std::string& name, const Rect& bounds);
    [[nodiscard]] bool Checkbox(const std::string& name, const std::string& label, const Rect& bounds);
    [[nodiscard]] bool Checkbox(const std::string& name, const Rect& bounds,
                                const CheckboxStyle& checkboxStyle);
    [[nodiscard]] bool Checkbox(const std::string& name, const std::string& label, const Rect& bounds,
                                const CheckboxStyle& checkboxStyle);
    [[nodiscard]] bool Toggle(const std::string& name);
    [[nodiscard]] bool Toggle(const std::string& name, const std::string& label);
    [[nodiscard]] bool Toggle(const std::string& name, const ToggleStyle& toggleStyle);
    [[nodiscard]] bool Toggle(const std::string& name, const std::string& label,
                              const ToggleStyle& toggleStyle);
    [[nodiscard]] bool Toggle(const std::string& name, const Vec2& itemSize,
                              const ToggleStyle& toggleStyle);
    [[nodiscard]] bool Toggle(const std::string& name, const std::string& label, const Vec2& itemSize,
                              const ToggleStyle& toggleStyle);
    [[nodiscard]] bool Toggle(const std::string& name, const Rect& bounds);
    [[nodiscard]] bool Toggle(const std::string& name, const std::string& label, const Rect& bounds);
    [[nodiscard]] bool Toggle(const std::string& name, const Rect& bounds,
                              const ToggleStyle& toggleStyle);
    [[nodiscard]] bool Toggle(const std::string& name, const std::string& label, const Rect& bounds,
                              const ToggleStyle& toggleStyle);
    [[nodiscard]] bool Slider(const std::string& name, float minimumValue, float maximumValue);
    [[nodiscard]] bool Slider(const std::string& name, const std::string& label, float minimumValue,
                              float maximumValue);
    [[nodiscard]] bool Slider(const std::string& name, float minimumValue, float maximumValue,
                              const SliderStyle& sliderStyle);
    [[nodiscard]] bool Slider(const std::string& name, const std::string& label, float minimumValue,
                              float maximumValue, const SliderStyle& sliderStyle);
    [[nodiscard]] bool Slider(const std::string& name, const Vec2& itemSize, float minimumValue,
                              float maximumValue, const SliderStyle& sliderStyle);
    [[nodiscard]] bool Slider(const std::string& name, const std::string& label, const Vec2& itemSize,
                              float minimumValue, float maximumValue, const SliderStyle& sliderStyle);
    [[nodiscard]] bool Slider(const std::string& name, const Rect& bounds, float minimumValue,
                              float maximumValue);
    [[nodiscard]] bool Slider(const std::string& name, const std::string& label, const Rect& bounds,
                              float minimumValue, float maximumValue);
    [[nodiscard]] bool Slider(const std::string& name, const Rect& bounds, float minimumValue,
                              float maximumValue, const SliderStyle& sliderStyle);
    [[nodiscard]] bool Slider(const std::string& name, const std::string& label, const Rect& bounds,
                              float minimumValue, float maximumValue, const SliderStyle& sliderStyle);
    [[nodiscard]] const RenderCommandList& EndFrame();

    void SetStyle(const Style& style);
    [[nodiscard]] const Style& GetStyle() const noexcept;
    [[nodiscard]] const Layout& GetLayout() const noexcept;
    [[nodiscard]] UiSurface GetRootSurface() const noexcept;
    void RequestFocus(const UiId& controlId);
    void RequestFocus(const std::string& name);
    void ClearFocus() noexcept;
    [[nodiscard]] bool HasFocus(const UiId& controlId) const noexcept;
    [[nodiscard]] bool HasFocus(const std::string& name) const;
    [[nodiscard]] const std::optional<UiId>& FocusedControlId() const noexcept;
    [[nodiscard]] float GetVerticalScrollOffset(const std::string& name) const;

private:
    struct LayoutFrame
    {
        LayoutDirection direction{LayoutDirection::Column};
        Rect contentBounds{};
        Vec2 cursor{};
        float gap{0.0f};
        Vec2 itemSize{};
    };

    struct ScrollPanelFrame
    {
        UiId id{};
        Rect bounds{};
    };

    void BeginLayoutContainer(const LayoutContainer& container, LayoutDirection direction);
    void EndLayoutContainer(LayoutDirection direction);
    [[nodiscard]] Rect GetNextLayoutBounds(const Vec2& requestedItemSize);
    [[nodiscard]] Rect GetContentBounds(const LayoutContainer& container) const noexcept;
    [[nodiscard]] Vec2 ResolveItemSize(const LayoutFrame& frame, const Vec2& requestedItemSize) const noexcept;
    void AdvanceLayoutCursor(LayoutFrame& frame, const Vec2& itemSize) noexcept;
    [[nodiscard]] bool HasActiveControl() const noexcept;
    [[nodiscard]] bool IsControlActive(const UiId& controlId) const noexcept;
    void CaptureControl(const UiId& controlId);
    void ReleaseActiveControl() noexcept;
    [[nodiscard]] bool IsMousePressConsumed() const noexcept;
    [[nodiscard]] bool IsMouseReleaseConsumed() const noexcept;
    void ConsumeMousePress() noexcept;
    void ConsumeMouseRelease() noexcept;
    void RegisterFocusableControl(const UiId& controlId);
    void ApplyFocusTraversal();
    void MoveFocusForward();
    void MoveFocusBackward();
    [[nodiscard]] bool GetBooleanState(const UiId& controlId) const;
    void SetBooleanState(const UiId& controlId, bool value);
    void ToggleBooleanState(const UiId& controlId);
    [[nodiscard]] float GetNumericState(const UiId& controlId, float defaultValue) const;
    void SetNumericState(const UiId& controlId, float value);
    [[nodiscard]] float GetClampedNumericState(const UiId& controlId, float defaultValue, float minimum, float maximum);
    [[nodiscard]] Color GetButtonColor(const UiId& buttonId, const Rect& bounds,
                                       const ButtonStyle& buttonStyle) const;
    [[nodiscard]] Color GetCheckboxBoxColor(const UiId& checkboxId, const Rect& bounds,
                                            const CheckboxStyle& checkboxStyle) const;
    [[nodiscard]] Color GetToggleTrackColor(const UiId& toggleId, const Rect& bounds,
                                            const ToggleStyle& toggleStyle) const;
    [[nodiscard]] Color GetSliderTrackColor(const UiId& sliderId, const Rect& bounds,
                                            const SliderStyle& sliderStyle) const;
    [[nodiscard]] float GetNormalizedSliderValue(float value, float minimumValue, float maximumValue) const noexcept;
    [[nodiscard]] float GetSliderValueAtMousePosition(const Rect& trackBounds, float minimumValue,
                                                      float maximumValue) const noexcept;
    [[nodiscard]] float GetClampedScrollOffset(const UiId& panelId, const Rect& bounds, float contentHeight);
    [[nodiscard]] float GetVerticalScrollOffset(const UiId& panelId) const;
    void DrawButtonLabel(const std::string& label, const Rect& bounds, const ButtonStyle& buttonStyle);
    void DrawCheckbox(const UiId& checkboxId, const std::string& label, const Rect& bounds,
                      const CheckboxStyle& checkboxStyle);
    void DrawToggle(const UiId& toggleId, const std::string& label, const Rect& bounds,
                    const ToggleStyle& toggleStyle);
    void DrawSlider(const UiId& sliderId, const std::string& label, const Rect& bounds, float minimumValue,
                    float maximumValue, const SliderStyle& sliderStyle);

    friend struct UiContextTestAccess;

    // Persistent runtime state survives BeginFrame so controls can respond across frames.
    Style _style{};
    std::optional<UiId> _activeControlId{};
    std::optional<UiId> _focusedControlId{};
    std::unordered_map<UiId, bool, UiIdHash> _booleanStates{};
    std::unordered_map<UiId, float, UiIdHash> _numericStates{};
    std::unordered_map<UiId, float, UiIdHash> _verticalScrollOffsets{};

    // Per-frame state is rebuilt by BeginFrame from the current layout and input snapshot.
    Layout _layout{};
    InputState _inputState{};
    std::vector<LayoutFrame> _layoutStack{};
    std::vector<ScrollPanelFrame> _scrollPanelStack{};
    std::vector<UiId> _focusableControlIds{};
    RenderCommandList _renderCommands{};
    bool _mousePressConsumed{false};
    bool _mouseReleaseConsumed{false};
};

} // namespace greenfield

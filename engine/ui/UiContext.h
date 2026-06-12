#pragma once

#include <string>
#include <vector>

#include "engine/input/InputState.h"
#include "engine/render/RenderCommandList.h"
#include "engine/ui/Layout.h"
#include "engine/ui/Style.h"

namespace greenfield
{

struct ButtonStyle
{
    Color normal{0.34f, 0.60f, 0.95f, 1.0f};
    Color hovered{0.40f, 0.72f, 0.96f, 1.0f};
    Color pressed{0.18f, 0.42f, 0.72f, 1.0f};
    Color border{0.68f, 0.78f, 0.92f, 0.35f};
    float cornerRadius{8.0f};
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
    void Panel(const Rect& bounds, const Color& color, float cornerRadius = 0.0f);
    void Panel(const Rect& bounds, const RectangleStyle& rectangleStyle);
    Rect Panel(const Color& color, float cornerRadius = 0.0f);
    Rect Panel(const Color& color, const Vec2& itemSize, float cornerRadius = 0.0f);
    Rect Panel(const RectangleStyle& rectangleStyle);
    Rect Panel(const RectangleStyle& rectangleStyle, const Vec2& itemSize);
    [[nodiscard]] bool Button(const std::string& name);
    [[nodiscard]] bool Button(const std::string& name, const ButtonStyle& buttonStyle);
    [[nodiscard]] bool Button(const std::string& name, const Vec2& itemSize, const ButtonStyle& buttonStyle);
    [[nodiscard]] bool Button(const std::string& name, const Rect& bounds);
    [[nodiscard]] bool Button(const std::string& name, const Rect& bounds, const ButtonStyle& buttonStyle);
    [[nodiscard]] const RenderCommandList& EndFrame();

    void SetStyle(const Style& style);
    [[nodiscard]] const Style& GetStyle() const noexcept;
    [[nodiscard]] const Layout& GetLayout() const noexcept;

private:
    struct LayoutFrame
    {
        LayoutDirection direction{LayoutDirection::Column};
        Rect contentBounds{};
        Vec2 cursor{};
        float gap{0.0f};
        Vec2 itemSize{};
    };

    void BeginLayoutContainer(const LayoutContainer& container, LayoutDirection direction);
    void EndLayoutContainer(LayoutDirection direction);
    [[nodiscard]] Rect GetNextLayoutBounds(const Vec2& requestedItemSize);
    [[nodiscard]] Rect GetContentBounds(const LayoutContainer& container) const noexcept;
    [[nodiscard]] Vec2 ResolveItemSize(const LayoutFrame& frame, const Vec2& requestedItemSize) const noexcept;
    void AdvanceLayoutCursor(LayoutFrame& frame, const Vec2& itemSize) noexcept;
    [[nodiscard]] bool IsButtonActive(const std::string& name) const;
    [[nodiscard]] Color GetButtonColor(const std::string& name, const Rect& bounds,
                                       const ButtonStyle& buttonStyle) const;

    Style _style{};
    Layout _layout{};
    InputState _inputState{};
    std::string _activeButtonName{};
    std::vector<LayoutFrame> _layoutStack{};
    RenderCommandList _renderCommands{};
};

} // namespace greenfield

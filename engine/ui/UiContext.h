#pragma once

#include <string>

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
    float cornerRadius{8.0f};
};

class UiContext
{
public:
    void BeginFrame(const Layout& layout);
    void BeginFrame(const Layout& layout, const InputState& inputState);
    void DrawFilledRectangle(const Rect& rectangle, const Color& color, float cornerRadius = 0.0f);
    [[nodiscard]] bool Button(const std::string& name, const Rect& bounds);
    [[nodiscard]] bool Button(const std::string& name, const Rect& bounds, const ButtonStyle& buttonStyle);
    [[nodiscard]] const RenderCommandList& EndFrame();

    void SetStyle(const Style& style);
    [[nodiscard]] const Style& GetStyle() const noexcept;
    [[nodiscard]] const Layout& GetLayout() const noexcept;

private:
    [[nodiscard]] bool IsButtonActive(const std::string& name) const;
    [[nodiscard]] Color GetButtonColor(
        const std::string& name,
        const Rect& bounds,
        const ButtonStyle& buttonStyle) const;

    Style _style{};
    Layout _layout{};
    InputState _inputState{};
    std::string _activeButtonName{};
    RenderCommandList _renderCommands{};
};

} // namespace greenfield

#include <cstdlib>

#include "engine/core/Color.h"
#include "engine/core/Rect.h"
#include "engine/core/Vec2.h"
#include "engine/input/InputState.h"
#include "engine/render/RenderCommandList.h"
#include "engine/ui/Layout.h"
#include "engine/ui/Style.h"
#include "engine/ui/UiContext.h"

namespace
{

[[nodiscard]] bool ColorsMatch(const greenfield::Color& firstColor, const greenfield::Color& secondColor)
{
    return firstColor.red == secondColor.red && firstColor.green == secondColor.green &&
        firstColor.blue == secondColor.blue && firstColor.alpha == secondColor.alpha;
}

} // namespace

int main()
{
    using namespace greenfield;

    const Vec2 size{640.0f, 360.0f};
    const Rect bounds{
        .position = Vec2{0.0f, 0.0f},
        .size = size,
    };

    RenderCommandList renderCommands;
    renderCommands.AddFillRectangle(bounds, Color{0.25f, 0.5f, 0.75f, 1.0f}, 6.0f);

    if (renderCommands.Size() != 1U)
    {
        return EXIT_FAILURE;
    }

    if (renderCommands.Commands()[0].cornerRadius != 6.0f)
    {
        return EXIT_FAILURE;
    }

    const Layout layout{
        .bounds = bounds,
        .padding = 12.0f,
    };

    const Style style{};
    if (layout.bounds.size.x <= 0.0f || style.accent.alpha <= 0.0f)
    {
        return EXIT_FAILURE;
    }

    const Rect testRectangle{
        .position = Vec2{10.0f, 20.0f},
        .size = Vec2{100.0f, 50.0f},
    };

    if (!ContainsPoint(testRectangle, Vec2{10.0f, 20.0f}))
    {
        return EXIT_FAILURE;
    }

    if (ContainsPoint(testRectangle, Vec2{110.0f, 70.0f}))
    {
        return EXIT_FAILURE;
    }

    UiContext uiContext;
    const ButtonStyle buttonStyle{
        .normal = Color{0.1f, 0.2f, 0.3f, 1.0f},
        .hovered = Color{0.2f, 0.3f, 0.4f, 1.0f},
        .pressed = Color{0.3f, 0.4f, 0.5f, 1.0f},
        .cornerRadius = 4.0f,
    };

    uiContext.BeginFrame(layout, InputState{.mousePosition = Vec2{20.0f, 30.0f}});
    if (uiContext.Button("test-button", testRectangle, buttonStyle))
    {
        return EXIT_FAILURE;
    }

    const auto& hoverCommands = uiContext.EndFrame();
    if (!ColorsMatch(hoverCommands.Commands()[0].color, buttonStyle.hovered))
    {
        return EXIT_FAILURE;
    }

    uiContext.BeginFrame(
        layout,
        InputState{
            .mousePosition = Vec2{20.0f, 30.0f},
            .leftMouseButtonDown = true,
            .leftMouseButtonPressed = true,
        });
    if (uiContext.Button("test-button", testRectangle, buttonStyle))
    {
        return EXIT_FAILURE;
    }

    const auto& pressedCommands = uiContext.EndFrame();
    if (!ColorsMatch(pressedCommands.Commands()[0].color, buttonStyle.pressed))
    {
        return EXIT_FAILURE;
    }

    uiContext.BeginFrame(
        layout,
        InputState{
            .mousePosition = Vec2{20.0f, 30.0f},
            .leftMouseButtonReleased = true,
        });
    if (!uiContext.Button("test-button", testRectangle, buttonStyle))
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

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

[[nodiscard]] bool RectanglesMatch(const greenfield::Rect& firstRectangle, const greenfield::Rect& secondRectangle)
{
    return firstRectangle.position.x == secondRectangle.position.x &&
           firstRectangle.position.y == secondRectangle.position.y && firstRectangle.size.x == secondRectangle.size.x &&
           firstRectangle.size.y == secondRectangle.size.y;
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
    const Color commandFillColor{0.25f, 0.5f, 0.75f, 0.8f};
    const Color commandBorderColor{0.75f, 0.85f, 0.95f, 0.5f};
    renderCommands.AddFillRectangle(bounds, commandFillColor, 6.0f, commandBorderColor, 2.0f);
    renderCommands.AddText("Hello", Rect{.position = Vec2{4.0f, 8.0f}, .size = Vec2{120.0f, 32.0f}}, 18.0f, commandFillColor);

    if (renderCommands.Size() != 2U)
    {
        return EXIT_FAILURE;
    }

    if (renderCommands.Commands()[0].cornerRadius != 6.0f)
    {
        return EXIT_FAILURE;
    }

    if (renderCommands.Commands()[1].type != RenderCommandType::DrawText ||
        renderCommands.Commands()[1].text != "Hello" || renderCommands.Commands()[1].fontSize != 18.0f ||
        !ColorsMatch(renderCommands.Commands()[1].textColor, commandFillColor))
    {
        return EXIT_FAILURE;
    }

    if (!ColorsMatch(renderCommands.Commands()[0].fillColor, commandFillColor) ||
        !ColorsMatch(renderCommands.Commands()[0].borderColor, commandBorderColor) ||
        renderCommands.Commands()[0].borderThickness != 2.0f)
    {
        return EXIT_FAILURE;
    }

    const Layout layout{
        .bounds = bounds,
        .padding = 12.0f,
    };

    const Style style{};
    if (layout.bounds.size.x <= 0.0f || style.accent.alpha <= 0.0f || style.textPrimary.alpha <= 0.0f ||
        style.panelCornerRadius <= 0.0f || style.panelBorderThickness <= 0.0f || style.panelBackground.alpha >= 1.0f)
    {
        return EXIT_FAILURE;
    }

    UiContext layoutContext;
    layoutContext.BeginFrame(layout);
    layoutContext.BeginColumn(LayoutContainer{
        .bounds =
            Rect{
                .position = Vec2{10.0f, 10.0f},
                .size = Vec2{200.0f, 160.0f},
            },
        .padding = 8.0f,
        .gap = 6.0f,
        .itemSize = Vec2{50.0f, 20.0f},
    });
    layoutContext.Panel(style.panelBackground);
    layoutContext.Panel(style.panelBackground);
    layoutContext.EndColumn();

    const auto& columnCommands = layoutContext.EndFrame();
    if (columnCommands.Size() != 2U)
    {
        return EXIT_FAILURE;
    }

    if (!RectanglesMatch(columnCommands.Commands()[0].rectangle,
                         Rect{
                             .position = Vec2{18.0f, 18.0f},
                             .size = Vec2{50.0f, 20.0f},
                         }))
    {
        return EXIT_FAILURE;
    }

    if (!RectanglesMatch(columnCommands.Commands()[1].rectangle,
                         Rect{
                             .position = Vec2{18.0f, 44.0f},
                             .size = Vec2{50.0f, 20.0f},
                         }))
    {
        return EXIT_FAILURE;
    }

    layoutContext.BeginFrame(layout);
    layoutContext.BeginRow(LayoutContainer{
        .bounds =
            Rect{
                .position = Vec2{0.0f, 0.0f},
                .size = Vec2{200.0f, 80.0f},
            },
        .padding = 5.0f,
        .gap = 7.0f,
        .itemSize = Vec2{30.0f, 40.0f},
    });
    layoutContext.Panel(style.panelBackground);
    layoutContext.Panel(style.panelBackground);
    layoutContext.EndRow();

    const auto& rowCommands = layoutContext.EndFrame();
    if (rowCommands.Size() != 2U)
    {
        return EXIT_FAILURE;
    }

    if (!RectanglesMatch(rowCommands.Commands()[0].rectangle,
                         Rect{
                             .position = Vec2{5.0f, 5.0f},
                             .size = Vec2{30.0f, 40.0f},
                         }))
    {
        return EXIT_FAILURE;
    }

    if (!RectanglesMatch(rowCommands.Commands()[1].rectangle,
                         Rect{
                             .position = Vec2{42.0f, 5.0f},
                             .size = Vec2{30.0f, 40.0f},
                         }))
    {
        return EXIT_FAILURE;
    }

    UiContext textLayoutContext;
    textLayoutContext.BeginFrame(layout);
    textLayoutContext.BeginColumn(LayoutContainer{
        .bounds =
            Rect{
                .position = Vec2{4.0f, 6.0f},
                .size = Vec2{260.0f, 120.0f},
            },
        .padding = 10.0f,
        .gap = 8.0f,
        .itemSize = Vec2{140.0f, 26.0f},
    });
    textLayoutContext.Text("Layout text", Vec2{140.0f, 26.0f}, 18.0f, style.textPrimary);
    const auto& textCommands = textLayoutContext.EndFrame();
    if (textCommands.Size() != 1U || textCommands.Commands()[0].type != RenderCommandType::DrawText)
    {
        return EXIT_FAILURE;
    }

    if (!RectanglesMatch(textCommands.Commands()[0].rectangle,
                         Rect{
                             .position = Vec2{14.0f, 16.0f},
                             .size = Vec2{140.0f, 26.0f},
                         }))
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
    if (!ColorsMatch(hoverCommands.Commands()[0].fillColor, buttonStyle.hovered) ||
        !ColorsMatch(hoverCommands.Commands()[0].borderColor, buttonStyle.border) ||
        hoverCommands.Commands()[0].borderThickness != buttonStyle.borderThickness)
    {
        return EXIT_FAILURE;
    }

    uiContext.BeginFrame(layout,
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
    if (!ColorsMatch(pressedCommands.Commands()[0].fillColor, buttonStyle.pressed))
    {
        return EXIT_FAILURE;
    }

    uiContext.BeginFrame(layout,
                         InputState{
                             .mousePosition = Vec2{20.0f, 30.0f},
                             .leftMouseButtonReleased = true,
                         });
    if (!uiContext.Button("test-button", testRectangle, buttonStyle))
    {
        return EXIT_FAILURE;
    }

    UiContext layoutButtonContext;
    layoutButtonContext.BeginFrame(layout,
                                   InputState{
                                       .mousePosition = Vec2{20.0f, 20.0f},
                                       .leftMouseButtonDown = true,
                                       .leftMouseButtonPressed = true,
                                   });
    layoutButtonContext.BeginColumn(LayoutContainer{
        .bounds =
            Rect{
                .position = Vec2{0.0f, 0.0f},
                .size = Vec2{200.0f, 100.0f},
            },
        .padding = 10.0f,
        .itemSize = Vec2{80.0f, 40.0f},
    });
    if (layoutButtonContext.Button("layout-button", buttonStyle))
    {
        return EXIT_FAILURE;
    }

    const auto& layoutPressedCommands = layoutButtonContext.EndFrame();
    if (!RectanglesMatch(layoutPressedCommands.Commands()[0].rectangle,
                         Rect{
                             .position = Vec2{10.0f, 10.0f},
                             .size = Vec2{80.0f, 40.0f},
                         }))
    {
        return EXIT_FAILURE;
    }

    if (!ColorsMatch(layoutPressedCommands.Commands()[0].fillColor, buttonStyle.pressed))
    {
        return EXIT_FAILURE;
    }

    layoutButtonContext.BeginFrame(layout,
                                   InputState{
                                       .mousePosition = Vec2{20.0f, 20.0f},
                                       .leftMouseButtonReleased = true,
                                   });
    layoutButtonContext.BeginColumn(LayoutContainer{
        .bounds =
            Rect{
                .position = Vec2{0.0f, 0.0f},
                .size = Vec2{200.0f, 100.0f},
            },
        .padding = 10.0f,
        .itemSize = Vec2{80.0f, 40.0f},
    });
    if (!layoutButtonContext.Button("layout-button", buttonStyle))
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

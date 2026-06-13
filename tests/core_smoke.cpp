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

    RenderCommandList clipCommands;
    const Rect clipBounds{
        .position = Vec2{12.0f, 16.0f},
        .size = Vec2{80.0f, 90.0f},
    };
    clipCommands.PushClip(clipBounds);
    clipCommands.PopClip();
    if (clipCommands.Size() != 2U || clipCommands.Commands()[0].type != RenderCommandType::PushClip ||
        clipCommands.Commands()[1].type != RenderCommandType::PopClip ||
        !RectanglesMatch(clipCommands.Commands()[0].rectangle, clipBounds))
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

    const Rect responsiveBounds{
        .position = Vec2{10.0f, 20.0f},
        .size = Vec2{300.0f, 180.0f},
    };

    if (!RectanglesMatch(InsetRectangle(responsiveBounds, 10.0f),
                         Rect{
                             .position = Vec2{20.0f, 30.0f},
                             .size = Vec2{280.0f, 160.0f},
                         }))
    {
        return EXIT_FAILURE;
    }

    const Rect intersectedRectangle =
        IntersectRectangles(responsiveBounds, Rect{.position = Vec2{250.0f, 100.0f}, .size = Vec2{120.0f, 140.0f}});
    if (!RectanglesMatch(intersectedRectangle,
                         Rect{
                             .position = Vec2{250.0f, 100.0f},
                             .size = Vec2{60.0f, 100.0f},
                         }))
    {
        return EXIT_FAILURE;
    }

    const Rect emptyIntersection =
        IntersectRectangles(responsiveBounds, Rect{.position = Vec2{400.0f, 20.0f}, .size = Vec2{20.0f, 20.0f}});
    if (!RectanglesMatch(emptyIntersection,
                         Rect{
                             .position = Vec2{400.0f, 20.0f},
                             .size = Vec2{0.0f, 20.0f},
                         }))
    {
        return EXIT_FAILURE;
    }

    if (!RectanglesMatch(InsetRectangle(responsiveBounds,
                                        EdgeInsets{.left = 8.0f, .top = 12.0f, .right = 18.0f, .bottom = 22.0f}),
                         Rect{
                             .position = Vec2{18.0f, 32.0f},
                             .size = Vec2{274.0f, 146.0f},
                         }))
    {
        return EXIT_FAILURE;
    }

    const LayoutSplit percentageSplit = SplitRectangleHorizontallyByPercentage(responsiveBounds, 0.25f, 12.0f);
    if (!RectanglesMatch(percentageSplit.first,
                         Rect{
                             .position = Vec2{10.0f, 20.0f},
                             .size = Vec2{72.0f, 180.0f},
                         }) ||
        !RectanglesMatch(percentageSplit.second,
                         Rect{
                             .position = Vec2{94.0f, 20.0f},
                             .size = Vec2{216.0f, 180.0f},
                         }))
    {
        return EXIT_FAILURE;
    }

    const Vec2 clampedSize = ClampSize(Vec2{80.0f, 260.0f}, Vec2{100.0f, 120.0f}, Vec2{240.0f, 220.0f});
    if (clampedSize.x != 100.0f || clampedSize.y != 220.0f)
    {
        return EXIT_FAILURE;
    }

    const LayoutSplit fixedFlexibleSplit =
        SplitRectangleHorizontallyFixedFlexible(responsiveBounds, 96.0f, 14.0f, FixedRegion::Second);
    if (!RectanglesMatch(fixedFlexibleSplit.first,
                         Rect{
                             .position = Vec2{10.0f, 20.0f},
                             .size = Vec2{190.0f, 180.0f},
                         }) ||
        !RectanglesMatch(fixedFlexibleSplit.second,
                         Rect{
                             .position = Vec2{214.0f, 20.0f},
                             .size = Vec2{96.0f, 180.0f},
                         }))
    {
        return EXIT_FAILURE;
    }

    if (!RectanglesMatch(CalculateColumnRegion(responsiveBounds, 1U, 3U, 15.0f),
                         Rect{
                             .position = Vec2{115.0f, 20.0f},
                             .size = Vec2{90.0f, 180.0f},
                         }))
    {
        return EXIT_FAILURE;
    }

    if (!RectanglesMatch(CalculateRowRegion(responsiveBounds, 2U, 3U, 15.0f),
                         Rect{
                             .position = Vec2{10.0f, 150.0f},
                             .size = Vec2{300.0f, 50.0f},
                         }))
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

    UiContext scrollContext;
    const Rect scrollPanelBounds{
        .position = Vec2{20.0f, 30.0f},
        .size = Vec2{120.0f, 80.0f},
    };
    scrollContext.BeginFrame(layout,
                             InputState{
                                 .mousePosition = Vec2{40.0f, 50.0f},
                                 .verticalScrollDelta = -10.0f,
                             });
    const Rect scrolledContent = scrollContext.BeginVerticalScrollPanel("scroll-test", scrollPanelBounds, 200.0f);
    scrollContext.EndVerticalScrollPanel();
    const auto& scrollCommands = scrollContext.EndFrame();
    if (scrollContext.GetVerticalScrollOffset("scroll-test") != 120.0f ||
        scrolledContent.position.y != -90.0f || scrollCommands.Size() != 2U ||
        scrollCommands.Commands()[0].type != RenderCommandType::PushClip ||
        scrollCommands.Commands()[1].type != RenderCommandType::PopClip)
    {
        return EXIT_FAILURE;
    }

    scrollContext.BeginFrame(layout,
                             InputState{
                                 .mousePosition = Vec2{40.0f, 50.0f},
                                 .verticalScrollDelta = 10.0f,
                             });
    const Rect resetContent = scrollContext.BeginVerticalScrollPanel("scroll-test", scrollPanelBounds, 200.0f);
    scrollContext.EndVerticalScrollPanel();
    const auto& resetScrollCommands = scrollContext.EndFrame();
    if (scrollContext.GetVerticalScrollOffset("scroll-test") != 0.0f || resetContent.position.y != 30.0f ||
        resetScrollCommands.Size() != 2U)
    {
        return EXIT_FAILURE;
    }

    scrollContext.BeginFrame(layout,
                             InputState{
                                 .mousePosition = Vec2{200.0f, 200.0f},
                                 .verticalScrollDelta = -10.0f,
                             });
    const Rect unchangedContent = scrollContext.BeginVerticalScrollPanel("scroll-test", scrollPanelBounds, 200.0f);
    scrollContext.EndVerticalScrollPanel();
    const auto& unchangedScrollCommands = scrollContext.EndFrame();
    if (scrollContext.GetVerticalScrollOffset("scroll-test") != 0.0f || unchangedContent.position.y != 30.0f ||
        unchangedScrollCommands.Size() != 2U)
    {
        return EXIT_FAILURE;
    }

    const InputState defaultInputState{};
    if (defaultInputState.verticalScrollDelta != 0.0f)
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

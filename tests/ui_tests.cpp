#include <cstdlib>

#include "engine/input/InputState.h"
#include "engine/ui/Style.h"
#include "engine/ui/UiContext.h"
#include "tests/TestHelpers.h"

namespace
{

[[nodiscard]] greenfield::Layout MakeLayout()
{
    return greenfield::Layout{
        .bounds =
            greenfield::Rect{
                .position = greenfield::Vec2{0.0f, 0.0f},
                .size = greenfield::Vec2{640.0f, 360.0f},
            },
        .padding = 12.0f,
    };
}

[[nodiscard]] bool TestColumnAndRowLayoutCommands()
{
    using namespace greenfield;
    using greenfield::tests::RectanglesMatch;

    const Style style{};
    UiContext columnContext;
    columnContext.BeginFrame(MakeLayout());
    columnContext.BeginColumn(LayoutContainer{
        .bounds = Rect{.position = Vec2{10.0f, 10.0f}, .size = Vec2{200.0f, 160.0f}},
        .padding = 8.0f,
        .gap = 6.0f,
        .itemSize = Vec2{50.0f, 20.0f},
    });
    columnContext.Panel(style.panelBackground);
    columnContext.Panel(style.panelBackground);
    columnContext.EndColumn();

    const auto& columnCommands = columnContext.EndFrame();
    if (columnCommands.Size() != 2U ||
        !RectanglesMatch(columnCommands.Commands()[0].rectangle, Rect{.position = Vec2{18.0f, 18.0f}, .size = Vec2{50.0f, 20.0f}}) ||
        !RectanglesMatch(columnCommands.Commands()[1].rectangle, Rect{.position = Vec2{18.0f, 44.0f}, .size = Vec2{50.0f, 20.0f}}))
    {
        return false;
    }

    UiContext rowContext;
    rowContext.BeginFrame(MakeLayout());
    rowContext.BeginRow(LayoutContainer{
        .bounds = Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{200.0f, 80.0f}},
        .padding = 5.0f,
        .gap = 7.0f,
        .itemSize = Vec2{30.0f, 40.0f},
    });
    rowContext.Panel(style.panelBackground);
    rowContext.Panel(style.panelBackground);
    rowContext.EndRow();

    const auto& rowCommands = rowContext.EndFrame();
    return rowCommands.Size() == 2U &&
           RectanglesMatch(rowCommands.Commands()[0].rectangle, Rect{.position = Vec2{5.0f, 5.0f}, .size = Vec2{30.0f, 40.0f}}) &&
           RectanglesMatch(rowCommands.Commands()[1].rectangle, Rect{.position = Vec2{42.0f, 5.0f}, .size = Vec2{30.0f, 40.0f}});
}

[[nodiscard]] bool TestTextEmitsRendererAgnosticCommand()
{
    using namespace greenfield;
    using greenfield::tests::RectanglesMatch;

    const Style style{};
    UiContext textContext;
    textContext.BeginFrame(MakeLayout());
    textContext.BeginColumn(LayoutContainer{
        .bounds = Rect{.position = Vec2{4.0f, 6.0f}, .size = Vec2{260.0f, 120.0f}},
        .padding = 10.0f,
        .gap = 8.0f,
        .itemSize = Vec2{140.0f, 26.0f},
    });
    textContext.Text("Layout text", Vec2{140.0f, 26.0f}, 18.0f, style.textPrimary);

    const auto& textCommands = textContext.EndFrame();
    return textCommands.Size() == 1U && textCommands.Commands()[0].type == RenderCommandType::DrawText &&
           RectanglesMatch(textCommands.Commands()[0].rectangle, Rect{.position = Vec2{14.0f, 16.0f}, .size = Vec2{140.0f, 26.0f}});
}

[[nodiscard]] bool TestScrollPanelClampsOffsetAndRecordsClipCommands()
{
    using namespace greenfield;

    UiContext scrollContext;
    const Rect scrollPanelBounds{
        .position = Vec2{20.0f, 30.0f},
        .size = Vec2{120.0f, 80.0f},
    };

    scrollContext.BeginFrame(MakeLayout(), InputState{.mousePosition = Vec2{40.0f, 50.0f}, .verticalScrollDelta = -10.0f});
    const Rect scrolledContent = scrollContext.BeginVerticalScrollPanel("scroll-test", scrollPanelBounds, 200.0f);
    scrollContext.EndVerticalScrollPanel();
    const auto& scrollCommands = scrollContext.EndFrame();
    if (scrollContext.GetVerticalScrollOffset("scroll-test") != 120.0f || scrolledContent.position.y != -90.0f ||
        scrollCommands.Size() != 2U || scrollCommands.Commands()[0].type != RenderCommandType::PushClip ||
        scrollCommands.Commands()[1].type != RenderCommandType::PopClip)
    {
        return false;
    }

    scrollContext.BeginFrame(MakeLayout(), InputState{.mousePosition = Vec2{40.0f, 50.0f}, .verticalScrollDelta = 10.0f});
    const Rect resetContent = scrollContext.BeginVerticalScrollPanel("scroll-test", scrollPanelBounds, 200.0f);
    scrollContext.EndVerticalScrollPanel();
    const auto& resetScrollCommands = scrollContext.EndFrame();
    if (scrollContext.GetVerticalScrollOffset("scroll-test") != 0.0f || resetContent.position.y != 30.0f || resetScrollCommands.Size() != 2U)
    {
        return false;
    }

    scrollContext.BeginFrame(MakeLayout(), InputState{.mousePosition = Vec2{200.0f, 200.0f}, .verticalScrollDelta = -10.0f});
    const Rect unchangedContent = scrollContext.BeginVerticalScrollPanel("scroll-test", scrollPanelBounds, 200.0f);
    scrollContext.EndVerticalScrollPanel();
    const auto& unchangedScrollCommands = scrollContext.EndFrame();
    return scrollContext.GetVerticalScrollOffset("scroll-test") == 0.0f && unchangedContent.position.y == 30.0f && unchangedScrollCommands.Size() == 2U;
}

[[nodiscard]] bool TestButtonHitAndClickBehavior()
{
    using namespace greenfield;
    using greenfield::tests::ColorsMatch;

    const Rect buttonBounds{
        .position = Vec2{10.0f, 20.0f},
        .size = Vec2{100.0f, 50.0f},
    };
    const ButtonStyle buttonStyle{
        .normal = Color{0.1f, 0.2f, 0.3f, 1.0f},
        .hovered = Color{0.2f, 0.3f, 0.4f, 1.0f},
        .pressed = Color{0.3f, 0.4f, 0.5f, 1.0f},
        .cornerRadius = 4.0f,
    };

    UiContext uiContext;
    uiContext.BeginFrame(MakeLayout(), InputState{.mousePosition = Vec2{20.0f, 30.0f}});
    if (uiContext.Button("test-button", buttonBounds, buttonStyle))
    {
        return false;
    }

    const auto& hoverCommands = uiContext.EndFrame();
    if (!ColorsMatch(hoverCommands.Commands()[0].fillColor, buttonStyle.hovered) ||
        !ColorsMatch(hoverCommands.Commands()[0].borderColor, buttonStyle.border) ||
        hoverCommands.Commands()[0].borderThickness != buttonStyle.borderThickness)
    {
        return false;
    }

    uiContext.BeginFrame(MakeLayout(), InputState{.mousePosition = Vec2{20.0f, 30.0f}, .leftMouseButtonDown = true, .leftMouseButtonPressed = true});
    if (uiContext.Button("test-button", buttonBounds, buttonStyle))
    {
        return false;
    }

    const auto& pressedCommands = uiContext.EndFrame();
    if (!ColorsMatch(pressedCommands.Commands()[0].fillColor, buttonStyle.pressed))
    {
        return false;
    }

    uiContext.BeginFrame(MakeLayout(), InputState{.mousePosition = Vec2{20.0f, 30.0f}, .leftMouseButtonReleased = true});
    return uiContext.Button("test-button", buttonBounds, buttonStyle);
}

[[nodiscard]] bool TestLayoutGeneratedButtonHitRegion()
{
    using namespace greenfield;
    using greenfield::tests::ColorsMatch;
    using greenfield::tests::RectanglesMatch;

    const ButtonStyle buttonStyle{
        .pressed = Color{0.3f, 0.4f, 0.5f, 1.0f},
        .cornerRadius = 4.0f,
    };

    UiContext buttonContext;
    buttonContext.BeginFrame(MakeLayout(), InputState{.mousePosition = Vec2{20.0f, 20.0f}, .leftMouseButtonDown = true, .leftMouseButtonPressed = true});
    buttonContext.BeginColumn(LayoutContainer{
        .bounds = Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{200.0f, 100.0f}},
        .padding = 10.0f,
        .itemSize = Vec2{80.0f, 40.0f},
    });
    if (buttonContext.Button("layout-button", buttonStyle))
    {
        return false;
    }

    const auto& pressedCommands = buttonContext.EndFrame();
    if (!RectanglesMatch(pressedCommands.Commands()[0].rectangle, Rect{.position = Vec2{10.0f, 10.0f}, .size = Vec2{80.0f, 40.0f}}) ||
        !ColorsMatch(pressedCommands.Commands()[0].fillColor, buttonStyle.pressed))
    {
        return false;
    }

    buttonContext.BeginFrame(MakeLayout(), InputState{.mousePosition = Vec2{20.0f, 20.0f}, .leftMouseButtonReleased = true});
    buttonContext.BeginColumn(LayoutContainer{
        .bounds = Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{200.0f, 100.0f}},
        .padding = 10.0f,
        .itemSize = Vec2{80.0f, 40.0f},
    });

    return buttonContext.Button("layout-button", buttonStyle);
}

} // namespace

int main()
{
    if (!TestColumnAndRowLayoutCommands() || !TestTextEmitsRendererAgnosticCommand() ||
        !TestScrollPanelClampsOffsetAndRecordsClipCommands() || !TestButtonHitAndClickBehavior() ||
        !TestLayoutGeneratedButtonHitRegion())
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

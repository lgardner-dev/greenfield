#include <cstdlib>
#include <unordered_map>

#include "engine/input/InputRouting.h"
#include "engine/input/InputState.h"
#include "engine/ui/Style.h"
#include "engine/ui/UiContext.h"
#include "engine/ui/UiId.h"
#include "tests/TestHelpers.h"

namespace greenfield
{

struct UiContextTestAccess
{
    [[nodiscard]] static bool GetBooleanState(const UiContext& uiContext, const UiId& controlId)
    {
        return uiContext.GetBooleanState(controlId);
    }

    static void SetBooleanState(UiContext& uiContext, const UiId& controlId, bool value)
    {
        uiContext.SetBooleanState(controlId, value);
    }

    static void ToggleBooleanState(UiContext& uiContext, const UiId& controlId)
    {
        uiContext.ToggleBooleanState(controlId);
    }
};

} // namespace greenfield

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

[[nodiscard]] bool TestUiSurfaceCarriesIdentityAndBounds()
{
    using namespace greenfield;
    using greenfield::tests::RectanglesMatch;

    const Rect bounds{
        .position = Vec2{8.0f, 12.0f},
        .size = Vec2{320.0f, 180.0f},
    };
    const UiSurface uiSurface{SurfaceId{.value = 77U}, bounds};

    return uiSurface.GetId().value == 77U && IsValidSurface(uiSurface.GetSurface()) &&
           RectanglesMatch(uiSurface.GetBounds(), bounds);
}

[[nodiscard]] bool TestRootSurfaceMatchesImmediateFrameBounds()
{
    using namespace greenfield;
    using greenfield::tests::RectanglesMatch;

    const Layout layout{
        .bounds = Rect{.position = Vec2{10.0f, 20.0f}, .size = Vec2{320.0f, 180.0f}},
        .padding = 12.0f,
        .itemSize = Vec2{80.0f, 30.0f},
    };

    UiContext uiContext;
    uiContext.BeginFrame(layout);
    const UiSurface rootSurface = uiContext.GetRootSurface();
    uiContext.Panel(Color{0.2f, 0.3f, 0.4f, 1.0f});

    const auto& commands = uiContext.EndFrame();
    return rootSurface.GetId().value == RootUiSurfaceId.value &&
           RectanglesMatch(rootSurface.GetBounds(), layout.bounds) && commands.Size() == 1U &&
           RectanglesMatch(commands.Commands()[0].rectangle,
                           Rect{.position = Vec2{22.0f, 32.0f}, .size = Vec2{80.0f, 30.0f}});
}

[[nodiscard]] bool TestInputPointRoutesToRootUiSurface()
{
    using namespace greenfield;

    UiContext uiContext;
    uiContext.BeginFrame(MakeLayout());

    const UiSurface rootSurface = uiContext.GetRootSurface();
    const InteractionNode rootNode = MakeInteractionNode(rootSurface.GetSurface());
    const HitTestResult hitTestResult = RouteInputPoint(rootNode, Vec2{24.0f, 28.0f});

    return hitTestResult.hit && hitTestResult.surfaceId.value == RootUiSurfaceId.value;
}

[[nodiscard]] bool TestInputPointOutsideRootUiSurfaceDoesNotRoute()
{
    using namespace greenfield;

    UiContext uiContext;
    uiContext.BeginFrame(MakeLayout());

    const UiSurface rootSurface = uiContext.GetRootSurface();
    const InteractionNode rootNode = MakeInteractionNode(rootSurface.GetSurface());
    const HitTestResult hitTestResult = RouteInputPoint(rootNode, Vec2{700.0f, 400.0f});

    return !hitTestResult.hit && !IsValidSurfaceId(hitTestResult.surfaceId);
}

[[nodiscard]] bool TestUiIdUsesExactStringIdentity()
{
    using namespace greenfield;

    const UiId firstButtonId = MakeUiId("button");
    const UiId matchingButtonId = MakeUiId("button");
    const UiId differentButtonId = MakeUiId("button-2");

    return firstButtonId == matchingButtonId && !(firstButtonId == differentButtonId);
}

[[nodiscard]] bool TestUiIdCanKeyStateMaps()
{
    using namespace greenfield;

    std::unordered_map<UiId, float, UiIdHash> offsets{};
    offsets[MakeUiId("panel")] = 42.0f;
    offsets[MakeUiId("panel-2")] = 7.0f;

    return offsets[MakeUiId("panel")] == 42.0f && offsets[MakeUiId("panel-2")] == 7.0f && offsets.size() == 2U;
}

[[nodiscard]] bool TestRootSurfaceAccessDoesNotChangeCommandEmission()
{
    using namespace greenfield;
    using greenfield::tests::ColorsMatch;
    using greenfield::tests::RectanglesMatch;

    const Layout layout = MakeLayout();
    const Color panelColor{0.2f, 0.3f, 0.4f, 1.0f};

    UiContext unchangedContext;
    unchangedContext.BeginFrame(layout);
    unchangedContext.Panel(panelColor, 4.0f);
    const auto& unchangedCommands = unchangedContext.EndFrame();

    UiContext surfaceAwareContext;
    surfaceAwareContext.BeginFrame(layout);
    const UiSurface rootSurface = surfaceAwareContext.GetRootSurface();
    surfaceAwareContext.Panel(panelColor, 4.0f);
    const auto& surfaceAwareCommands = surfaceAwareContext.EndFrame();

    return rootSurface.GetId().value == RootUiSurfaceId.value && unchangedCommands.Size() == 1U &&
           surfaceAwareCommands.Size() == 1U &&
           unchangedCommands.Commands()[0].type == surfaceAwareCommands.Commands()[0].type &&
           RectanglesMatch(unchangedCommands.Commands()[0].rectangle, surfaceAwareCommands.Commands()[0].rectangle) &&
           ColorsMatch(unchangedCommands.Commands()[0].fillColor, surfaceAwareCommands.Commands()[0].fillColor) &&
           unchangedCommands.Commands()[0].cornerRadius == surfaceAwareCommands.Commands()[0].cornerRadius;
}

[[nodiscard]] bool TestBeginFrameClearsPreviousFrameCommands()
{
    using namespace greenfield;

    UiContext uiContext;
    uiContext.BeginFrame(MakeLayout());
    uiContext.Panel(Color{0.2f, 0.3f, 0.4f, 1.0f});

    const auto& previousCommands = uiContext.EndFrame();
    if (previousCommands.Size() != 1U)
    {
        return false;
    }

    uiContext.BeginFrame(MakeLayout());
    const auto& currentCommands = uiContext.EndFrame();
    return currentCommands.IsEmpty();
}

[[nodiscard]] bool TestBeginFrameResetsLayoutState()
{
    using namespace greenfield;
    using greenfield::tests::RectanglesMatch;

    UiContext uiContext;
    uiContext.BeginFrame(MakeLayout());
    uiContext.BeginColumn(LayoutContainer{
        .bounds = Rect{.position = Vec2{40.0f, 50.0f}, .size = Vec2{200.0f, 100.0f}},
        .padding = 10.0f,
        .itemSize = Vec2{80.0f, 24.0f},
    });
    uiContext.Panel(Color{0.2f, 0.3f, 0.4f, 1.0f});
    const auto& nestedCommands = uiContext.EndFrame();
    if (nestedCommands.Size() != 1U)
    {
        return false;
    }

    uiContext.BeginFrame(MakeLayout());
    uiContext.Panel(Color{0.2f, 0.3f, 0.4f, 1.0f});

    const auto& resetCommands = uiContext.EndFrame();
    return resetCommands.Size() == 1U &&
           RectanglesMatch(resetCommands.Commands()[0].rectangle,
                           Rect{.position = Vec2{12.0f, 12.0f}, .size = Vec2{616.0f, 48.0f}});
}

[[nodiscard]] bool TestEndFrameReturnsRendererNeutralCommands()
{
    using namespace greenfield;
    using greenfield::tests::RectanglesMatch;

    UiContext uiContext;
    uiContext.BeginFrame(MakeLayout());
    uiContext.DrawFilledRectangle(Rect{.position = Vec2{4.0f, 8.0f}, .size = Vec2{24.0f, 32.0f}},
                                  Color{0.2f, 0.3f, 0.4f, 1.0f});

    const RenderCommandList& commands = uiContext.EndFrame();
    return commands.Size() == 1U && commands.Commands()[0].type == RenderCommandType::FillRectangle &&
           RectanglesMatch(commands.Commands()[0].rectangle,
                           Rect{.position = Vec2{4.0f, 8.0f}, .size = Vec2{24.0f, 32.0f}});
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

[[nodiscard]] bool TestScrollOffsetsPersistByPanelIdentity()
{
    using namespace greenfield;

    UiContext uiContext;
    const Rect scrollPanelBounds{
        .position = Vec2{20.0f, 30.0f},
        .size = Vec2{120.0f, 80.0f},
    };

    uiContext.BeginFrame(MakeLayout(), InputState{.mousePosition = Vec2{40.0f, 50.0f}, .verticalScrollDelta = -1.0f});
    const Rect initialContent = uiContext.BeginVerticalScrollPanel("scroll-a", scrollPanelBounds, 200.0f);
    uiContext.EndVerticalScrollPanel();
    const auto& initialCommands = uiContext.EndFrame();
    if (initialContent.position.y != -12.0f || initialCommands.Size() != 2U)
    {
        return false;
    }

    uiContext.BeginFrame(MakeLayout());
    const Rect persistedContent = uiContext.BeginVerticalScrollPanel("scroll-a", scrollPanelBounds, 200.0f);
    uiContext.EndVerticalScrollPanel();
    const auto& persistedCommands = uiContext.EndFrame();
    if (persistedCommands.Size() != 2U)
    {
        return false;
    }

    uiContext.BeginFrame(MakeLayout());
    const Rect separateContent = uiContext.BeginVerticalScrollPanel("scroll-b", scrollPanelBounds, 200.0f);
    uiContext.EndVerticalScrollPanel();
    const auto& separateCommands = uiContext.EndFrame();
    if (separateCommands.Size() != 2U)
    {
        return false;
    }

    return uiContext.GetVerticalScrollOffset("scroll-a") == 42.0f &&
           uiContext.GetVerticalScrollOffset("scroll-b") == 0.0f && persistedContent.position.y == -12.0f &&
           separateContent.position.y == 30.0f;
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

[[nodiscard]] bool TestActiveControlIdentitySurvivesPressReleaseFrames()
{
    using namespace greenfield;

    const Rect firstButtonBounds{
        .position = Vec2{10.0f, 20.0f},
        .size = Vec2{100.0f, 50.0f},
    };
    const Rect secondButtonBounds{
        .position = Vec2{140.0f, 20.0f},
        .size = Vec2{100.0f, 50.0f},
    };

    UiContext uiContext;
    uiContext.BeginFrame(MakeLayout(), InputState{.mousePosition = Vec2{20.0f, 30.0f}, .leftMouseButtonDown = true, .leftMouseButtonPressed = true});
    if (uiContext.Button("first-button", firstButtonBounds) || uiContext.Button("second-button", secondButtonBounds))
    {
        return false;
    }
    const auto& pressCommands = uiContext.EndFrame();
    if (pressCommands.Size() != 4U)
    {
        return false;
    }

    uiContext.BeginFrame(MakeLayout(), InputState{.mousePosition = Vec2{150.0f, 30.0f}, .leftMouseButtonReleased = true});
    if (uiContext.Button("first-button", firstButtonBounds) || uiContext.Button("second-button", secondButtonBounds))
    {
        return false;
    }
    const auto& mismatchedReleaseCommands = uiContext.EndFrame();
    if (mismatchedReleaseCommands.Size() != 4U)
    {
        return false;
    }

    uiContext.BeginFrame(MakeLayout(), InputState{.mousePosition = Vec2{150.0f, 30.0f}, .leftMouseButtonDown = true, .leftMouseButtonPressed = true});
    if (uiContext.Button("first-button", firstButtonBounds) || uiContext.Button("second-button", secondButtonBounds))
    {
        return false;
    }
    const auto& secondPressCommands = uiContext.EndFrame();
    if (secondPressCommands.Size() != 4U)
    {
        return false;
    }

    uiContext.BeginFrame(MakeLayout(), InputState{.mousePosition = Vec2{150.0f, 30.0f}, .leftMouseButtonReleased = true});
    return !uiContext.Button("first-button", firstButtonBounds) && uiContext.Button("second-button", secondButtonBounds);
}

[[nodiscard]] bool TestOverlappingButtonPressIsConsumedByActiveControl()
{
    using namespace greenfield;

    const Rect overlappingButtonBounds{
        .position = Vec2{10.0f, 20.0f},
        .size = Vec2{100.0f, 50.0f},
    };

    UiContext uiContext;
    uiContext.BeginFrame(MakeLayout(), InputState{.mousePosition = Vec2{20.0f, 30.0f}, .leftMouseButtonDown = true, .leftMouseButtonPressed = true});
    if (uiContext.Button("first-button", overlappingButtonBounds) ||
        uiContext.Button("second-button", overlappingButtonBounds))
    {
        return false;
    }
    const auto& pressCommands = uiContext.EndFrame();
    if (pressCommands.Size() != 4U)
    {
        return false;
    }

    uiContext.BeginFrame(MakeLayout(), InputState{.mousePosition = Vec2{20.0f, 30.0f}, .leftMouseButtonReleased = true});
    const bool firstButtonClicked = uiContext.Button("first-button", overlappingButtonBounds);
    const bool secondButtonClicked = uiContext.Button("second-button", overlappingButtonBounds);
    const auto& releaseCommands = uiContext.EndFrame();

    return firstButtonClicked && !secondButtonClicked && releaseCommands.Size() == 4U;
}

[[nodiscard]] bool TestFocusDefaultsToEmpty()
{
    using namespace greenfield;

    const UiContext uiContext;
    return !uiContext.FocusedControlId().has_value() && !uiContext.HasFocus("missing-control");
}

[[nodiscard]] bool TestRequestFocusByIdAndName()
{
    using namespace greenfield;

    UiContext uiContext;
    const UiId firstControlId = MakeUiId("first-control");

    uiContext.RequestFocus(firstControlId);
    if (!uiContext.HasFocus(firstControlId) || !uiContext.HasFocus("first-control") ||
        !uiContext.FocusedControlId().has_value() || uiContext.FocusedControlId().value() != firstControlId)
    {
        return false;
    }

    uiContext.RequestFocus("second-control");
    return !uiContext.HasFocus(firstControlId) && uiContext.HasFocus("second-control") &&
           uiContext.FocusedControlId().has_value() &&
           uiContext.FocusedControlId().value() == MakeUiId("second-control");
}

[[nodiscard]] bool TestClearFocusRemovesFocus()
{
    using namespace greenfield;

    UiContext uiContext;
    uiContext.RequestFocus("focused-control");
    uiContext.ClearFocus();

    return !uiContext.FocusedControlId().has_value() && !uiContext.HasFocus("focused-control");
}

[[nodiscard]] bool TestFocusPersistsAcrossFrames()
{
    using namespace greenfield;

    UiContext uiContext;
    uiContext.RequestFocus("persistent-control");
    uiContext.BeginFrame(MakeLayout());
    uiContext.Panel(Color{0.2f, 0.3f, 0.4f, 1.0f});
    const auto& firstCommands = uiContext.EndFrame();
    if (firstCommands.Size() != 1U || !uiContext.HasFocus("persistent-control"))
    {
        return false;
    }

    uiContext.BeginFrame(MakeLayout());
    const auto& secondCommands = uiContext.EndFrame();
    return secondCommands.IsEmpty() && uiContext.HasFocus("persistent-control");
}

[[nodiscard]] bool TestBooleanStatePersistsAcrossFrames()
{
    using namespace greenfield;

    UiContext uiContext;
    const UiId controlId = MakeUiId("stateful-control");
    if (UiContextTestAccess::GetBooleanState(uiContext, controlId))
    {
        return false;
    }

    UiContextTestAccess::SetBooleanState(uiContext, controlId, true);
    uiContext.BeginFrame(MakeLayout());
    const auto& firstCommands = uiContext.EndFrame();
    if (!firstCommands.IsEmpty() || !UiContextTestAccess::GetBooleanState(uiContext, controlId))
    {
        return false;
    }

    UiContextTestAccess::ToggleBooleanState(uiContext, controlId);
    uiContext.BeginFrame(MakeLayout());
    const auto& secondCommands = uiContext.EndFrame();
    return secondCommands.IsEmpty() && !UiContextTestAccess::GetBooleanState(uiContext, controlId);
}

[[nodiscard]] bool TestBooleanStateIsIndependentPerUiId()
{
    using namespace greenfield;

    UiContext uiContext;
    const UiId firstControlId = MakeUiId("first-stateful-control");
    const UiId secondControlId = MakeUiId("second-stateful-control");

    UiContextTestAccess::SetBooleanState(uiContext, firstControlId, true);
    UiContextTestAccess::SetBooleanState(uiContext, secondControlId, false);
    uiContext.BeginFrame(MakeLayout());
    const auto& firstCommands = uiContext.EndFrame();
    if (!firstCommands.IsEmpty() || !UiContextTestAccess::GetBooleanState(uiContext, firstControlId) ||
        UiContextTestAccess::GetBooleanState(uiContext, secondControlId))
    {
        return false;
    }

    UiContextTestAccess::ToggleBooleanState(uiContext, secondControlId);
    uiContext.BeginFrame(MakeLayout());
    const auto& secondCommands = uiContext.EndFrame();
    return secondCommands.IsEmpty() && UiContextTestAccess::GetBooleanState(uiContext, firstControlId) &&
           UiContextTestAccess::GetBooleanState(uiContext, secondControlId);
}

[[nodiscard]] bool TestFocusIsIndependentFromActiveControlAndScrollState()
{
    using namespace greenfield;

    UiContext uiContext;
    const Rect buttonBounds{
        .position = Vec2{10.0f, 20.0f},
        .size = Vec2{100.0f, 50.0f},
    };
    const Rect scrollPanelBounds{
        .position = Vec2{140.0f, 20.0f},
        .size = Vec2{120.0f, 80.0f},
    };

    uiContext.RequestFocus("focused-control");
    uiContext.BeginFrame(MakeLayout(), InputState{.mousePosition = Vec2{20.0f, 30.0f}, .leftMouseButtonDown = true, .leftMouseButtonPressed = true});
    if (uiContext.Button("active-button", buttonBounds) || !uiContext.HasFocus("focused-control"))
    {
        return false;
    }
    const auto& buttonPressCommands = uiContext.EndFrame();
    if (buttonPressCommands.Size() != 2U)
    {
        return false;
    }

    uiContext.BeginFrame(MakeLayout(), InputState{.mousePosition = Vec2{20.0f, 30.0f}, .leftMouseButtonReleased = true});
    if (!uiContext.Button("active-button", buttonBounds) || !uiContext.HasFocus("focused-control"))
    {
        return false;
    }
    const auto& buttonReleaseCommands = uiContext.EndFrame();
    if (buttonReleaseCommands.Size() != 2U)
    {
        return false;
    }

    uiContext.BeginFrame(MakeLayout(), InputState{.mousePosition = Vec2{150.0f, 30.0f}, .verticalScrollDelta = -1.0f});
    const Rect scrolledContent = uiContext.BeginVerticalScrollPanel("scroll-panel", scrollPanelBounds, 200.0f);
    uiContext.EndVerticalScrollPanel();
    const auto& scrollCommands = uiContext.EndFrame();

    return uiContext.HasFocus("focused-control") && uiContext.GetVerticalScrollOffset("scroll-panel") == 42.0f &&
           scrolledContent.position.y == -22.0f && scrollCommands.Size() == 2U;
}

} // namespace

int main()
{
    if (!TestColumnAndRowLayoutCommands() || !TestUiSurfaceCarriesIdentityAndBounds() ||
        !TestRootSurfaceMatchesImmediateFrameBounds() || !TestInputPointRoutesToRootUiSurface() ||
        !TestInputPointOutsideRootUiSurfaceDoesNotRoute() || !TestUiIdUsesExactStringIdentity() ||
        !TestUiIdCanKeyStateMaps() || !TestRootSurfaceAccessDoesNotChangeCommandEmission() ||
        !TestBeginFrameClearsPreviousFrameCommands() || !TestBeginFrameResetsLayoutState() ||
        !TestEndFrameReturnsRendererNeutralCommands() || !TestTextEmitsRendererAgnosticCommand() ||
        !TestScrollPanelClampsOffsetAndRecordsClipCommands() || !TestScrollOffsetsPersistByPanelIdentity() ||
        !TestButtonHitAndClickBehavior() || !TestLayoutGeneratedButtonHitRegion() ||
        !TestActiveControlIdentitySurvivesPressReleaseFrames() ||
        !TestOverlappingButtonPressIsConsumedByActiveControl() || !TestFocusDefaultsToEmpty() ||
        !TestRequestFocusByIdAndName() || !TestClearFocusRemovesFocus() ||
        !TestFocusPersistsAcrossFrames() || !TestBooleanStatePersistsAcrossFrames() ||
        !TestBooleanStateIsIndependentPerUiId() || !TestFocusIsIndependentFromActiveControlAndScrollState())
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

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

    [[nodiscard]] static float GetNumericState(const UiContext& uiContext, const UiId& controlId, float defaultValue)
    {
        return uiContext.GetNumericState(controlId, defaultValue);
    }

    static void SetNumericState(UiContext& uiContext, const UiId& controlId, float value)
    {
        uiContext.SetNumericState(controlId, value);
    }

    [[nodiscard]] static std::string GetTextState(const UiContext& uiContext, const UiId& controlId)
    {
        return uiContext.GetTextState(controlId);
    }

    static void SetTextState(UiContext& uiContext, const UiId& controlId, const std::string& value)
    {
        uiContext.SetTextState(controlId, value);
    }

    [[nodiscard]] static float GetClampedNumericState(UiContext& uiContext, const UiId& controlId,
                                                      float defaultValue, float minimum, float maximum)
    {
        return uiContext.GetClampedNumericState(controlId, defaultValue, minimum, maximum);
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

[[nodiscard]] bool TestCheckboxDefaultsUnchecked()
{
    using namespace greenfield;

    const Rect checkboxBounds{
        .position = Vec2{10.0f, 20.0f},
        .size = Vec2{140.0f, 36.0f},
    };

    UiContext uiContext;
    uiContext.BeginFrame(MakeLayout());
    if (uiContext.Checkbox("default-checkbox", checkboxBounds))
    {
        return false;
    }

    const auto& commands = uiContext.EndFrame();
    return commands.Size() == 2U &&
           !UiContextTestAccess::GetBooleanState(uiContext, MakeUiId("default-checkbox"));
}

[[nodiscard]] bool TestCheckboxClickTogglesCheckedState()
{
    using namespace greenfield;

    const Rect checkboxBounds{
        .position = Vec2{10.0f, 20.0f},
        .size = Vec2{140.0f, 36.0f},
    };

    UiContext uiContext;
    uiContext.BeginFrame(MakeLayout(), InputState{.mousePosition = Vec2{20.0f, 30.0f}, .leftMouseButtonDown = true, .leftMouseButtonPressed = true});
    if (uiContext.Checkbox("toggle-checkbox", checkboxBounds))
    {
        return false;
    }
    const auto& pressCommands = uiContext.EndFrame();
    if (pressCommands.Size() != 2U)
    {
        return false;
    }

    uiContext.BeginFrame(MakeLayout(), InputState{.mousePosition = Vec2{20.0f, 30.0f}, .leftMouseButtonReleased = true});
    const bool changed = uiContext.Checkbox("toggle-checkbox", checkboxBounds);
    const auto& releaseCommands = uiContext.EndFrame();

    return changed && releaseCommands.Size() == 3U &&
           UiContextTestAccess::GetBooleanState(uiContext, MakeUiId("toggle-checkbox"));
}

[[nodiscard]] bool TestCheckboxCheckedStatePersistsAcrossFrames()
{
    using namespace greenfield;

    const Rect checkboxBounds{
        .position = Vec2{10.0f, 20.0f},
        .size = Vec2{140.0f, 36.0f},
    };

    UiContext uiContext;
    UiContextTestAccess::SetBooleanState(uiContext, MakeUiId("persistent-checkbox"), true);

    uiContext.BeginFrame(MakeLayout());
    const bool changed = uiContext.Checkbox("persistent-checkbox", checkboxBounds);
    const auto& firstCommands = uiContext.EndFrame();
    if (changed || firstCommands.Size() != 3U ||
        !UiContextTestAccess::GetBooleanState(uiContext, MakeUiId("persistent-checkbox")))
    {
        return false;
    }

    uiContext.BeginFrame(MakeLayout());
    const bool secondFrameChanged = uiContext.Checkbox("persistent-checkbox", checkboxBounds);
    const auto& secondCommands = uiContext.EndFrame();
    return !secondFrameChanged && secondCommands.Size() == 3U &&
           UiContextTestAccess::GetBooleanState(uiContext, MakeUiId("persistent-checkbox"));
}

[[nodiscard]] bool TestCheckboxSecondClickTogglesBackOff()
{
    using namespace greenfield;

    const Rect checkboxBounds{
        .position = Vec2{10.0f, 20.0f},
        .size = Vec2{140.0f, 36.0f},
    };

    UiContext uiContext;
    UiContextTestAccess::SetBooleanState(uiContext, MakeUiId("second-click-checkbox"), true);

    uiContext.BeginFrame(MakeLayout(), InputState{.mousePosition = Vec2{20.0f, 30.0f}, .leftMouseButtonDown = true, .leftMouseButtonPressed = true});
    if (uiContext.Checkbox("second-click-checkbox", checkboxBounds))
    {
        return false;
    }
    const auto& pressCommands = uiContext.EndFrame();
    if (pressCommands.Size() != 3U)
    {
        return false;
    }

    uiContext.BeginFrame(MakeLayout(), InputState{.mousePosition = Vec2{20.0f, 30.0f}, .leftMouseButtonReleased = true});
    const bool changed = uiContext.Checkbox("second-click-checkbox", checkboxBounds);
    const auto& releaseCommands = uiContext.EndFrame();

    return changed && releaseCommands.Size() == 2U &&
           !UiContextTestAccess::GetBooleanState(uiContext, MakeUiId("second-click-checkbox"));
}

[[nodiscard]] bool TestCheckboxStateIsIndependentPerUiId()
{
    using namespace greenfield;

    const Rect firstCheckboxBounds{
        .position = Vec2{10.0f, 20.0f},
        .size = Vec2{140.0f, 36.0f},
    };
    const Rect secondCheckboxBounds{
        .position = Vec2{10.0f, 64.0f},
        .size = Vec2{140.0f, 36.0f},
    };

    UiContext uiContext;
    UiContextTestAccess::SetBooleanState(uiContext, MakeUiId("first-checkbox"), true);

    uiContext.BeginFrame(MakeLayout());
    const bool firstChanged = uiContext.Checkbox("first-checkbox", firstCheckboxBounds);
    const bool secondChanged = uiContext.Checkbox("second-checkbox", secondCheckboxBounds);
    const auto& commands = uiContext.EndFrame();

    return !firstChanged && !secondChanged && commands.Size() == 5U &&
           UiContextTestAccess::GetBooleanState(uiContext, MakeUiId("first-checkbox")) &&
           !UiContextTestAccess::GetBooleanState(uiContext, MakeUiId("second-checkbox"));
}

[[nodiscard]] bool TestOverlappingCheckboxAndButtonConsumeOneGesture()
{
    using namespace greenfield;

    const Rect overlappingBounds{
        .position = Vec2{10.0f, 20.0f},
        .size = Vec2{140.0f, 40.0f},
    };

    UiContext uiContext;
    uiContext.BeginFrame(MakeLayout(), InputState{.mousePosition = Vec2{20.0f, 30.0f}, .leftMouseButtonDown = true, .leftMouseButtonPressed = true});
    if (uiContext.Checkbox("overlap-checkbox", overlappingBounds) ||
        uiContext.Button("overlap-button", overlappingBounds))
    {
        return false;
    }
    const auto& pressCommands = uiContext.EndFrame();
    if (pressCommands.Size() != 4U)
    {
        return false;
    }

    uiContext.BeginFrame(MakeLayout(), InputState{.mousePosition = Vec2{20.0f, 30.0f}, .leftMouseButtonReleased = true});
    const bool checkboxChanged = uiContext.Checkbox("overlap-checkbox", overlappingBounds);
    const bool buttonClicked = uiContext.Button("overlap-button", overlappingBounds);
    const auto& releaseCommands = uiContext.EndFrame();

    return checkboxChanged && !buttonClicked && releaseCommands.Size() == 5U;
}

[[nodiscard]] bool TestOverlappingCheckboxesConsumeOneGesture()
{
    using namespace greenfield;

    const Rect overlappingBounds{
        .position = Vec2{10.0f, 20.0f},
        .size = Vec2{140.0f, 40.0f},
    };

    UiContext uiContext;
    uiContext.BeginFrame(MakeLayout(), InputState{.mousePosition = Vec2{20.0f, 30.0f}, .leftMouseButtonDown = true, .leftMouseButtonPressed = true});
    if (uiContext.Checkbox("first-overlap-checkbox", overlappingBounds) ||
        uiContext.Checkbox("second-overlap-checkbox", overlappingBounds))
    {
        return false;
    }
    const auto& pressCommands = uiContext.EndFrame();
    if (pressCommands.Size() != 4U)
    {
        return false;
    }

    uiContext.BeginFrame(MakeLayout(), InputState{.mousePosition = Vec2{20.0f, 30.0f}, .leftMouseButtonReleased = true});
    const bool firstChanged = uiContext.Checkbox("first-overlap-checkbox", overlappingBounds);
    const bool secondChanged = uiContext.Checkbox("second-overlap-checkbox", overlappingBounds);
    const auto& releaseCommands = uiContext.EndFrame();

    return firstChanged && !secondChanged && releaseCommands.Size() == 5U &&
           UiContextTestAccess::GetBooleanState(uiContext, MakeUiId("first-overlap-checkbox")) &&
           !UiContextTestAccess::GetBooleanState(uiContext, MakeUiId("second-overlap-checkbox"));
}

[[nodiscard]] bool TestLayoutCheckboxAdvancesLayout()
{
    using namespace greenfield;
    using greenfield::tests::RectanglesMatch;

    UiContext uiContext;
    uiContext.BeginFrame(MakeLayout());
    uiContext.BeginColumn(LayoutContainer{
        .bounds = Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{200.0f, 120.0f}},
        .padding = 10.0f,
        .gap = 6.0f,
        .itemSize = Vec2{80.0f, 40.0f},
    });
    const bool changed = uiContext.Checkbox("layout-checkbox");
    uiContext.Panel(Color{0.2f, 0.3f, 0.4f, 1.0f});

    const auto& commands = uiContext.EndFrame();
    return !changed && commands.Size() == 3U &&
           RectanglesMatch(commands.Commands()[2].rectangle,
                           Rect{.position = Vec2{10.0f, 56.0f}, .size = Vec2{80.0f, 40.0f}});
}

[[nodiscard]] bool TestExplicitBoundsCheckboxDoesNotAdvanceLayout()
{
    using namespace greenfield;
    using greenfield::tests::RectanglesMatch;

    UiContext uiContext;
    uiContext.BeginFrame(MakeLayout());
    uiContext.BeginColumn(LayoutContainer{
        .bounds = Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{200.0f, 120.0f}},
        .padding = 10.0f,
        .gap = 6.0f,
        .itemSize = Vec2{80.0f, 40.0f},
    });
    const bool changed =
        uiContext.Checkbox("explicit-checkbox",
                           Rect{.position = Vec2{30.0f, 70.0f}, .size = Vec2{120.0f, 32.0f}});
    uiContext.Panel(Color{0.2f, 0.3f, 0.4f, 1.0f});

    const auto& commands = uiContext.EndFrame();
    return !changed && commands.Size() == 3U &&
           RectanglesMatch(commands.Commands()[2].rectangle,
                           Rect{.position = Vec2{10.0f, 10.0f}, .size = Vec2{80.0f, 40.0f}});
}

[[nodiscard]] bool TestCheckboxRenderCommandsIncludeBoxIndicatorAndLabel()
{
    using namespace greenfield;
    using greenfield::tests::RectanglesMatch;

    const Rect checkboxBounds{
        .position = Vec2{10.0f, 20.0f},
        .size = Vec2{140.0f, 36.0f},
    };

    UiContext uiContext;
    UiContextTestAccess::SetBooleanState(uiContext, MakeUiId("render-checkbox"), true);
    uiContext.BeginFrame(MakeLayout());
    const bool changed = uiContext.Checkbox("render-checkbox", "Render label", checkboxBounds);
    const auto& commands = uiContext.EndFrame();

    return !changed && commands.Size() == 3U && commands.Commands()[0].type == RenderCommandType::FillRectangle &&
           commands.Commands()[1].type == RenderCommandType::FillRectangle &&
           commands.Commands()[2].type == RenderCommandType::DrawText &&
           RectanglesMatch(commands.Commands()[0].rectangle,
                           Rect{.position = Vec2{10.0f, 28.0f}, .size = Vec2{20.0f, 20.0f}}) &&
           RectanglesMatch(commands.Commands()[1].rectangle,
                           Rect{.position = Vec2{15.0f, 33.0f}, .size = Vec2{10.0f, 10.0f}}) &&
           commands.Commands()[2].text == "Render label";
}

[[nodiscard]] bool TestToggleDefaultsOff()
{
    using namespace greenfield;

    const Rect toggleBounds{
        .position = Vec2{10.0f, 20.0f},
        .size = Vec2{160.0f, 36.0f},
    };

    UiContext uiContext;
    uiContext.BeginFrame(MakeLayout());
    if (uiContext.Toggle("default-toggle", toggleBounds))
    {
        return false;
    }

    const auto& commands = uiContext.EndFrame();
    return commands.Size() == 3U &&
           !UiContextTestAccess::GetBooleanState(uiContext, MakeUiId("default-toggle"));
}

[[nodiscard]] bool TestToggleClickTogglesOn()
{
    using namespace greenfield;

    const Rect toggleBounds{
        .position = Vec2{10.0f, 20.0f},
        .size = Vec2{160.0f, 36.0f},
    };

    UiContext uiContext;
    uiContext.BeginFrame(MakeLayout(), InputState{.mousePosition = Vec2{20.0f, 30.0f}, .leftMouseButtonDown = true, .leftMouseButtonPressed = true});
    if (uiContext.Toggle("toggle-on", toggleBounds))
    {
        return false;
    }
    const auto& pressCommands = uiContext.EndFrame();
    if (pressCommands.Size() != 3U)
    {
        return false;
    }

    uiContext.BeginFrame(MakeLayout(), InputState{.mousePosition = Vec2{20.0f, 30.0f}, .leftMouseButtonReleased = true});
    const bool changed = uiContext.Toggle("toggle-on", toggleBounds);
    const auto& releaseCommands = uiContext.EndFrame();

    return changed && releaseCommands.Size() == 3U &&
           UiContextTestAccess::GetBooleanState(uiContext, MakeUiId("toggle-on"));
}

[[nodiscard]] bool TestToggleOnStatePersistsAcrossFrames()
{
    using namespace greenfield;

    const Rect toggleBounds{
        .position = Vec2{10.0f, 20.0f},
        .size = Vec2{160.0f, 36.0f},
    };

    UiContext uiContext;
    UiContextTestAccess::SetBooleanState(uiContext, MakeUiId("persistent-toggle"), true);

    uiContext.BeginFrame(MakeLayout());
    const bool changed = uiContext.Toggle("persistent-toggle", toggleBounds);
    const auto& firstCommands = uiContext.EndFrame();
    if (changed || firstCommands.Size() != 3U ||
        !UiContextTestAccess::GetBooleanState(uiContext, MakeUiId("persistent-toggle")))
    {
        return false;
    }

    uiContext.BeginFrame(MakeLayout());
    const bool secondFrameChanged = uiContext.Toggle("persistent-toggle", toggleBounds);
    const auto& secondCommands = uiContext.EndFrame();
    return !secondFrameChanged && secondCommands.Size() == 3U &&
           UiContextTestAccess::GetBooleanState(uiContext, MakeUiId("persistent-toggle"));
}

[[nodiscard]] bool TestToggleSecondClickTogglesBackOff()
{
    using namespace greenfield;

    const Rect toggleBounds{
        .position = Vec2{10.0f, 20.0f},
        .size = Vec2{160.0f, 36.0f},
    };

    UiContext uiContext;
    UiContextTestAccess::SetBooleanState(uiContext, MakeUiId("second-click-toggle"), true);

    uiContext.BeginFrame(MakeLayout(), InputState{.mousePosition = Vec2{20.0f, 30.0f}, .leftMouseButtonDown = true, .leftMouseButtonPressed = true});
    if (uiContext.Toggle("second-click-toggle", toggleBounds))
    {
        return false;
    }
    const auto& pressCommands = uiContext.EndFrame();
    if (pressCommands.Size() != 3U)
    {
        return false;
    }

    uiContext.BeginFrame(MakeLayout(), InputState{.mousePosition = Vec2{20.0f, 30.0f}, .leftMouseButtonReleased = true});
    const bool changed = uiContext.Toggle("second-click-toggle", toggleBounds);
    const auto& releaseCommands = uiContext.EndFrame();

    return changed && releaseCommands.Size() == 3U &&
           !UiContextTestAccess::GetBooleanState(uiContext, MakeUiId("second-click-toggle"));
}

[[nodiscard]] bool TestToggleStateIsIndependentPerUiId()
{
    using namespace greenfield;

    const Rect firstToggleBounds{
        .position = Vec2{10.0f, 20.0f},
        .size = Vec2{160.0f, 36.0f},
    };
    const Rect secondToggleBounds{
        .position = Vec2{10.0f, 64.0f},
        .size = Vec2{160.0f, 36.0f},
    };

    UiContext uiContext;
    UiContextTestAccess::SetBooleanState(uiContext, MakeUiId("first-toggle"), true);

    uiContext.BeginFrame(MakeLayout());
    const bool firstChanged = uiContext.Toggle("first-toggle", firstToggleBounds);
    const bool secondChanged = uiContext.Toggle("second-toggle", secondToggleBounds);
    const auto& commands = uiContext.EndFrame();

    return !firstChanged && !secondChanged && commands.Size() == 6U &&
           UiContextTestAccess::GetBooleanState(uiContext, MakeUiId("first-toggle")) &&
           !UiContextTestAccess::GetBooleanState(uiContext, MakeUiId("second-toggle"));
}

[[nodiscard]] bool TestToggleAndCheckboxStatesAreIndependentForDifferentUiIds()
{
    using namespace greenfield;

    const Rect toggleBounds{
        .position = Vec2{10.0f, 20.0f},
        .size = Vec2{160.0f, 36.0f},
    };
    const Rect checkboxBounds{
        .position = Vec2{10.0f, 64.0f},
        .size = Vec2{160.0f, 36.0f},
    };

    UiContext uiContext;
    UiContextTestAccess::SetBooleanState(uiContext, MakeUiId("independent-toggle"), true);

    uiContext.BeginFrame(MakeLayout());
    const bool toggleChanged = uiContext.Toggle("independent-toggle", toggleBounds);
    const bool checkboxChanged = uiContext.Checkbox("independent-checkbox", checkboxBounds);
    const auto& commands = uiContext.EndFrame();

    return !toggleChanged && !checkboxChanged && commands.Size() == 5U &&
           UiContextTestAccess::GetBooleanState(uiContext, MakeUiId("independent-toggle")) &&
           !UiContextTestAccess::GetBooleanState(uiContext, MakeUiId("independent-checkbox"));
}

[[nodiscard]] bool TestToggleAndCheckboxShareStateForMatchingUiId()
{
    using namespace greenfield;
    using greenfield::tests::RectanglesMatch;

    const Rect toggleBounds{
        .position = Vec2{10.0f, 20.0f},
        .size = Vec2{160.0f, 36.0f},
    };
    const Rect checkboxBounds{
        .position = Vec2{10.0f, 64.0f},
        .size = Vec2{160.0f, 36.0f},
    };

    UiContext uiContext;
    UiContextTestAccess::SetBooleanState(uiContext, MakeUiId("shared-state-control"), true);

    uiContext.BeginFrame(MakeLayout());
    const bool toggleChanged = uiContext.Toggle("shared-state-control", toggleBounds);
    const bool checkboxChanged = uiContext.Checkbox("shared-state-control", checkboxBounds);
    const auto& commands = uiContext.EndFrame();

    return !toggleChanged && !checkboxChanged && commands.Size() == 6U &&
           RectanglesMatch(commands.Commands()[1].rectangle,
                           Rect{.position = Vec2{33.0f, 30.0f}, .size = Vec2{16.0f, 16.0f}}) &&
           RectanglesMatch(commands.Commands()[4].rectangle,
                           Rect{.position = Vec2{15.0f, 77.0f}, .size = Vec2{10.0f, 10.0f}}) &&
           UiContextTestAccess::GetBooleanState(uiContext, MakeUiId("shared-state-control"));
}

[[nodiscard]] bool TestOverlappingToggleAndButtonConsumeOneGesture()
{
    using namespace greenfield;

    const Rect overlappingBounds{
        .position = Vec2{10.0f, 20.0f},
        .size = Vec2{160.0f, 40.0f},
    };

    UiContext uiContext;
    uiContext.BeginFrame(MakeLayout(), InputState{.mousePosition = Vec2{20.0f, 30.0f}, .leftMouseButtonDown = true, .leftMouseButtonPressed = true});
    if (uiContext.Toggle("overlap-toggle", overlappingBounds) ||
        uiContext.Button("overlap-button-with-toggle", overlappingBounds))
    {
        return false;
    }
    const auto& pressCommands = uiContext.EndFrame();
    if (pressCommands.Size() != 5U)
    {
        return false;
    }

    uiContext.BeginFrame(MakeLayout(), InputState{.mousePosition = Vec2{20.0f, 30.0f}, .leftMouseButtonReleased = true});
    const bool toggleChanged = uiContext.Toggle("overlap-toggle", overlappingBounds);
    const bool buttonClicked = uiContext.Button("overlap-button-with-toggle", overlappingBounds);
    const auto& releaseCommands = uiContext.EndFrame();

    return toggleChanged && !buttonClicked && releaseCommands.Size() == 5U &&
           UiContextTestAccess::GetBooleanState(uiContext, MakeUiId("overlap-toggle"));
}

[[nodiscard]] bool TestOverlappingToggleAndCheckboxConsumeOneGesture()
{
    using namespace greenfield;

    const Rect overlappingBounds{
        .position = Vec2{10.0f, 20.0f},
        .size = Vec2{160.0f, 40.0f},
    };

    UiContext uiContext;
    uiContext.BeginFrame(MakeLayout(), InputState{.mousePosition = Vec2{20.0f, 30.0f}, .leftMouseButtonDown = true, .leftMouseButtonPressed = true});
    if (uiContext.Toggle("overlap-toggle-checkbox", overlappingBounds) ||
        uiContext.Checkbox("overlap-checkbox-with-toggle", overlappingBounds))
    {
        return false;
    }
    const auto& pressCommands = uiContext.EndFrame();
    if (pressCommands.Size() != 5U)
    {
        return false;
    }

    uiContext.BeginFrame(MakeLayout(), InputState{.mousePosition = Vec2{20.0f, 30.0f}, .leftMouseButtonReleased = true});
    const bool toggleChanged = uiContext.Toggle("overlap-toggle-checkbox", overlappingBounds);
    const bool checkboxChanged = uiContext.Checkbox("overlap-checkbox-with-toggle", overlappingBounds);
    const auto& releaseCommands = uiContext.EndFrame();

    return toggleChanged && !checkboxChanged && releaseCommands.Size() == 5U &&
           UiContextTestAccess::GetBooleanState(uiContext, MakeUiId("overlap-toggle-checkbox")) &&
           !UiContextTestAccess::GetBooleanState(uiContext, MakeUiId("overlap-checkbox-with-toggle"));
}

[[nodiscard]] bool TestOverlappingTogglesConsumeOneGesture()
{
    using namespace greenfield;

    const Rect overlappingBounds{
        .position = Vec2{10.0f, 20.0f},
        .size = Vec2{160.0f, 40.0f},
    };

    UiContext uiContext;
    uiContext.BeginFrame(MakeLayout(), InputState{.mousePosition = Vec2{20.0f, 30.0f}, .leftMouseButtonDown = true, .leftMouseButtonPressed = true});
    if (uiContext.Toggle("first-overlap-toggle", overlappingBounds) ||
        uiContext.Toggle("second-overlap-toggle", overlappingBounds))
    {
        return false;
    }
    const auto& pressCommands = uiContext.EndFrame();
    if (pressCommands.Size() != 6U)
    {
        return false;
    }

    uiContext.BeginFrame(MakeLayout(), InputState{.mousePosition = Vec2{20.0f, 30.0f}, .leftMouseButtonReleased = true});
    const bool firstChanged = uiContext.Toggle("first-overlap-toggle", overlappingBounds);
    const bool secondChanged = uiContext.Toggle("second-overlap-toggle", overlappingBounds);
    const auto& releaseCommands = uiContext.EndFrame();

    return firstChanged && !secondChanged && releaseCommands.Size() == 6U &&
           UiContextTestAccess::GetBooleanState(uiContext, MakeUiId("first-overlap-toggle")) &&
           !UiContextTestAccess::GetBooleanState(uiContext, MakeUiId("second-overlap-toggle"));
}

[[nodiscard]] bool TestLayoutToggleAdvancesLayout()
{
    using namespace greenfield;
    using greenfield::tests::RectanglesMatch;

    UiContext uiContext;
    uiContext.BeginFrame(MakeLayout());
    uiContext.BeginColumn(LayoutContainer{
        .bounds = Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{220.0f, 120.0f}},
        .padding = 10.0f,
        .gap = 6.0f,
        .itemSize = Vec2{100.0f, 40.0f},
    });
    const bool changed = uiContext.Toggle("layout-toggle");
    uiContext.Panel(Color{0.2f, 0.3f, 0.4f, 1.0f});

    const auto& commands = uiContext.EndFrame();
    return !changed && commands.Size() == 4U &&
           RectanglesMatch(commands.Commands()[3].rectangle,
                           Rect{.position = Vec2{10.0f, 56.0f}, .size = Vec2{100.0f, 40.0f}});
}

[[nodiscard]] bool TestExplicitBoundsToggleDoesNotAdvanceLayout()
{
    using namespace greenfield;
    using greenfield::tests::RectanglesMatch;

    UiContext uiContext;
    uiContext.BeginFrame(MakeLayout());
    uiContext.BeginColumn(LayoutContainer{
        .bounds = Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{220.0f, 120.0f}},
        .padding = 10.0f,
        .gap = 6.0f,
        .itemSize = Vec2{100.0f, 40.0f},
    });
    const bool changed =
        uiContext.Toggle("explicit-toggle",
                         Rect{.position = Vec2{30.0f, 70.0f}, .size = Vec2{150.0f, 32.0f}});
    uiContext.Panel(Color{0.2f, 0.3f, 0.4f, 1.0f});

    const auto& commands = uiContext.EndFrame();
    return !changed && commands.Size() == 4U &&
           RectanglesMatch(commands.Commands()[3].rectangle,
                           Rect{.position = Vec2{10.0f, 10.0f}, .size = Vec2{100.0f, 40.0f}});
}

[[nodiscard]] bool TestToggleRenderCommandsIncludeTrackKnobAndLabel()
{
    using namespace greenfield;
    using greenfield::tests::RectanglesMatch;

    const Rect toggleBounds{
        .position = Vec2{10.0f, 20.0f},
        .size = Vec2{160.0f, 36.0f},
    };

    UiContext uiContext;
    UiContextTestAccess::SetBooleanState(uiContext, MakeUiId("render-toggle"), true);
    uiContext.BeginFrame(MakeLayout());
    const bool changed = uiContext.Toggle("render-toggle", "Render toggle", toggleBounds);
    const auto& commands = uiContext.EndFrame();

    return !changed && commands.Size() == 3U && commands.Commands()[0].type == RenderCommandType::FillRectangle &&
           commands.Commands()[1].type == RenderCommandType::FillRectangle &&
           commands.Commands()[2].type == RenderCommandType::DrawText &&
           RectanglesMatch(commands.Commands()[0].rectangle,
                           Rect{.position = Vec2{10.0f, 27.0f}, .size = Vec2{42.0f, 22.0f}}) &&
           RectanglesMatch(commands.Commands()[1].rectangle,
                           Rect{.position = Vec2{33.0f, 30.0f}, .size = Vec2{16.0f, 16.0f}}) &&
           commands.Commands()[2].text == "Render toggle";
}

[[nodiscard]] bool TestSliderDefaultsToMinimumValue()
{
    using namespace greenfield;

    const Rect sliderBounds{
        .position = Vec2{10.0f, 20.0f},
        .size = Vec2{180.0f, 36.0f},
    };

    UiContext uiContext;
    uiContext.BeginFrame(MakeLayout());
    if (uiContext.Slider("default-slider", sliderBounds, 2.0f, 10.0f))
    {
        return false;
    }

    const auto& commands = uiContext.EndFrame();
    return commands.Size() == 4U &&
           UiContextTestAccess::GetNumericState(uiContext, MakeUiId("default-slider"), 0.0f) == 2.0f;
}

[[nodiscard]] bool TestSliderValuePersistsAcrossFrames()
{
    using namespace greenfield;

    const Rect sliderBounds{
        .position = Vec2{10.0f, 20.0f},
        .size = Vec2{180.0f, 36.0f},
    };

    UiContext uiContext;
    UiContextTestAccess::SetNumericState(uiContext, MakeUiId("persistent-slider"), 6.0f);

    uiContext.BeginFrame(MakeLayout());
    const bool changed = uiContext.Slider("persistent-slider", sliderBounds, 0.0f, 10.0f);
    const auto& firstCommands = uiContext.EndFrame();
    if (changed || firstCommands.Size() != 4U ||
        UiContextTestAccess::GetNumericState(uiContext, MakeUiId("persistent-slider"), 0.0f) != 6.0f)
    {
        return false;
    }

    uiContext.BeginFrame(MakeLayout());
    const bool secondFrameChanged = uiContext.Slider("persistent-slider", sliderBounds, 0.0f, 10.0f);
    const auto& secondCommands = uiContext.EndFrame();
    return !secondFrameChanged && secondCommands.Size() == 4U &&
           UiContextTestAccess::GetNumericState(uiContext, MakeUiId("persistent-slider"), 0.0f) == 6.0f;
}

[[nodiscard]] bool TestSliderClickChangesValueOnlyOnChangedFrame()
{
    using namespace greenfield;

    const Rect sliderBounds{
        .position = Vec2{10.0f, 20.0f},
        .size = Vec2{180.0f, 36.0f},
    };

    UiContext uiContext;
    uiContext.BeginFrame(MakeLayout(), InputState{.mousePosition = Vec2{70.0f, 30.0f}, .leftMouseButtonDown = true, .leftMouseButtonPressed = true});
    const bool changedOnPress = uiContext.Slider("click-slider", sliderBounds, 0.0f, 10.0f);
    const auto& pressCommands = uiContext.EndFrame();
    if (!changedOnPress || pressCommands.Size() != 4U ||
        UiContextTestAccess::GetNumericState(uiContext, MakeUiId("click-slider"), 0.0f) != 5.0f)
    {
        return false;
    }

    uiContext.BeginFrame(MakeLayout(), InputState{.mousePosition = Vec2{70.0f, 30.0f}, .leftMouseButtonReleased = true});
    const bool changedOnRelease = uiContext.Slider("click-slider", sliderBounds, 0.0f, 10.0f);
    const auto& releaseCommands = uiContext.EndFrame();

    return !changedOnRelease && releaseCommands.Size() == 4U &&
           UiContextTestAccess::GetNumericState(uiContext, MakeUiId("click-slider"), 0.0f) == 5.0f;
}

[[nodiscard]] bool TestSliderClampsClickAfterTrackToMaximum()
{
    using namespace greenfield;

    const Rect sliderBounds{
        .position = Vec2{10.0f, 20.0f},
        .size = Vec2{220.0f, 36.0f},
    };

    UiContext uiContext;
    uiContext.BeginFrame(MakeLayout(), InputState{.mousePosition = Vec2{180.0f, 30.0f}, .leftMouseButtonDown = true, .leftMouseButtonPressed = true});
    const bool changed = uiContext.Slider("clamped-slider", sliderBounds, 0.0f, 10.0f);
    const auto& commands = uiContext.EndFrame();

    return changed && commands.Size() == 4U &&
           UiContextTestAccess::GetNumericState(uiContext, MakeUiId("clamped-slider"), 0.0f) == 10.0f;
}

[[nodiscard]] bool TestSliderHandlesReversedAndDegenerateRanges()
{
    using namespace greenfield;

    const Rect firstSliderBounds{
        .position = Vec2{10.0f, 20.0f},
        .size = Vec2{180.0f, 36.0f},
    };
    const Rect secondSliderBounds{
        .position = Vec2{10.0f, 64.0f},
        .size = Vec2{180.0f, 36.0f},
    };

    UiContext uiContext;
    uiContext.BeginFrame(MakeLayout());
    const bool reversedChanged = uiContext.Slider("reversed-slider", firstSliderBounds, 10.0f, 0.0f);
    const bool degenerateChanged = uiContext.Slider("degenerate-slider", secondSliderBounds, 3.0f, 3.0f);
    const auto& commands = uiContext.EndFrame();

    return !reversedChanged && !degenerateChanged && commands.Size() == 8U &&
           UiContextTestAccess::GetNumericState(uiContext, MakeUiId("reversed-slider"), 5.0f) == 0.0f &&
           UiContextTestAccess::GetNumericState(uiContext, MakeUiId("degenerate-slider"), 0.0f) == 3.0f;
}

[[nodiscard]] bool TestSliderStateIsIndependentPerUiId()
{
    using namespace greenfield;

    const Rect firstSliderBounds{
        .position = Vec2{10.0f, 20.0f},
        .size = Vec2{180.0f, 36.0f},
    };
    const Rect secondSliderBounds{
        .position = Vec2{10.0f, 64.0f},
        .size = Vec2{180.0f, 36.0f},
    };

    UiContext uiContext;
    UiContextTestAccess::SetNumericState(uiContext, MakeUiId("first-slider"), 0.25f);

    uiContext.BeginFrame(MakeLayout());
    const bool firstChanged = uiContext.Slider("first-slider", firstSliderBounds, 0.0f, 1.0f);
    const bool secondChanged = uiContext.Slider("second-slider", secondSliderBounds, 0.0f, 1.0f);
    const auto& commands = uiContext.EndFrame();

    return !firstChanged && !secondChanged && commands.Size() == 8U &&
           UiContextTestAccess::GetNumericState(uiContext, MakeUiId("first-slider"), 0.0f) == 0.25f &&
           UiContextTestAccess::GetNumericState(uiContext, MakeUiId("second-slider"), 0.5f) == 0.0f;
}

[[nodiscard]] bool TestLayoutSliderAdvancesLayout()
{
    using namespace greenfield;
    using greenfield::tests::RectanglesMatch;

    UiContext uiContext;
    uiContext.BeginFrame(MakeLayout());
    uiContext.BeginColumn(LayoutContainer{
        .bounds = Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{240.0f, 120.0f}},
        .padding = 10.0f,
        .gap = 6.0f,
        .itemSize = Vec2{180.0f, 40.0f},
    });
    const bool changed = uiContext.Slider("layout-slider", 0.0f, 1.0f);
    uiContext.Panel(Color{0.2f, 0.3f, 0.4f, 1.0f});

    const auto& commands = uiContext.EndFrame();
    return !changed && commands.Size() == 5U &&
           RectanglesMatch(commands.Commands()[4].rectangle,
                           Rect{.position = Vec2{10.0f, 56.0f}, .size = Vec2{180.0f, 40.0f}});
}

[[nodiscard]] bool TestExplicitBoundsSliderDoesNotAdvanceLayout()
{
    using namespace greenfield;
    using greenfield::tests::RectanglesMatch;

    UiContext uiContext;
    uiContext.BeginFrame(MakeLayout());
    uiContext.BeginColumn(LayoutContainer{
        .bounds = Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{240.0f, 120.0f}},
        .padding = 10.0f,
        .gap = 6.0f,
        .itemSize = Vec2{120.0f, 40.0f},
    });
    const bool changed =
        uiContext.Slider("explicit-slider",
                         Rect{.position = Vec2{30.0f, 70.0f}, .size = Vec2{180.0f, 32.0f}},
                         0.0f,
                         1.0f);
    uiContext.Panel(Color{0.2f, 0.3f, 0.4f, 1.0f});

    const auto& commands = uiContext.EndFrame();
    return !changed && commands.Size() == 5U &&
           RectanglesMatch(commands.Commands()[4].rectangle,
                           Rect{.position = Vec2{10.0f, 10.0f}, .size = Vec2{120.0f, 40.0f}});
}

[[nodiscard]] bool TestSliderRenderCommandsIncludeTrackFillThumbAndLabel()
{
    using namespace greenfield;
    using greenfield::tests::RectanglesMatch;

    const Rect sliderBounds{
        .position = Vec2{10.0f, 20.0f},
        .size = Vec2{180.0f, 36.0f},
    };

    UiContext uiContext;
    UiContextTestAccess::SetNumericState(uiContext, MakeUiId("render-slider"), 0.5f);
    uiContext.BeginFrame(MakeLayout());
    const bool changed = uiContext.Slider("render-slider", "Render slider", sliderBounds, 0.0f, 1.0f);
    const auto& commands = uiContext.EndFrame();

    return !changed && commands.Size() == 4U && commands.Commands()[0].type == RenderCommandType::FillRectangle &&
           commands.Commands()[1].type == RenderCommandType::FillRectangle &&
           commands.Commands()[2].type == RenderCommandType::FillRectangle &&
           commands.Commands()[3].type == RenderCommandType::DrawText &&
           RectanglesMatch(commands.Commands()[0].rectangle,
                           Rect{.position = Vec2{10.0f, 34.0f}, .size = Vec2{120.0f, 8.0f}}) &&
           RectanglesMatch(commands.Commands()[1].rectangle,
                           Rect{.position = Vec2{10.0f, 34.0f}, .size = Vec2{60.0f, 8.0f}}) &&
           RectanglesMatch(commands.Commands()[2].rectangle,
                           Rect{.position = Vec2{63.0f, 27.0f}, .size = Vec2{14.0f, 22.0f}}) &&
           commands.Commands()[3].text == "Render slider";
}

[[nodiscard]] bool TestSliderDragUpdatesWhileCaptured()
{
    using namespace greenfield;

    const Rect sliderBounds{
        .position = Vec2{10.0f, 20.0f},
        .size = Vec2{180.0f, 36.0f},
    };

    UiContext uiContext;
    uiContext.BeginFrame(MakeLayout(), InputState{.mousePosition = Vec2{10.0f, 30.0f}, .leftMouseButtonDown = true, .leftMouseButtonPressed = true});
    const bool changedOnPress = uiContext.Slider("drag-slider", sliderBounds, 0.0f, 1.0f);
    const auto& pressCommands = uiContext.EndFrame();
    if (changedOnPress || pressCommands.Size() != 4U)
    {
        return false;
    }

    uiContext.BeginFrame(MakeLayout(), InputState{.mousePosition = Vec2{130.0f, 30.0f}, .leftMouseButtonDown = true});
    const bool changedOnDrag = uiContext.Slider("drag-slider", sliderBounds, 0.0f, 1.0f);
    const auto& dragCommands = uiContext.EndFrame();

    return changedOnDrag && dragCommands.Size() == 4U &&
           UiContextTestAccess::GetNumericState(uiContext, MakeUiId("drag-slider"), 0.0f) == 1.0f;
}

[[nodiscard]] bool TestOverlappingSliderAndButtonConsumeOneGesture()
{
    using namespace greenfield;

    const Rect overlappingBounds{
        .position = Vec2{10.0f, 20.0f},
        .size = Vec2{180.0f, 40.0f},
    };

    UiContext uiContext;
    uiContext.BeginFrame(MakeLayout(), InputState{.mousePosition = Vec2{70.0f, 30.0f}, .leftMouseButtonDown = true, .leftMouseButtonPressed = true});
    const bool sliderChanged = uiContext.Slider("overlap-slider", overlappingBounds, 0.0f, 1.0f);
    const bool buttonClickedOnPress = uiContext.Button("overlap-button-with-slider", overlappingBounds);
    const auto& pressCommands = uiContext.EndFrame();
    if (!sliderChanged || buttonClickedOnPress || pressCommands.Size() != 6U)
    {
        return false;
    }

    uiContext.BeginFrame(MakeLayout(), InputState{.mousePosition = Vec2{70.0f, 30.0f}, .leftMouseButtonReleased = true});
    const bool sliderChangedOnRelease = uiContext.Slider("overlap-slider", overlappingBounds, 0.0f, 1.0f);
    const bool buttonClickedOnRelease = uiContext.Button("overlap-button-with-slider", overlappingBounds);
    const auto& releaseCommands = uiContext.EndFrame();

    return !sliderChangedOnRelease && !buttonClickedOnRelease && releaseCommands.Size() == 6U;
}

[[nodiscard]] bool TestFocusDefaultsToEmpty()
{
    using namespace greenfield;

    const UiContext uiContext;
    return !uiContext.FocusedControlId().has_value() && !uiContext.HasFocus("missing-control");
}

[[nodiscard]] bool TestControlStylesDefaultToOuterRingFocusConfiguration()
{
    using namespace greenfield;

    const ButtonStyle buttonStyle{};
    const CheckboxStyle checkboxStyle{};
    const ToggleStyle toggleStyle{};
    const SliderStyle sliderStyle{};

    const auto hasExpectedDefaults = [](const FocusStyle& focusStyle) {
        return focusStyle.kind == FocusVisualKind::OuterRing &&
               tests::ColorsMatch(focusStyle.color, Color{0.92f, 0.96f, 1.0f, 0.95f}) &&
               focusStyle.thickness == 2.0f && focusStyle.outset == 2.0f &&
               focusStyle.cornerRadiusOffset == 2.0f;
    };

    return hasExpectedDefaults(buttonStyle.focus) &&
           hasExpectedDefaults(checkboxStyle.focus) &&
           hasExpectedDefaults(toggleStyle.focus) &&
           hasExpectedDefaults(sliderStyle.focus);
}

[[nodiscard]] bool TestControlStylesPreserveCustomFocusConfiguration()
{
    using namespace greenfield;

    const FocusStyle customFocusStyle{
        .kind = FocusVisualKind::None,
        .color = Color{0.2f, 0.7f, 0.3f, 0.8f},
        .thickness = 3.0f,
        .outset = 4.0f,
        .cornerRadiusOffset = 5.0f,
    };

    ButtonStyle buttonStyle{};
    buttonStyle.focus = customFocusStyle;

    CheckboxStyle checkboxStyle{};
    checkboxStyle.focus = customFocusStyle;

    ToggleStyle toggleStyle{};
    toggleStyle.focus = customFocusStyle;

    SliderStyle sliderStyle{};
    sliderStyle.focus = customFocusStyle;

    return buttonStyle.focus.kind == FocusVisualKind::None &&
           tests::ColorsMatch(buttonStyle.focus.color, customFocusStyle.color) &&
           buttonStyle.focus.thickness == customFocusStyle.thickness &&
           buttonStyle.focus.outset == customFocusStyle.outset &&
           buttonStyle.focus.cornerRadiusOffset == customFocusStyle.cornerRadiusOffset &&
           checkboxStyle.focus.kind == FocusVisualKind::None &&
           tests::ColorsMatch(checkboxStyle.focus.color, customFocusStyle.color) &&
           checkboxStyle.focus.thickness == customFocusStyle.thickness &&
           checkboxStyle.focus.outset == customFocusStyle.outset &&
           checkboxStyle.focus.cornerRadiusOffset == customFocusStyle.cornerRadiusOffset &&
           toggleStyle.focus.kind == FocusVisualKind::None &&
           tests::ColorsMatch(toggleStyle.focus.color, customFocusStyle.color) &&
           toggleStyle.focus.thickness == customFocusStyle.thickness &&
           toggleStyle.focus.outset == customFocusStyle.outset &&
           toggleStyle.focus.cornerRadiusOffset == customFocusStyle.cornerRadiusOffset &&
           sliderStyle.focus.kind == FocusVisualKind::None &&
           tests::ColorsMatch(sliderStyle.focus.color, customFocusStyle.color) &&
           sliderStyle.focus.thickness == customFocusStyle.thickness &&
           sliderStyle.focus.outset == customFocusStyle.outset &&
           sliderStyle.focus.cornerRadiusOffset == customFocusStyle.cornerRadiusOffset;
}

[[nodiscard]] bool TestFocusedButtonEmitsOuterFocusRingCommand()
{
    using namespace greenfield;
    using greenfield::tests::ColorsMatch;
    using greenfield::tests::RectanglesMatch;

    const Rect buttonBounds{
        .position = Vec2{10.0f, 20.0f},
        .size = Vec2{100.0f, 40.0f},
    };

    UiContext uiContext;
    uiContext.RequestFocus("focused-button");
    uiContext.BeginFrame(MakeLayout());
    const bool clicked = uiContext.Button("focused-button", buttonBounds);
    const auto& commands = uiContext.EndFrame();

    return !clicked && commands.Size() == 3U &&
           commands.Commands()[0].type == RenderCommandType::FillRectangle &&
           RectanglesMatch(commands.Commands()[0].rectangle,
                           Rect{.position = Vec2{8.0f, 18.0f}, .size = Vec2{104.0f, 44.0f}}) &&
           ColorsMatch(commands.Commands()[0].fillColor, Color{0.0f, 0.0f, 0.0f, 0.0f}) &&
           ColorsMatch(commands.Commands()[0].borderColor, Color{0.92f, 0.96f, 1.0f, 0.95f}) &&
           commands.Commands()[0].borderThickness == 2.0f &&
           commands.Commands()[0].cornerRadius == 10.0f &&
           commands.Commands()[1].type == RenderCommandType::FillRectangle &&
           commands.Commands()[2].type == RenderCommandType::DrawText;
}

[[nodiscard]] bool TestFocusedCheckboxEmitsFocusRingAroundBox()
{
    using namespace greenfield;
    using greenfield::tests::RectanglesMatch;

    const Rect checkboxBounds{
        .position = Vec2{10.0f, 20.0f},
        .size = Vec2{140.0f, 36.0f},
    };

    UiContext uiContext;
    uiContext.RequestFocus("focused-checkbox");
    uiContext.BeginFrame(MakeLayout());
    const bool changed = uiContext.Checkbox("focused-checkbox", "Focused checkbox", checkboxBounds);
    const auto& commands = uiContext.EndFrame();

    return !changed && commands.Size() == 3U &&
           RectanglesMatch(commands.Commands()[0].rectangle,
                           Rect{.position = Vec2{8.0f, 26.0f}, .size = Vec2{24.0f, 24.0f}}) &&
           commands.Commands()[1].type == RenderCommandType::FillRectangle &&
           RectanglesMatch(commands.Commands()[1].rectangle,
                           Rect{.position = Vec2{10.0f, 28.0f}, .size = Vec2{20.0f, 20.0f}}) &&
           commands.Commands()[2].type == RenderCommandType::DrawText;
}

[[nodiscard]] bool TestFocusedToggleEmitsFocusRingAroundTrack()
{
    using namespace greenfield;
    using greenfield::tests::RectanglesMatch;

    const Rect toggleBounds{
        .position = Vec2{10.0f, 20.0f},
        .size = Vec2{160.0f, 36.0f},
    };

    UiContext uiContext;
    uiContext.RequestFocus("focused-toggle");
    uiContext.BeginFrame(MakeLayout());
    const bool changed = uiContext.Toggle("focused-toggle", "Focused toggle", toggleBounds);
    const auto& commands = uiContext.EndFrame();

    return !changed && commands.Size() == 4U &&
           RectanglesMatch(commands.Commands()[0].rectangle,
                           Rect{.position = Vec2{8.0f, 25.0f}, .size = Vec2{46.0f, 26.0f}}) &&
           commands.Commands()[1].type == RenderCommandType::FillRectangle &&
           RectanglesMatch(commands.Commands()[1].rectangle,
                           Rect{.position = Vec2{10.0f, 27.0f}, .size = Vec2{42.0f, 22.0f}}) &&
           commands.Commands()[2].type == RenderCommandType::FillRectangle &&
           commands.Commands()[3].type == RenderCommandType::DrawText;
}

[[nodiscard]] bool TestFocusedSliderEmitsFocusRingAroundTrack()
{
    using namespace greenfield;
    using greenfield::tests::RectanglesMatch;

    const Rect sliderBounds{
        .position = Vec2{10.0f, 20.0f},
        .size = Vec2{180.0f, 36.0f},
    };

    UiContext uiContext;
    uiContext.RequestFocus("focused-slider");
    uiContext.BeginFrame(MakeLayout());
    const bool changed = uiContext.Slider("focused-slider", "Focused slider", sliderBounds, 0.0f, 1.0f);
    const auto& commands = uiContext.EndFrame();

    return !changed && commands.Size() == 5U &&
           RectanglesMatch(commands.Commands()[0].rectangle,
                           Rect{.position = Vec2{8.0f, 32.0f}, .size = Vec2{124.0f, 12.0f}}) &&
           commands.Commands()[1].type == RenderCommandType::FillRectangle &&
           RectanglesMatch(commands.Commands()[1].rectangle,
                           Rect{.position = Vec2{10.0f, 34.0f}, .size = Vec2{120.0f, 8.0f}}) &&
           commands.Commands()[2].type == RenderCommandType::FillRectangle &&
           commands.Commands()[3].type == RenderCommandType::FillRectangle &&
           commands.Commands()[4].type == RenderCommandType::DrawText;
}

[[nodiscard]] bool TestFocusVisualKindNoneSuppressesFocusRing()
{
    using namespace greenfield;

    ButtonStyle buttonStyle{};
    buttonStyle.focus.kind = FocusVisualKind::None;

    UiContext uiContext;
    uiContext.RequestFocus("button-without-focus-ring");
    uiContext.BeginFrame(MakeLayout());
    const bool clicked = uiContext.Button("button-without-focus-ring",
                                          "No focus ring",
                                          Rect{.position = Vec2{10.0f, 20.0f}, .size = Vec2{100.0f, 40.0f}},
                                          buttonStyle);
    const auto& commands = uiContext.EndFrame();

    return !clicked && commands.Size() == 2U &&
           commands.Commands()[0].type == RenderCommandType::FillRectangle &&
           commands.Commands()[1].type == RenderCommandType::DrawText;
}

[[nodiscard]] bool TestCustomFocusStyleControlsFocusRingGeometry()
{
    using namespace greenfield;
    using greenfield::tests::ColorsMatch;
    using greenfield::tests::RectanglesMatch;

    ButtonStyle buttonStyle{};
    buttonStyle.cornerRadius = 6.0f;
    buttonStyle.focus = FocusStyle{
        .kind = FocusVisualKind::OuterRing,
        .color = Color{0.2f, 0.7f, 0.3f, 0.8f},
        .thickness = 3.0f,
        .outset = 4.0f,
        .cornerRadiusOffset = 5.0f,
    };

    UiContext uiContext;
    uiContext.RequestFocus("custom-focus-button");
    uiContext.BeginFrame(MakeLayout());
    const bool clicked = uiContext.Button("custom-focus-button",
                                          "Custom focus",
                                          Rect{.position = Vec2{10.0f, 20.0f}, .size = Vec2{100.0f, 40.0f}},
                                          buttonStyle);
    const auto& commands = uiContext.EndFrame();

    return !clicked && commands.Size() == 3U &&
           RectanglesMatch(commands.Commands()[0].rectangle,
                           Rect{.position = Vec2{6.0f, 16.0f}, .size = Vec2{108.0f, 48.0f}}) &&
           ColorsMatch(commands.Commands()[0].borderColor, buttonStyle.focus.color) &&
           commands.Commands()[0].borderThickness == 3.0f &&
           commands.Commands()[0].cornerRadius == 11.0f;
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

[[nodiscard]] bool TestTabFocusesFirstFocusableControlWhenNothingIsFocused()
{
    using namespace greenfield;

    UiContext uiContext;
    uiContext.BeginFrame(MakeLayout(), InputState{.tabPressed = true});
    const bool buttonClicked = uiContext.Button("first-focusable", Rect{.position = Vec2{10.0f, 20.0f}, .size = Vec2{100.0f, 40.0f}});
    const bool checkboxChanged =
        uiContext.Checkbox("second-focusable", Rect{.position = Vec2{10.0f, 70.0f}, .size = Vec2{140.0f, 36.0f}});
    const auto& commands = uiContext.EndFrame();

    return !buttonClicked && !checkboxChanged && commands.Size() == 4U && uiContext.HasFocus("first-focusable");
}

[[nodiscard]] bool TestShiftTabFocusesLastFocusableControlWhenNothingIsFocused()
{
    using namespace greenfield;

    UiContext uiContext;
    uiContext.BeginFrame(MakeLayout(), InputState{.shiftTabPressed = true});
    const bool buttonClicked = uiContext.Button("first-focusable", Rect{.position = Vec2{10.0f, 20.0f}, .size = Vec2{100.0f, 40.0f}});
    const bool toggleChanged =
        uiContext.Toggle("last-focusable", Rect{.position = Vec2{10.0f, 70.0f}, .size = Vec2{160.0f, 36.0f}});
    const auto& commands = uiContext.EndFrame();

    return !buttonClicked && !toggleChanged && commands.Size() == 5U && uiContext.HasFocus("last-focusable");
}

[[nodiscard]] bool TestTabMovesFocusForwardInEncounterOrder()
{
    using namespace greenfield;

    UiContext uiContext;
    uiContext.RequestFocus("first-control");

    uiContext.BeginFrame(MakeLayout(), InputState{.tabPressed = true});
    const bool firstClicked =
        uiContext.Button("first-control", Rect{.position = Vec2{10.0f, 20.0f}, .size = Vec2{100.0f, 40.0f}});
    const bool secondChanged =
        uiContext.Checkbox("second-control", Rect{.position = Vec2{10.0f, 70.0f}, .size = Vec2{140.0f, 36.0f}});
    const bool thirdChanged =
        uiContext.Toggle("third-control", Rect{.position = Vec2{10.0f, 116.0f}, .size = Vec2{160.0f, 36.0f}});
    const auto& commands = uiContext.EndFrame();

    return !firstClicked && !secondChanged && !thirdChanged && commands.Size() == 8U &&
           uiContext.HasFocus("second-control");
}

[[nodiscard]] bool TestShiftTabMovesFocusBackwardInEncounterOrder()
{
    using namespace greenfield;

    UiContext uiContext;
    uiContext.RequestFocus("third-control");

    uiContext.BeginFrame(MakeLayout(), InputState{.shiftTabPressed = true});
    const bool firstClicked =
        uiContext.Button("first-control", Rect{.position = Vec2{10.0f, 20.0f}, .size = Vec2{100.0f, 40.0f}});
    const bool secondChanged =
        uiContext.Checkbox("second-control", Rect{.position = Vec2{10.0f, 70.0f}, .size = Vec2{140.0f, 36.0f}});
    const bool thirdChanged =
        uiContext.Toggle("third-control", Rect{.position = Vec2{10.0f, 116.0f}, .size = Vec2{160.0f, 36.0f}});
    const auto& commands = uiContext.EndFrame();

    return !firstClicked && !secondChanged && !thirdChanged && commands.Size() == 8U &&
           uiContext.HasFocus("second-control");
}

[[nodiscard]] bool TestFocusPersistsWhenFocusedControlIsStillRegistered()
{
    using namespace greenfield;

    UiContext uiContext;
    uiContext.RequestFocus("persistent-focusable");

    uiContext.BeginFrame(MakeLayout());
    const bool firstClicked =
        uiContext.Button("first-control", Rect{.position = Vec2{10.0f, 20.0f}, .size = Vec2{100.0f, 40.0f}});
    const bool firstChanged =
        uiContext.Checkbox("persistent-focusable", Rect{.position = Vec2{10.0f, 70.0f}, .size = Vec2{160.0f, 36.0f}});
    const auto& firstCommands = uiContext.EndFrame();
    if (firstClicked || firstChanged || firstCommands.Size() != 5U || !uiContext.HasFocus("persistent-focusable"))
    {
        return false;
    }

    uiContext.BeginFrame(MakeLayout());
    const bool secondClicked =
        uiContext.Button("first-control", Rect{.position = Vec2{10.0f, 20.0f}, .size = Vec2{100.0f, 40.0f}});
    const bool secondChanged =
        uiContext.Checkbox("persistent-focusable", Rect{.position = Vec2{10.0f, 70.0f}, .size = Vec2{160.0f, 36.0f}});
    const auto& secondCommands = uiContext.EndFrame();

    return !secondClicked && !secondChanged && secondCommands.Size() == 5U &&
           uiContext.HasFocus("persistent-focusable");
}

[[nodiscard]] bool TestStaleFocusedControlTraversalStartsFromFrameEdge()
{
    using namespace greenfield;

    UiContext uiContext;
    uiContext.RequestFocus("missing-control");

    uiContext.BeginFrame(MakeLayout(), InputState{.tabPressed = true});
    const bool firstClicked =
        uiContext.Button("first-control", Rect{.position = Vec2{10.0f, 20.0f}, .size = Vec2{100.0f, 40.0f}});
    const bool secondClicked =
        uiContext.Button("second-control", Rect{.position = Vec2{10.0f, 70.0f}, .size = Vec2{100.0f, 40.0f}});
    const auto& forwardCommands = uiContext.EndFrame();
    if (firstClicked || secondClicked || forwardCommands.Size() != 4U || !uiContext.HasFocus("first-control"))
    {
        return false;
    }

    uiContext.RequestFocus("missing-control");
    uiContext.BeginFrame(MakeLayout(), InputState{.shiftTabPressed = true});
    const bool backwardFirstClicked =
        uiContext.Button("first-control", Rect{.position = Vec2{10.0f, 20.0f}, .size = Vec2{100.0f, 40.0f}});
    const bool backwardSecondClicked =
        uiContext.Button("second-control", Rect{.position = Vec2{10.0f, 70.0f}, .size = Vec2{100.0f, 40.0f}});
    const auto& backwardCommands = uiContext.EndFrame();

    return !backwardFirstClicked && !backwardSecondClicked && backwardCommands.Size() == 4U &&
           uiContext.HasFocus("second-control");
}

[[nodiscard]] bool TestKeyboardTraversalDoesNotActivateControls()
{
    using namespace greenfield;

    UiContext uiContext;
    uiContext.BeginFrame(MakeLayout(), InputState{.tabPressed = true, .enterPressed = true, .spacePressed = true});
    const bool buttonClicked = uiContext.Button("keyboard-button", Rect{.position = Vec2{10.0f, 20.0f}, .size = Vec2{100.0f, 40.0f}});
    const bool checkboxChanged =
        uiContext.Checkbox("keyboard-checkbox", Rect{.position = Vec2{10.0f, 70.0f}, .size = Vec2{140.0f, 36.0f}});
    const bool toggleChanged =
        uiContext.Toggle("keyboard-toggle", Rect{.position = Vec2{10.0f, 116.0f}, .size = Vec2{160.0f, 36.0f}});
    const bool sliderChanged =
        uiContext.Slider("keyboard-slider", Rect{.position = Vec2{10.0f, 162.0f}, .size = Vec2{180.0f, 36.0f}}, 0.0f, 1.0f);
    const auto& commands = uiContext.EndFrame();

    return !buttonClicked && !checkboxChanged && !toggleChanged && !sliderChanged && commands.Size() == 11U &&
           uiContext.HasFocus("keyboard-button") &&
           !UiContextTestAccess::GetBooleanState(uiContext, MakeUiId("keyboard-checkbox")) &&
           !UiContextTestAccess::GetBooleanState(uiContext, MakeUiId("keyboard-toggle")) &&
           UiContextTestAccess::GetNumericState(uiContext, MakeUiId("keyboard-slider"), 0.5f) == 0.0f;
}

[[nodiscard]] bool TestFocusedButtonActivatesOnEnter()
{
    using namespace greenfield;

    UiContext uiContext;
    uiContext.RequestFocus("keyboard-button");
    uiContext.BeginFrame(MakeLayout(), InputState{.enterPressed = true});
    const bool clicked =
        uiContext.Button("keyboard-button", Rect{.position = Vec2{10.0f, 20.0f}, .size = Vec2{100.0f, 40.0f}});
    const auto& commands = uiContext.EndFrame();

    return clicked && commands.Size() == 3U;
}

[[nodiscard]] bool TestFocusedButtonActivatesOnSpace()
{
    using namespace greenfield;

    UiContext uiContext;
    uiContext.RequestFocus("keyboard-button");
    uiContext.BeginFrame(MakeLayout(), InputState{.spacePressed = true});
    const bool clicked =
        uiContext.Button("keyboard-button", Rect{.position = Vec2{10.0f, 20.0f}, .size = Vec2{100.0f, 40.0f}});
    const auto& commands = uiContext.EndFrame();

    return clicked && commands.Size() == 3U;
}

[[nodiscard]] bool TestUnfocusedButtonIgnoresKeyboardActivation()
{
    using namespace greenfield;

    UiContext uiContext;
    uiContext.RequestFocus("other-control");
    uiContext.BeginFrame(MakeLayout(), InputState{.enterPressed = true, .spacePressed = true});
    const bool clicked =
        uiContext.Button("keyboard-button", Rect{.position = Vec2{10.0f, 20.0f}, .size = Vec2{100.0f, 40.0f}});
    const auto& commands = uiContext.EndFrame();

    return !clicked && commands.Size() == 2U;
}

[[nodiscard]] bool TestFocusedCheckboxTogglesOnEnter()
{
    using namespace greenfield;

    UiContext uiContext;
    uiContext.RequestFocus("keyboard-checkbox");
    uiContext.BeginFrame(MakeLayout(), InputState{.enterPressed = true});
    const bool changed =
        uiContext.Checkbox("keyboard-checkbox", Rect{.position = Vec2{10.0f, 20.0f}, .size = Vec2{140.0f, 36.0f}});
    const auto& commands = uiContext.EndFrame();

    return changed && commands.Size() == 4U &&
           UiContextTestAccess::GetBooleanState(uiContext, MakeUiId("keyboard-checkbox"));
}

[[nodiscard]] bool TestFocusedCheckboxTogglesOnSpace()
{
    using namespace greenfield;

    UiContext uiContext;
    uiContext.RequestFocus("keyboard-checkbox");
    uiContext.BeginFrame(MakeLayout(), InputState{.spacePressed = true});
    const bool changed =
        uiContext.Checkbox("keyboard-checkbox", Rect{.position = Vec2{10.0f, 20.0f}, .size = Vec2{140.0f, 36.0f}});
    const auto& commands = uiContext.EndFrame();

    return changed && commands.Size() == 4U &&
           UiContextTestAccess::GetBooleanState(uiContext, MakeUiId("keyboard-checkbox"));
}

[[nodiscard]] bool TestUnfocusedCheckboxIgnoresKeyboardActivation()
{
    using namespace greenfield;

    UiContext uiContext;
    uiContext.RequestFocus("other-control");
    uiContext.BeginFrame(MakeLayout(), InputState{.enterPressed = true, .spacePressed = true});
    const bool changed =
        uiContext.Checkbox("keyboard-checkbox", Rect{.position = Vec2{10.0f, 20.0f}, .size = Vec2{140.0f, 36.0f}});
    const auto& commands = uiContext.EndFrame();

    return !changed && commands.Size() == 2U &&
           !UiContextTestAccess::GetBooleanState(uiContext, MakeUiId("keyboard-checkbox"));
}

[[nodiscard]] bool TestFocusedToggleTogglesOnEnter()
{
    using namespace greenfield;

    UiContext uiContext;
    uiContext.RequestFocus("keyboard-toggle");
    uiContext.BeginFrame(MakeLayout(), InputState{.enterPressed = true});
    const bool changed =
        uiContext.Toggle("keyboard-toggle", Rect{.position = Vec2{10.0f, 20.0f}, .size = Vec2{160.0f, 36.0f}});
    const auto& commands = uiContext.EndFrame();

    return changed && commands.Size() == 4U &&
           UiContextTestAccess::GetBooleanState(uiContext, MakeUiId("keyboard-toggle"));
}

[[nodiscard]] bool TestFocusedToggleTogglesOnSpace()
{
    using namespace greenfield;

    UiContext uiContext;
    uiContext.RequestFocus("keyboard-toggle");
    uiContext.BeginFrame(MakeLayout(), InputState{.spacePressed = true});
    const bool changed =
        uiContext.Toggle("keyboard-toggle", Rect{.position = Vec2{10.0f, 20.0f}, .size = Vec2{160.0f, 36.0f}});
    const auto& commands = uiContext.EndFrame();

    return changed && commands.Size() == 4U &&
           UiContextTestAccess::GetBooleanState(uiContext, MakeUiId("keyboard-toggle"));
}

[[nodiscard]] bool TestUnfocusedToggleIgnoresKeyboardActivation()
{
    using namespace greenfield;

    UiContext uiContext;
    uiContext.RequestFocus("other-control");
    uiContext.BeginFrame(MakeLayout(), InputState{.enterPressed = true, .spacePressed = true});
    const bool changed =
        uiContext.Toggle("keyboard-toggle", Rect{.position = Vec2{10.0f, 20.0f}, .size = Vec2{160.0f, 36.0f}});
    const auto& commands = uiContext.EndFrame();

    return !changed && commands.Size() == 3U &&
           !UiContextTestAccess::GetBooleanState(uiContext, MakeUiId("keyboard-toggle"));
}

[[nodiscard]] bool TestKeyboardActivationIsIgnoredDuringMouseCapture()
{
    using namespace greenfield;

    UiContext uiContext;
    uiContext.RequestFocus("captured-checkbox");
    uiContext.BeginFrame(MakeLayout(), InputState{.mousePosition = Vec2{20.0f, 30.0f}, .leftMouseButtonDown = true, .leftMouseButtonPressed = true});
    const bool changedOnPress =
        uiContext.Checkbox("captured-checkbox", Rect{.position = Vec2{10.0f, 20.0f}, .size = Vec2{140.0f, 36.0f}});
    const auto& pressCommands = uiContext.EndFrame();
    if (changedOnPress || pressCommands.Size() != 3U)
    {
        return false;
    }

    uiContext.BeginFrame(MakeLayout(), InputState{.mousePosition = Vec2{20.0f, 30.0f}, .leftMouseButtonDown = true, .enterPressed = true, .spacePressed = true});
    const bool changedDuringCapture =
        uiContext.Checkbox("captured-checkbox", Rect{.position = Vec2{10.0f, 20.0f}, .size = Vec2{140.0f, 36.0f}});
    const auto& dragCommands = uiContext.EndFrame();

    return !changedDuringCapture && dragCommands.Size() == 3U &&
           !UiContextTestAccess::GetBooleanState(uiContext, MakeUiId("captured-checkbox"));
}

[[nodiscard]] bool TestKeyboardActivationKeepsRendererNeutralCommands()
{
    using namespace greenfield;

    UiContext uiContext;
    uiContext.RequestFocus("neutral-checkbox");
    uiContext.BeginFrame(MakeLayout(), InputState{.enterPressed = true});
    const bool changed =
        uiContext.Checkbox("neutral-checkbox", Rect{.position = Vec2{10.0f, 20.0f}, .size = Vec2{140.0f, 36.0f}});
    const auto& commands = uiContext.EndFrame();

    return changed && commands.Size() == 4U && commands.Commands()[0].type == RenderCommandType::FillRectangle &&
           commands.Commands()[1].type == RenderCommandType::FillRectangle &&
           commands.Commands()[2].type == RenderCommandType::FillRectangle &&
           commands.Commands()[3].type == RenderCommandType::DrawText;
}

[[nodiscard]] bool TestFocusedSliderRightArrowIncreasesValue()
{
    using namespace greenfield;

    UiContext uiContext;
    uiContext.RequestFocus("keyboard-slider");
    uiContext.BeginFrame(MakeLayout(), InputState{.rightArrowPressed = true});
    const bool changed =
        uiContext.Slider("keyboard-slider", Rect{.position = Vec2{10.0f, 20.0f}, .size = Vec2{180.0f, 36.0f}}, 0.0f, 10.0f);
    const auto& commands = uiContext.EndFrame();

    return changed && commands.Size() == 5U &&
           UiContextTestAccess::GetNumericState(uiContext, MakeUiId("keyboard-slider"), 0.0f) == 0.5f;
}

[[nodiscard]] bool TestFocusedSliderLeftArrowDecreasesValue()
{
    using namespace greenfield;

    UiContext uiContext;
    UiContextTestAccess::SetNumericState(uiContext, MakeUiId("keyboard-slider"), 6.0f);
    uiContext.RequestFocus("keyboard-slider");
    uiContext.BeginFrame(MakeLayout(), InputState{.leftArrowPressed = true});
    const bool changed =
        uiContext.Slider("keyboard-slider", Rect{.position = Vec2{10.0f, 20.0f}, .size = Vec2{180.0f, 36.0f}}, 0.0f, 10.0f);
    const auto& commands = uiContext.EndFrame();

    return changed && commands.Size() == 5U &&
           UiContextTestAccess::GetNumericState(uiContext, MakeUiId("keyboard-slider"), 0.0f) == 5.5f;
}

[[nodiscard]] bool TestUnfocusedSliderIgnoresArrowKeys()
{
    using namespace greenfield;

    UiContext uiContext;
    UiContextTestAccess::SetNumericState(uiContext, MakeUiId("keyboard-slider"), 4.0f);
    uiContext.RequestFocus("other-control");
    uiContext.BeginFrame(MakeLayout(), InputState{.leftArrowPressed = true, .rightArrowPressed = true});
    const bool changed =
        uiContext.Slider("keyboard-slider", Rect{.position = Vec2{10.0f, 20.0f}, .size = Vec2{180.0f, 36.0f}}, 0.0f, 10.0f);
    const auto& commands = uiContext.EndFrame();

    return !changed && commands.Size() == 4U &&
           UiContextTestAccess::GetNumericState(uiContext, MakeUiId("keyboard-slider"), 0.0f) == 4.0f;
}

[[nodiscard]] bool TestFocusedSliderRightArrowClampsAtMaximum()
{
    using namespace greenfield;

    UiContext uiContext;
    UiContextTestAccess::SetNumericState(uiContext, MakeUiId("keyboard-slider"), 9.8f);
    uiContext.RequestFocus("keyboard-slider");
    uiContext.BeginFrame(MakeLayout(), InputState{.rightArrowPressed = true});
    const bool changed =
        uiContext.Slider("keyboard-slider", Rect{.position = Vec2{10.0f, 20.0f}, .size = Vec2{180.0f, 36.0f}}, 0.0f, 10.0f);
    const auto& commands = uiContext.EndFrame();

    return changed && commands.Size() == 5U &&
           UiContextTestAccess::GetNumericState(uiContext, MakeUiId("keyboard-slider"), 0.0f) == 10.0f;
}

[[nodiscard]] bool TestFocusedSliderLeftArrowClampsAtMinimum()
{
    using namespace greenfield;

    UiContext uiContext;
    UiContextTestAccess::SetNumericState(uiContext, MakeUiId("keyboard-slider"), 0.2f);
    uiContext.RequestFocus("keyboard-slider");
    uiContext.BeginFrame(MakeLayout(), InputState{.leftArrowPressed = true});
    const bool changed =
        uiContext.Slider("keyboard-slider", Rect{.position = Vec2{10.0f, 20.0f}, .size = Vec2{180.0f, 36.0f}}, 0.0f, 10.0f);
    const auto& commands = uiContext.EndFrame();

    return changed && commands.Size() == 5U &&
           UiContextTestAccess::GetNumericState(uiContext, MakeUiId("keyboard-slider"), 0.0f) == 0.0f;
}

[[nodiscard]] bool TestFocusedSliderKeyboardAdjustmentHandlesReversedRange()
{
    using namespace greenfield;

    UiContext uiContext;
    UiContextTestAccess::SetNumericState(uiContext, MakeUiId("keyboard-slider"), 6.0f);
    uiContext.RequestFocus("keyboard-slider");
    uiContext.BeginFrame(MakeLayout(), InputState{.rightArrowPressed = true});
    const bool changed =
        uiContext.Slider("keyboard-slider", Rect{.position = Vec2{10.0f, 20.0f}, .size = Vec2{180.0f, 36.0f}}, 10.0f, 0.0f);
    const auto& commands = uiContext.EndFrame();

    return changed && commands.Size() == 5U &&
           UiContextTestAccess::GetNumericState(uiContext, MakeUiId("keyboard-slider"), 0.0f) == 6.5f;
}

[[nodiscard]] bool TestFocusedSliderKeyboardAdjustmentDegenerateRangeReturnsFalse()
{
    using namespace greenfield;

    UiContext uiContext;
    uiContext.RequestFocus("keyboard-slider");
    uiContext.BeginFrame(MakeLayout(), InputState{.leftArrowPressed = true, .rightArrowPressed = true});
    const bool changed =
        uiContext.Slider("keyboard-slider", Rect{.position = Vec2{10.0f, 20.0f}, .size = Vec2{180.0f, 36.0f}}, 3.0f, 3.0f);
    const auto& commands = uiContext.EndFrame();

    return !changed && commands.Size() == 5U &&
           UiContextTestAccess::GetNumericState(uiContext, MakeUiId("keyboard-slider"), 0.0f) == 3.0f;
}

[[nodiscard]] bool TestFocusedSliderKeyboardAdjustmentReturnsTrueOnlyWhenValueChanges()
{
    using namespace greenfield;

    UiContext uiContext;
    UiContextTestAccess::SetNumericState(uiContext, MakeUiId("keyboard-slider"), 10.0f);
    uiContext.RequestFocus("keyboard-slider");
    uiContext.BeginFrame(MakeLayout(), InputState{.rightArrowPressed = true});
    const bool changed =
        uiContext.Slider("keyboard-slider", Rect{.position = Vec2{10.0f, 20.0f}, .size = Vec2{180.0f, 36.0f}}, 0.0f, 10.0f);
    const auto& commands = uiContext.EndFrame();

    return !changed && commands.Size() == 5U &&
           UiContextTestAccess::GetNumericState(uiContext, MakeUiId("keyboard-slider"), 0.0f) == 10.0f;
}

[[nodiscard]] bool TestFocusedSliderKeyboardAdjustmentIsIgnoredDuringMouseCapture()
{
    using namespace greenfield;

    UiContext uiContext;
    uiContext.RequestFocus("keyboard-slider");
    uiContext.BeginFrame(MakeLayout(), InputState{.mousePosition = Vec2{10.0f, 30.0f}, .leftMouseButtonDown = true, .leftMouseButtonPressed = true});
    const bool changedOnPress =
        uiContext.Slider("keyboard-slider", Rect{.position = Vec2{10.0f, 20.0f}, .size = Vec2{180.0f, 36.0f}}, 0.0f, 1.0f);
    const auto& pressCommands = uiContext.EndFrame();
    if (changedOnPress || pressCommands.Size() != 5U)
    {
        return false;
    }

    uiContext.BeginFrame(MakeLayout(),
                         InputState{.mousePosition = Vec2{130.0f, 30.0f}, .leftMouseButtonDown = true, .rightArrowPressed = true});
    const bool changedDuringCapture =
        uiContext.Slider("keyboard-slider", Rect{.position = Vec2{10.0f, 20.0f}, .size = Vec2{180.0f, 36.0f}}, 0.0f, 1.0f);
    const auto& dragCommands = uiContext.EndFrame();

    return changedDuringCapture && dragCommands.Size() == 5U &&
           UiContextTestAccess::GetNumericState(uiContext, MakeUiId("keyboard-slider"), 0.0f) == 1.0f;
}

[[nodiscard]] bool TestTextStateDefaultsToEmpty()
{
    using namespace greenfield;

    const UiContext uiContext;
    return UiContextTestAccess::GetTextState(uiContext, MakeUiId("missing-text-control")).empty();
}

[[nodiscard]] bool TestTextStatePersistsAcrossFrames()
{
    using namespace greenfield;

    UiContext uiContext;
    const UiId controlId = MakeUiId("persistent-text-control");
    UiContextTestAccess::SetTextState(uiContext, controlId, "seed");

    uiContext.BeginFrame(MakeLayout());
    const bool changed = uiContext.TextInput("persistent-text-control",
                                             Rect{.position = Vec2{10.0f, 20.0f}, .size = Vec2{180.0f, 36.0f}});
    const auto& firstCommands = uiContext.EndFrame();
    if (changed || firstCommands.Size() != 2U ||
        UiContextTestAccess::GetTextState(uiContext, controlId) != "seed")
    {
        return false;
    }

    uiContext.BeginFrame(MakeLayout());
    const bool secondFrameChanged = uiContext.TextInput("persistent-text-control",
                                                        Rect{.position = Vec2{10.0f, 20.0f}, .size = Vec2{180.0f, 36.0f}});
    const auto& secondCommands = uiContext.EndFrame();
    return !secondFrameChanged && secondCommands.Size() == 2U &&
           UiContextTestAccess::GetTextState(uiContext, controlId) == "seed";
}

[[nodiscard]] bool TestTextStateIsIndependentPerUiId()
{
    using namespace greenfield;

    UiContext uiContext;
    UiContextTestAccess::SetTextState(uiContext, MakeUiId("first-text-control"), "alpha");

    uiContext.BeginFrame(MakeLayout());
    const bool firstChanged =
        uiContext.TextInput("first-text-control", Rect{.position = Vec2{10.0f, 20.0f}, .size = Vec2{180.0f, 36.0f}});
    const bool secondChanged =
        uiContext.TextInput("second-text-control", Rect{.position = Vec2{10.0f, 64.0f}, .size = Vec2{180.0f, 36.0f}});
    const auto& commands = uiContext.EndFrame();

    return !firstChanged && !secondChanged && commands.Size() == 3U &&
           UiContextTestAccess::GetTextState(uiContext, MakeUiId("first-text-control")) == "alpha" &&
           UiContextTestAccess::GetTextState(uiContext, MakeUiId("second-text-control")).empty();
}

[[nodiscard]] bool TestFocusedTextInputAppendsCommittedText()
{
    using namespace greenfield;

    UiContext uiContext;
    uiContext.RequestFocus("focused-text-input");
    uiContext.BeginFrame(MakeLayout(), InputState{.committedText = "abc"});
    const bool changed =
        uiContext.TextInput("focused-text-input", Rect{.position = Vec2{10.0f, 20.0f}, .size = Vec2{180.0f, 36.0f}});
    const auto& commands = uiContext.EndFrame();

    return changed && commands.Size() == 3U &&
           UiContextTestAccess::GetTextState(uiContext, MakeUiId("focused-text-input")) == "abc" &&
           commands.Commands()[2].type == RenderCommandType::DrawText &&
           commands.Commands()[2].text == "abc";
}

[[nodiscard]] bool TestFocusedTextInputBackspaceRemovesLastCodeUnit()
{
    using namespace greenfield;

    UiContext uiContext;
    UiContextTestAccess::SetTextState(uiContext, MakeUiId("backspace-text-input"), "seed");
    uiContext.RequestFocus("backspace-text-input");
    uiContext.BeginFrame(MakeLayout(), InputState{.backspacePressed = true});
    const bool changed =
        uiContext.TextInput("backspace-text-input", Rect{.position = Vec2{10.0f, 20.0f}, .size = Vec2{180.0f, 36.0f}});
    const auto& commands = uiContext.EndFrame();

    return changed && commands.Size() == 3U &&
           UiContextTestAccess::GetTextState(uiContext, MakeUiId("backspace-text-input")) == "see" &&
           commands.Commands()[2].text == "see";
}

[[nodiscard]] bool TestTextInputReturnsTrueOnlyWhenTextChanges()
{
    using namespace greenfield;

    UiContext uiContext;
    uiContext.RequestFocus("steady-text-input");

    uiContext.BeginFrame(MakeLayout());
    const bool unchanged = uiContext.TextInput("steady-text-input",
                                               Rect{.position = Vec2{10.0f, 20.0f}, .size = Vec2{180.0f, 36.0f}});
    const auto& unchangedCommands = uiContext.EndFrame();
    if (unchanged || unchangedCommands.Size() != 2U)
    {
        return false;
    }

    uiContext.BeginFrame(MakeLayout(), InputState{.committedText = "x"});
    const bool changed = uiContext.TextInput("steady-text-input",
                                             Rect{.position = Vec2{10.0f, 20.0f}, .size = Vec2{180.0f, 36.0f}});
    const auto& changedCommands = uiContext.EndFrame();
    if (!changed || changedCommands.Size() != 3U)
    {
        return false;
    }

    uiContext.BeginFrame(MakeLayout(), InputState{.backspacePressed = true});
    const bool deleted = uiContext.TextInput("steady-text-input",
                                             Rect{.position = Vec2{10.0f, 20.0f}, .size = Vec2{180.0f, 36.0f}});
    const auto& deletedCommands = uiContext.EndFrame();
    if (!deleted || deletedCommands.Size() != 2U)
    {
        return false;
    }

    uiContext.BeginFrame(MakeLayout(), InputState{.backspacePressed = true});
    const bool unchangedBackspace = uiContext.TextInput("steady-text-input",
                                                        Rect{.position = Vec2{10.0f, 20.0f}, .size = Vec2{180.0f, 36.0f}});
    const auto& finalCommands = uiContext.EndFrame();
    return !unchangedBackspace && finalCommands.Size() == 2U &&
           UiContextTestAccess::GetTextState(uiContext, MakeUiId("steady-text-input")).empty();
}

[[nodiscard]] bool TestTextInputNetNoOpDoesNotReportChange()
{
    using namespace greenfield;

    UiContext uiContext;
    uiContext.RequestFocus("net-no-op-text-input");

    uiContext.BeginFrame(MakeLayout(), InputState{.backspacePressed = true, .committedText = "x"});
    const bool unchanged = uiContext.TextInput("net-no-op-text-input",
                                               Rect{.position = Vec2{10.0f, 20.0f}, .size = Vec2{180.0f, 36.0f}});
    const auto& firstCommands = uiContext.EndFrame();
    if (unchanged || firstCommands.Size() != 2U ||
        !UiContextTestAccess::GetTextState(uiContext, MakeUiId("net-no-op-text-input")).empty())
    {
        return false;
    }

    UiContextTestAccess::SetTextState(uiContext, MakeUiId("net-no-op-text-input"), "seed");
    uiContext.BeginFrame(MakeLayout(), InputState{.backspacePressed = true, .committedText = "x"});
    const bool seededUnchanged =
        uiContext.TextInput("net-no-op-text-input", Rect{.position = Vec2{10.0f, 20.0f}, .size = Vec2{180.0f, 36.0f}});
    const auto& secondCommands = uiContext.EndFrame();
    return !seededUnchanged && secondCommands.Size() == 3U &&
           UiContextTestAccess::GetTextState(uiContext, MakeUiId("net-no-op-text-input")) == "seed" &&
           secondCommands.Commands()[2].text == "seed";
}

[[nodiscard]] bool TestUnfocusedTextInputIgnoresCommittedTextAndBackspace()
{
    using namespace greenfield;

    UiContext uiContext;
    UiContextTestAccess::SetTextState(uiContext, MakeUiId("quiet-text-input"), "seed");
    uiContext.RequestFocus("other-control");
    uiContext.BeginFrame(MakeLayout(), InputState{.backspacePressed = true, .committedText = "abc"});
    const bool changed =
        uiContext.TextInput("quiet-text-input", Rect{.position = Vec2{10.0f, 20.0f}, .size = Vec2{180.0f, 36.0f}});
    const auto& commands = uiContext.EndFrame();

    return !changed && commands.Size() == 2U &&
           UiContextTestAccess::GetTextState(uiContext, MakeUiId("quiet-text-input")) == "seed" &&
           commands.Commands()[1].text == "seed";
}

[[nodiscard]] bool TestTextInputClickFocusesField()
{
    using namespace greenfield;

    UiContext uiContext;
    const Rect bounds{
        .position = Vec2{10.0f, 20.0f},
        .size = Vec2{180.0f, 36.0f},
    };

    uiContext.BeginFrame(MakeLayout(), InputState{
                                           .mousePosition = Vec2{20.0f, 30.0f},
                                           .leftMouseButtonDown = true,
                                           .leftMouseButtonPressed = true,
                                       });
    const bool changedOnPress = uiContext.TextInput("click-focus-text-input", bounds);
    const auto& pressCommands = uiContext.EndFrame();
    if (changedOnPress || pressCommands.Size() != 2U || !uiContext.HasFocus("click-focus-text-input"))
    {
        return false;
    }

    uiContext.BeginFrame(MakeLayout(), InputState{.committedText = "z"});
    const bool changedOnType = uiContext.TextInput("click-focus-text-input", bounds);
    const auto& typeCommands = uiContext.EndFrame();
    return changedOnType && typeCommands.Size() == 3U &&
           UiContextTestAccess::GetTextState(uiContext, MakeUiId("click-focus-text-input")) == "z";
}

[[nodiscard]] bool TestTextInputIgnoresEnterAndSpaceActivation()
{
    using namespace greenfield;

    UiContext uiContext;
    uiContext.RequestFocus("focused-text-input");

    uiContext.BeginFrame(MakeLayout(), InputState{.enterPressed = true, .spacePressed = true});
    const bool changed =
        uiContext.TextInput("focused-text-input", Rect{.position = Vec2{10.0f, 20.0f}, .size = Vec2{180.0f, 36.0f}});
    const auto& commands = uiContext.EndFrame();
    return !changed && commands.Size() == 2U && uiContext.HasFocus("focused-text-input") &&
           UiContextTestAccess::GetTextState(uiContext, MakeUiId("focused-text-input")).empty();
}

[[nodiscard]] bool TestTextInputTabTraversalPreservesFocusOrder()
{
    using namespace greenfield;

    UiContext uiContext;
    uiContext.BeginFrame(MakeLayout(), InputState{.tabPressed = true});
    const bool buttonClicked = uiContext.Button("first-focusable",
                                                Rect{.position = Vec2{10.0f, 20.0f}, .size = Vec2{120.0f, 36.0f}});
    const bool textChanged = uiContext.TextInput("second-focusable",
                                                 Rect{.position = Vec2{10.0f, 64.0f}, .size = Vec2{180.0f, 36.0f}});
    const auto& firstCommands = uiContext.EndFrame();
    if (buttonClicked || textChanged || firstCommands.Size() != 3U || !uiContext.HasFocus("first-focusable"))
    {
        return false;
    }

    uiContext.BeginFrame(MakeLayout(), InputState{.tabPressed = true});
    const bool secondButtonClicked = uiContext.Button("first-focusable",
                                                      Rect{.position = Vec2{10.0f, 20.0f}, .size = Vec2{120.0f, 36.0f}});
    const bool secondTextChanged = uiContext.TextInput("second-focusable",
                                                       Rect{.position = Vec2{10.0f, 64.0f}, .size = Vec2{180.0f, 36.0f}});
    const auto& secondCommands = uiContext.EndFrame();
    return !secondButtonClicked && !secondTextChanged && secondCommands.Size() == 4U &&
           uiContext.HasFocus("second-focusable");
}

[[nodiscard]] bool TestShiftTabMovesFocusBackwardFromTextInput()
{
    using namespace greenfield;

    UiContext uiContext;
    uiContext.RequestFocus("second-focusable");
    uiContext.BeginFrame(MakeLayout(), InputState{.shiftTabPressed = true});
    const bool buttonClicked = uiContext.Button("first-focusable",
                                                Rect{.position = Vec2{10.0f, 20.0f}, .size = Vec2{120.0f, 36.0f}});
    const bool textChanged = uiContext.TextInput("second-focusable",
                                                 Rect{.position = Vec2{10.0f, 64.0f}, .size = Vec2{180.0f, 36.0f}});
    const auto& commands = uiContext.EndFrame();
    return !buttonClicked && !textChanged && commands.Size() == 4U && uiContext.HasFocus("first-focusable");
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

[[nodiscard]] bool TestNumericStateDefaultsToProvidedValue()
{
    using namespace greenfield;

    const UiContext uiContext;
    return UiContextTestAccess::GetNumericState(uiContext, MakeUiId("missing-numeric-control"), 0.35f) == 0.35f;
}

[[nodiscard]] bool TestNumericStatePersistsAcrossFrames()
{
    using namespace greenfield;

    UiContext uiContext;
    const UiId controlId = MakeUiId("persistent-numeric-control");
    UiContextTestAccess::SetNumericState(uiContext, controlId, 0.75f);

    uiContext.BeginFrame(MakeLayout());
    const auto& firstCommands = uiContext.EndFrame();
    if (!firstCommands.IsEmpty() || UiContextTestAccess::GetNumericState(uiContext, controlId, 0.0f) != 0.75f)
    {
        return false;
    }

    uiContext.BeginFrame(MakeLayout());
    const auto& secondCommands = uiContext.EndFrame();
    return secondCommands.IsEmpty() && UiContextTestAccess::GetNumericState(uiContext, controlId, 0.0f) == 0.75f;
}

[[nodiscard]] bool TestNumericStateIsIndependentPerUiId()
{
    using namespace greenfield;

    UiContext uiContext;
    const UiId firstControlId = MakeUiId("first-numeric-control");
    const UiId secondControlId = MakeUiId("second-numeric-control");
    UiContextTestAccess::SetNumericState(uiContext, firstControlId, 0.25f);
    UiContextTestAccess::SetNumericState(uiContext, secondControlId, 0.80f);

    return UiContextTestAccess::GetNumericState(uiContext, firstControlId, 0.0f) == 0.25f &&
           UiContextTestAccess::GetNumericState(uiContext, secondControlId, 0.0f) == 0.80f;
}

[[nodiscard]] bool TestNumericStateCanBeOverwritten()
{
    using namespace greenfield;

    UiContext uiContext;
    const UiId controlId = MakeUiId("overwritten-numeric-control");
    UiContextTestAccess::SetNumericState(uiContext, controlId, 0.20f);
    UiContextTestAccess::SetNumericState(uiContext, controlId, 0.90f);

    return UiContextTestAccess::GetNumericState(uiContext, controlId, 0.0f) == 0.90f;
}

[[nodiscard]] bool TestClampedNumericStateHandlesRanges()
{
    using namespace greenfield;

    UiContext uiContext;
    const UiId belowControlId = MakeUiId("below-range-numeric-control");
    const UiId aboveControlId = MakeUiId("above-range-numeric-control");
    const UiId reversedRangeControlId = MakeUiId("reversed-range-numeric-control");
    const UiId degenerateRangeControlId = MakeUiId("degenerate-range-numeric-control");

    UiContextTestAccess::SetNumericState(uiContext, belowControlId, -0.25f);
    UiContextTestAccess::SetNumericState(uiContext, aboveControlId, 1.25f);
    UiContextTestAccess::SetNumericState(uiContext, reversedRangeControlId, 12.0f);
    UiContextTestAccess::SetNumericState(uiContext, degenerateRangeControlId, 8.0f);

    return UiContextTestAccess::GetClampedNumericState(uiContext, belowControlId, 0.0f, 0.0f, 1.0f) == 0.0f &&
           UiContextTestAccess::GetNumericState(uiContext, belowControlId, 0.5f) == 0.0f &&
           UiContextTestAccess::GetClampedNumericState(uiContext, aboveControlId, 0.0f, 0.0f, 1.0f) == 1.0f &&
           UiContextTestAccess::GetNumericState(uiContext, aboveControlId, 0.5f) == 1.0f &&
           UiContextTestAccess::GetClampedNumericState(uiContext, reversedRangeControlId, 0.0f, 10.0f, 5.0f) == 10.0f &&
           UiContextTestAccess::GetNumericState(uiContext, reversedRangeControlId, 0.0f) == 10.0f &&
           UiContextTestAccess::GetClampedNumericState(uiContext, degenerateRangeControlId, 0.0f, 3.0f, 3.0f) == 3.0f &&
           UiContextTestAccess::GetNumericState(uiContext, degenerateRangeControlId, 0.0f) == 3.0f;
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
        !TestOverlappingButtonPressIsConsumedByActiveControl() || !TestCheckboxDefaultsUnchecked() ||
        !TestCheckboxClickTogglesCheckedState() || !TestCheckboxCheckedStatePersistsAcrossFrames() ||
        !TestCheckboxSecondClickTogglesBackOff() || !TestCheckboxStateIsIndependentPerUiId() ||
        !TestOverlappingCheckboxAndButtonConsumeOneGesture() ||
        !TestOverlappingCheckboxesConsumeOneGesture() || !TestLayoutCheckboxAdvancesLayout() ||
        !TestExplicitBoundsCheckboxDoesNotAdvanceLayout() ||
        !TestCheckboxRenderCommandsIncludeBoxIndicatorAndLabel() || !TestToggleDefaultsOff() ||
        !TestToggleClickTogglesOn() || !TestToggleOnStatePersistsAcrossFrames() ||
        !TestToggleSecondClickTogglesBackOff() || !TestToggleStateIsIndependentPerUiId() ||
        !TestToggleAndCheckboxStatesAreIndependentForDifferentUiIds() ||
        !TestToggleAndCheckboxShareStateForMatchingUiId() ||
        !TestOverlappingToggleAndButtonConsumeOneGesture() ||
        !TestOverlappingToggleAndCheckboxConsumeOneGesture() ||
        !TestOverlappingTogglesConsumeOneGesture() || !TestLayoutToggleAdvancesLayout() ||
        !TestExplicitBoundsToggleDoesNotAdvanceLayout() ||
        !TestToggleRenderCommandsIncludeTrackKnobAndLabel() || !TestSliderDefaultsToMinimumValue() ||
        !TestSliderValuePersistsAcrossFrames() || !TestSliderClickChangesValueOnlyOnChangedFrame() ||
        !TestSliderClampsClickAfterTrackToMaximum() ||
        !TestSliderHandlesReversedAndDegenerateRanges() ||
        !TestSliderStateIsIndependentPerUiId() || !TestLayoutSliderAdvancesLayout() ||
        !TestExplicitBoundsSliderDoesNotAdvanceLayout() ||
        !TestSliderRenderCommandsIncludeTrackFillThumbAndLabel() ||
        !TestSliderDragUpdatesWhileCaptured() ||
        !TestOverlappingSliderAndButtonConsumeOneGesture() || !TestFocusDefaultsToEmpty() ||
        !TestControlStylesDefaultToOuterRingFocusConfiguration() ||
        !TestControlStylesPreserveCustomFocusConfiguration() ||
        !TestFocusedButtonEmitsOuterFocusRingCommand() ||
        !TestFocusedCheckboxEmitsFocusRingAroundBox() ||
        !TestFocusedToggleEmitsFocusRingAroundTrack() ||
        !TestFocusedSliderEmitsFocusRingAroundTrack() ||
        !TestFocusVisualKindNoneSuppressesFocusRing() ||
        !TestCustomFocusStyleControlsFocusRingGeometry() ||
        !TestRequestFocusByIdAndName() || !TestClearFocusRemovesFocus() ||
        !TestFocusPersistsAcrossFrames() ||
        !TestTabFocusesFirstFocusableControlWhenNothingIsFocused() ||
        !TestShiftTabFocusesLastFocusableControlWhenNothingIsFocused() ||
        !TestTabMovesFocusForwardInEncounterOrder() ||
        !TestShiftTabMovesFocusBackwardInEncounterOrder() ||
        !TestFocusPersistsWhenFocusedControlIsStillRegistered() ||
        !TestStaleFocusedControlTraversalStartsFromFrameEdge() ||
        !TestKeyboardTraversalDoesNotActivateControls() || !TestFocusedButtonActivatesOnEnter() ||
        !TestFocusedButtonActivatesOnSpace() || !TestUnfocusedButtonIgnoresKeyboardActivation() ||
        !TestFocusedCheckboxTogglesOnEnter() || !TestFocusedCheckboxTogglesOnSpace() ||
        !TestUnfocusedCheckboxIgnoresKeyboardActivation() || !TestFocusedToggleTogglesOnEnter() ||
        !TestFocusedToggleTogglesOnSpace() || !TestUnfocusedToggleIgnoresKeyboardActivation() ||
        !TestKeyboardActivationIsIgnoredDuringMouseCapture() ||
        !TestKeyboardActivationKeepsRendererNeutralCommands() ||
        !TestFocusedSliderRightArrowIncreasesValue() ||
        !TestFocusedSliderLeftArrowDecreasesValue() ||
        !TestUnfocusedSliderIgnoresArrowKeys() ||
        !TestFocusedSliderRightArrowClampsAtMaximum() ||
        !TestFocusedSliderLeftArrowClampsAtMinimum() ||
        !TestFocusedSliderKeyboardAdjustmentHandlesReversedRange() ||
        !TestFocusedSliderKeyboardAdjustmentDegenerateRangeReturnsFalse() ||
        !TestFocusedSliderKeyboardAdjustmentReturnsTrueOnlyWhenValueChanges() ||
        !TestFocusedSliderKeyboardAdjustmentIsIgnoredDuringMouseCapture() ||
        !TestTextStateDefaultsToEmpty() || !TestTextStatePersistsAcrossFrames() ||
        !TestTextStateIsIndependentPerUiId() || !TestFocusedTextInputAppendsCommittedText() ||
        !TestFocusedTextInputBackspaceRemovesLastCodeUnit() ||
        !TestTextInputReturnsTrueOnlyWhenTextChanges() ||
        !TestTextInputNetNoOpDoesNotReportChange() ||
        !TestUnfocusedTextInputIgnoresCommittedTextAndBackspace() ||
        !TestTextInputClickFocusesField() || !TestTextInputIgnoresEnterAndSpaceActivation() ||
        !TestTextInputTabTraversalPreservesFocusOrder() || !TestShiftTabMovesFocusBackwardFromTextInput() ||
        !TestBooleanStatePersistsAcrossFrames() ||
        !TestBooleanStateIsIndependentPerUiId() || !TestNumericStateDefaultsToProvidedValue() ||
        !TestNumericStatePersistsAcrossFrames() || !TestNumericStateIsIndependentPerUiId() ||
        !TestNumericStateCanBeOverwritten() || !TestClampedNumericStateHandlesRanges() ||
        !TestFocusIsIndependentFromActiveControlAndScrollState())
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

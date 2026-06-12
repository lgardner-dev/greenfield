#include <chrono>
#include <exception>
#include <iostream>
#include <thread>

#include "engine/core/Color.h"
#include "engine/core/Rect.h"
#include "engine/core/Vec2.h"
#include "engine/platform/SdlStartupPresenter.h"
#include "engine/platform/SdlWindow.h"
#include "engine/render/webgpu/WebGpuContext.h"
#include "engine/render/webgpu/WebGpuRenderer.h"
#include "engine/ui/Layout.h"
#include "engine/ui/Style.h"
#include "engine/ui/UiContext.h"

namespace
{

using namespace greenfield;

ButtonStyle GetCardButtonStyle(bool isSelected)
{
    if (isSelected)
    {
        return ButtonStyle{
            .normal = Color{0.18f, 0.64f, 0.48f, 0.88f},
            .hovered = Color{0.26f, 0.76f, 0.56f, 0.94f},
            .pressed = Color{0.12f, 0.46f, 0.34f, 0.96f},
            .border = Color{0.58f, 0.96f, 0.78f, 0.70f},
            .cornerRadius = 18.0f,
            .borderThickness = 1.5f,
        };
    }

    return ButtonStyle{
        .normal = Color{0.21f, 0.29f, 0.42f, 0.86f},
        .hovered = Color{0.28f, 0.44f, 0.64f, 0.94f},
        .pressed = Color{0.15f, 0.24f, 0.36f, 0.96f},
        .border = Color{0.56f, 0.68f, 0.86f, 0.42f},
        .cornerRadius = 18.0f,
        .borderThickness = 1.5f,
    };
}

RectangleStyle GetPanelStyle(const Style& style)
{
    return RectangleStyle{
        .fillColor = style.panelBackground,
        .cornerRadius = style.panelCornerRadius,
        .borderColor = style.panelBorder,
        .borderThickness = style.panelBorderThickness,
    };
}

void BuildSimpleUi(UiContext& uiContext, int windowWidth, int windowHeight, bool& firstSelected, bool& secondSelected,
                   bool& thirdSelected)
{
    const Style& style = uiContext.GetStyle();
    const auto rootPanelWidth = static_cast<float>(windowWidth) * 0.68f;
    const auto rootPanelHeight = static_cast<float>(windowHeight) * 0.58f;
    const auto rootPanel = Rect{
        .position = Vec2{32.0f, 32.0f},
        .size = Vec2{rootPanelWidth, rootPanelHeight},
    };
    uiContext.Panel(rootPanel, GetPanelStyle(style));

    uiContext.Panel(Rect{
                        .position = Vec2{rootPanel.position.x + rootPanel.size.x - 176.0f, rootPanel.position.y + 24.0f},
                        .size = Vec2{128.0f, 128.0f},
                    },
                    RectangleStyle{
                        .fillColor = Color{0.34f, 0.60f, 0.95f, 0.14f},
                        .cornerRadius = 32.0f,
                        .borderColor = Color{0.60f, 0.76f, 0.95f, 0.22f},
                        .borderThickness = 1.0f,
                    });

    uiContext.BeginColumn(LayoutContainer{
        .bounds = rootPanel,
        .padding = 24.0f,
        .gap = 18.0f,
    });

    uiContext.Panel(RectangleStyle{
                        .fillColor = style.accent,
                        .cornerRadius = 10.0f,
                        .borderColor = Color{0.70f, 0.84f, 1.0f, 0.35f},
                        .borderThickness = 1.0f,
                    },
                    Vec2{rootPanel.size.x * 0.34f, 26.0f});

    const Rect cardRowBounds = uiContext.Panel(RectangleStyle{
                                                   .fillColor = Color{0.12f, 0.15f, 0.20f, 0.52f},
                                                   .cornerRadius = 22.0f,
                                                   .borderColor = Color{0.48f, 0.56f, 0.68f, 0.32f},
                                                   .borderThickness = 1.0f,
                                               },
                                               Vec2{0.0f, 126.0f});
    const float cardRowPadding = 12.0f;
    const float cardGap = 18.0f;
    const float cardWidth = (cardRowBounds.size.x - cardRowPadding * 2.0f - cardGap * 2.0f) / 3.0f;

    uiContext.BeginRow(LayoutContainer{
        .bounds = cardRowBounds,
        .padding = cardRowPadding,
        .gap = cardGap,
        .itemSize = Vec2{cardWidth, 102.0f},
    });

    if (uiContext.Button("first-card", GetCardButtonStyle(firstSelected)))
    {
        firstSelected = !firstSelected;
    }

    if (uiContext.Button("second-card", GetCardButtonStyle(secondSelected)))
    {
        secondSelected = !secondSelected;
    }

    if (uiContext.Button("third-card", GetCardButtonStyle(thirdSelected)))
    {
        thirdSelected = !thirdSelected;
    }

    uiContext.EndRow();

    const Rect detailPanel = uiContext.Panel(RectangleStyle{
                                                 .fillColor = Color{0.11f, 0.13f, 0.17f, 0.76f},
                                                 .cornerRadius = 18.0f,
                                                 .borderColor = Color{0.45f, 0.54f, 0.66f, 0.38f},
                                                 .borderThickness = 1.0f,
                                             },
                                             Vec2{0.0f, 138.0f});
    uiContext.BeginColumn(LayoutContainer{
        .bounds = detailPanel,
        .padding = 16.0f,
        .gap = 12.0f,
        .itemSize = Vec2{0.0f, 28.0f},
    });

    uiContext.Panel(RectangleStyle{
                        .fillColor = Color{0.24f, 0.78f, 0.56f, 0.88f},
                        .cornerRadius = 9.0f,
                        .borderColor = Color{0.58f, 0.96f, 0.78f, 0.42f},
                        .borderThickness = 1.0f,
                    },
                    Vec2{detailPanel.size.x * 0.58f, 28.0f});
    uiContext.Panel(RectangleStyle{
        .fillColor = Color{0.26f, 0.38f, 0.58f, 0.72f},
        .cornerRadius = 9.0f,
        .borderColor = Color{0.54f, 0.68f, 0.88f, 0.30f},
        .borderThickness = 1.0f,
    });
    uiContext.AddSpacing(2.0f);
    uiContext.Panel(RectangleStyle{
                        .fillColor = Color{0.34f, 0.60f, 0.95f, 0.82f},
                        .cornerRadius = 9.0f,
                        .borderColor = Color{0.70f, 0.84f, 1.0f, 0.34f},
                        .borderThickness = 1.0f,
                    },
                    Vec2{detailPanel.size.x * 0.72f, 28.0f});
    uiContext.EndColumn();

    uiContext.EndColumn();
}

} // namespace

int main()
{
    try
    {
        std::cout << "Starting Greenfield sandbox\n";

        SdlWindow window{"Greenfield Sandbox", 1280, 720};
        {
            SdlStartupPresenter startupPresenter{window};
            startupPresenter.DrawFrame();

            window.PollEvents();
            if (window.ShouldClose())
            {
                return 0;
            }
        }

        WebGpuContext webGpuContext{window};
        WebGpuRenderer renderer{webGpuContext};
        UiContext uiContext;

        const Style style{};
        uiContext.SetStyle(style);
        bool firstSelected = false;
        bool secondSelected = false;
        bool thirdSelected = false;

        while (!window.ShouldClose())
        {
            window.PollEvents();
            if (window.ShouldClose())
            {
                break;
            }

            renderer.BeginFrame();

            const Layout layout{
                .bounds =
                    Rect{
                        .position = Vec2{0.0f, 0.0f},
                        .size = Vec2{static_cast<float>(window.GetWidth()), static_cast<float>(window.GetHeight())},
                    },
                .padding = 16.0f,
            };

            uiContext.BeginFrame(layout, window.GetInputState());
            BuildSimpleUi(
                uiContext, window.GetWidth(), window.GetHeight(), firstSelected, secondSelected, thirdSelected);

            const auto& renderCommands = uiContext.EndFrame();
            renderer.Submit(renderCommands);
            renderer.EndFrame();

            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }

        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Greenfield sandbox failed: " << exception.what() << '\n';
    }
    catch (...)
    {
        std::cerr << "Greenfield sandbox failed with an unknown error.\n";
    }

    return 1;
}

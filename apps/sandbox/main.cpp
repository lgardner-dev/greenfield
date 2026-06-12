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
            .normal = Color{0.24f, 0.78f, 0.56f, 1.0f},
            .hovered = Color{0.35f, 0.88f, 0.66f, 1.0f},
            .pressed = Color{0.16f, 0.56f, 0.40f, 1.0f},
            .cornerRadius = 8.0f,
        };
    }

    return ButtonStyle{
        .normal = Color{0.26f, 0.38f, 0.58f, 1.0f},
        .hovered = Color{0.34f, 0.58f, 0.86f, 1.0f},
        .pressed = Color{0.18f, 0.32f, 0.52f, 1.0f},
        .cornerRadius = 8.0f,
    };
}

void BuildSimpleUi(UiContext& uiContext, int windowWidth, int windowHeight, bool& firstSelected, bool& secondSelected,
                   bool& thirdSelected)
{
    const auto panelWidth = static_cast<float>(windowWidth) * 0.62f;
    const auto panelHeight = static_cast<float>(windowHeight) * 0.52f;
    const auto panel = Rect{
        .position = Vec2{32.0f, 32.0f},
        .size = Vec2{panelWidth, panelHeight},
    };
    uiContext.Panel(panel, uiContext.GetStyle().panelBackground, 12.0f);

    uiContext.BeginColumn(LayoutContainer{
        .bounds = panel,
        .padding = 24.0f,
        .gap = 20.0f,
    });

    uiContext.Panel(uiContext.GetStyle().accent, Vec2{panel.size.x * 0.42f, 28.0f});

    const Rect cardRowBounds = uiContext.Panel(Color{0.21f, 0.24f, 0.30f, 1.0f}, Vec2{0.0f, 108.0f}, 8.0f);
    const float cardRowPadding = 10.0f;
    const float cardGap = 18.0f;
    const float cardWidth = (cardRowBounds.size.x - (cardRowPadding * 2.0f) - (cardGap * 2.0f)) / 3.0f;

    uiContext.BeginRow(LayoutContainer{
        .bounds = cardRowBounds,
        .padding = cardRowPadding,
        .gap = cardGap,
        .itemSize = Vec2{cardWidth, 88.0f},
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

    const Rect detailPanel = uiContext.Panel(Color{0.15f, 0.17f, 0.21f, 1.0f}, Vec2{0.0f, 122.0f}, 8.0f);
    uiContext.BeginColumn(LayoutContainer{
        .bounds = detailPanel,
        .padding = 14.0f,
        .gap = 12.0f,
        .itemSize = Vec2{0.0f, 28.0f},
    });

    uiContext.Panel(Color{0.24f, 0.78f, 0.56f, 1.0f}, Vec2{detailPanel.size.x * 0.58f, 28.0f});
    uiContext.Panel(Color{0.26f, 0.38f, 0.58f, 1.0f});
    uiContext.AddSpacing(2.0f);
    uiContext.Panel(Color{0.34f, 0.60f, 0.95f, 1.0f}, Vec2{detailPanel.size.x * 0.72f, 28.0f});
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

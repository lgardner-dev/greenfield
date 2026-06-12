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

void BuildSimpleUi(UiContext& uiContext, int windowWidth, int windowHeight)
{
    const auto panelWidth = static_cast<float>(windowWidth) * 0.62f;
    const auto panelHeight = static_cast<float>(windowHeight) * 0.52f;
    const auto panel = Rect{
        .position = Vec2{32.0f, 32.0f},
        .size = Vec2{panelWidth, panelHeight},
    };
    uiContext.DrawFilledRectangle(panel, uiContext.GetStyle().panelBackground, 12.0f);

    const auto accentBar = Rect{
        .position = Vec2{48.0f, 48.0f},
        .size = Vec2{panel.size.x * 0.42f, 28.0f},
    };
    uiContext.DrawFilledRectangle(accentBar, uiContext.GetStyle().accent);

    const auto firstCard = Rect{
        .position = Vec2{48.0f, 104.0f},
        .size = Vec2{180.0f, 88.0f},
    };
    uiContext.DrawFilledRectangle(firstCard, Color{0.24f, 0.78f, 0.56f, 1.0f}, 8.0f);

    const auto secondCard = Rect{
        .position = Vec2{252.0f, 104.0f},
        .size = Vec2{180.0f, 88.0f},
    };
    uiContext.DrawFilledRectangle(secondCard, Color{0.95f, 0.66f, 0.30f, 1.0f}, 8.0f);

    const auto thirdCard = Rect{
        .position = Vec2{456.0f, 104.0f},
        .size = Vec2{180.0f, 88.0f},
    };
    uiContext.DrawFilledRectangle(thirdCard, Color{0.72f, 0.48f, 0.96f, 1.0f}, 8.0f);
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

        while (!window.ShouldClose())
        {
            window.PollEvents();
            if (window.ShouldClose())
            {
                break;
            }

            renderer.BeginFrame();

            const Layout layout{
                .bounds = Rect{
                    .position = Vec2{0.0f, 0.0f},
                    .size = Vec2{static_cast<float>(window.GetWidth()), static_cast<float>(window.GetHeight())},
                },
                .padding = 16.0f,
            };

            uiContext.BeginFrame(layout);
            BuildSimpleUi(uiContext, window.GetWidth(), window.GetHeight());

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

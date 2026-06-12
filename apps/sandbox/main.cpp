#include <chrono>
#include <exception>
#include <iostream>
#include <thread>

#include "engine/core/Color.h"
#include "engine/core/Rect.h"
#include "engine/core/Vec2.h"
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
    const auto background = Rect{
        .position = Vec2{0.0f, 0.0f},
        .size = Vec2{static_cast<float>(windowWidth), static_cast<float>(windowHeight)},
    };
    uiContext.DrawFilledRectangle(background, uiContext.GetStyle().windowBackground);

    const auto panelWidth = static_cast<float>(windowWidth) * 0.5f;
    const auto panelHeight = static_cast<float>(windowHeight) * 0.35f;
    const auto panel = Rect{
        .position = Vec2{32.0f, 32.0f},
        .size = Vec2{panelWidth - 64.0f, panelHeight - 64.0f},
    };
    uiContext.DrawFilledRectangle(panel, uiContext.GetStyle().panelBackground);

    const auto accentBar = Rect{
        .position = Vec2{48.0f, 48.0f},
        .size = Vec2{panel.size.x * 0.5f, 24.0f},
    };
    uiContext.DrawFilledRectangle(accentBar, uiContext.GetStyle().accent);
}

} // namespace

int main()
{
    try
    {
        std::cout << "Starting Greenfield sandbox\n";

        SdlWindow window{"Greenfield Sandbox", 1280, 720};
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

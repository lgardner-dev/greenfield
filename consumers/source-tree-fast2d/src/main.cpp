#include <cmath>
#include <iostream>

#include "engine/core/Color.h"
#include "engine/core/Rect.h"
#include "engine/core/Vec2.h"
#include "engine/input/InputState.h"
#include "engine/render/RenderCommandList.h"
#include "engine/render/fast2d/Fast2DRenderer.h"
#include "engine/ui/Layout.h"
#include "engine/ui/UiContext.h"

namespace
{

constexpr greenfield::Color BackgroundColor{0.06f, 0.08f, 0.10f, 1.0f};
constexpr greenfield::Color PanelColor{0.18f, 0.24f, 0.30f, 1.0f};
constexpr greenfield::Color ButtonColor{0.26f, 0.46f, 0.72f, 1.0f};

[[nodiscard]] bool ColorsMatch(const greenfield::Color& firstColor, const greenfield::Color& secondColor)
{
    constexpr float Tolerance = 0.0001f;
    return std::abs(firstColor.red - secondColor.red) <= Tolerance &&
           std::abs(firstColor.green - secondColor.green) <= Tolerance &&
           std::abs(firstColor.blue - secondColor.blue) <= Tolerance &&
           std::abs(firstColor.alpha - secondColor.alpha) <= Tolerance;
}

[[nodiscard]] greenfield::Rect MakeRectangle(float x, float y, float width, float height)
{
    return greenfield::Rect{
        .position = greenfield::Vec2{.x = x, .y = y},
        .size = greenfield::Vec2{.x = width, .y = height},
    };
}

[[nodiscard]] greenfield::Layout MakeLayout()
{
    return greenfield::Layout{
        .bounds = MakeRectangle(0.0f, 0.0f, 240.0f, 160.0f),
        .padding = 0.0f,
        .gap = 0.0f,
        .itemSize = greenfield::Vec2{.x = 0.0f, .y = 0.0f},
    };
}

[[nodiscard]] const greenfield::RenderCommandList& BuildUserInterface(greenfield::UiContext& uiContext)
{
    uiContext.BeginFrame(MakeLayout(), greenfield::InputState{});
    const greenfield::ButtonStyle buttonStyle{
        .normal = ButtonColor,
        .hovered = ButtonColor,
        .pressed = ButtonColor,
    };
    const greenfield::Rect buttonBounds = MakeRectangle(32.0f, 40.0f, 144.0f, 36.0f);
    const greenfield::Rect textBounds = MakeRectangle(32.0f, 84.0f, 160.0f, 20.0f);
    const greenfield::Color textColor{0.9f, 0.94f, 1.0f, 1.0f};

    uiContext.DrawFilledRectangle(MakeRectangle(0.0f, 0.0f, 240.0f, 160.0f), BackgroundColor);
    uiContext.Panel(MakeRectangle(16.0f, 16.0f, 208.0f, 104.0f), PanelColor, 8.0f);
    const bool wasButtonActivated = uiContext.Button("consumer-action", "Consumer Action", buttonBounds, buttonStyle);
    uiContext.DrawText("source-tree sdk frame", textBounds, 14.0f, textColor);
    (void)wasButtonActivated;
    return uiContext.EndFrame();
}

[[nodiscard]] bool RenderFrame(const greenfield::RenderCommandList& renderCommands,
                               greenfield::Fast2DRenderer& renderer)
{
    renderer.BeginFrame();
    renderer.Submit(renderCommands);
    renderer.EndFrame();

    return renderer.CompletedFrameCommandCount() == renderCommands.Size() && renderer.PreparedFillOperationCount() >= 3U &&
           renderer.DeferredTextCommandCount() >= 2U && ColorsMatch(renderer.RasterPixelAt(200U, 100U), PanelColor) &&
           ColorsMatch(renderer.RasterPixelAt(40U, 56U), ButtonColor);
}

} // namespace

int main()
{
    greenfield::UiContext uiContext;
    const greenfield::RenderCommandList& renderCommands = BuildUserInterface(uiContext);
    if (renderCommands.IsEmpty())
    {
        std::cerr << "Greenfield source-tree consumer did not emit render commands.\n";
        return 1;
    }

    greenfield::Fast2DRenderer renderer{240U, 160U};
    if (!RenderFrame(renderCommands, renderer))
    {
        std::cerr << "Greenfield source-tree consumer failed Fast2D frame validation.\n";
        return 1;
    }

    std::cout << "Greenfield source-tree Fast2D consumer rendered " << renderCommands.Size() << " commands.\n";
    return 0;
}

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

[[nodiscard]] greenfield::Rect MakeRectangle(float x, float y, float width, float height)
{
    return greenfield::Rect{
        .position = greenfield::Vec2{.x = x, .y = y},
        .size = greenfield::Vec2{.x = width, .y = height},
    };
}

[[nodiscard]] greenfield::Layout MakeAppLayout()
{
    return greenfield::Layout{
        .bounds = MakeRectangle(0.0f, 0.0f, 320.0f, 180.0f),
        .padding = 16.0f,
        .gap = 8.0f,
        .itemSize = greenfield::Vec2{.x = 180.0f, .y = 40.0f},
    };
}

} // namespace

int main()
{
    greenfield::UiContext uiContext;
    uiContext.BeginFrame(MakeAppLayout(), greenfield::InputState{});
    uiContext.DrawFilledRectangle(MakeRectangle(0.0f, 0.0f, 320.0f, 180.0f), greenfield::Color{0.08f, 0.10f, 0.12f, 1.0f});
    const bool wasButtonActivated = uiContext.Button("template-action", "Template Action");
    (void)wasButtonActivated;
    const greenfield::RenderCommandList& renderCommands = uiContext.EndFrame();

    greenfield::Fast2DRenderer renderer{320U, 180U};
    renderer.BeginFrame();
    renderer.Submit(renderCommands);
    renderer.EndFrame();

    if (renderer.CompletedFrameCommandCount() != renderCommands.Size())
    {
        std::cerr << "Greenfield template app failed to render its frame.\n";
        return 1;
    }

    std::cout << "Greenfield template app rendered " << renderCommands.Size() << " commands.\n";

    return 0;
}

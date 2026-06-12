#include <cstdlib>

#include "engine/core/Color.h"
#include "engine/core/Rect.h"
#include "engine/core/Vec2.h"
#include "engine/render/RenderCommandList.h"
#include "engine/ui/Layout.h"
#include "engine/ui/Style.h"

int main()
{
    using namespace greenfield;

    const Vec2 size{640.0f, 360.0f};
    const Rect bounds{
        .position = Vec2{0.0f, 0.0f},
        .size = size,
    };

    RenderCommandList renderCommands;
    renderCommands.AddFillRectangle(bounds, Color{0.25f, 0.5f, 0.75f, 1.0f});

    if (renderCommands.Size() != 1U)
    {
        return EXIT_FAILURE;
    }

    const Layout layout{
        .bounds = bounds,
        .padding = 12.0f,
    };

    const Style style{};
    if (layout.bounds.size.x <= 0.0f || style.accent.alpha <= 0.0f)
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

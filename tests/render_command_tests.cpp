#include <cstdlib>

#include "engine/render/RenderCommandList.h"
#include "tests/TestHelpers.h"

namespace
{

[[nodiscard]] bool TestRectangleAndTextCommands()
{
    using namespace greenfield;
    using greenfield::tests::ColorsMatch;

    const Rect bounds{
        .position = Vec2{0.0f, 0.0f},
        .size = Vec2{640.0f, 360.0f},
    };
    const Color fillColor{0.25f, 0.5f, 0.75f, 0.8f};
    const Color borderColor{0.75f, 0.85f, 0.95f, 0.5f};

    RenderCommandList renderCommands;
    renderCommands.AddFillRectangle(bounds, fillColor, 6.0f, borderColor, 2.0f);
    renderCommands.AddText("Hello", Rect{.position = Vec2{4.0f, 8.0f}, .size = Vec2{120.0f, 32.0f}}, 18.0f, fillColor);

    const auto commands = renderCommands.Commands();
    return renderCommands.Size() == 2U && commands[0].type == RenderCommandType::FillRectangle &&
           commands[0].cornerRadius == 6.0f && ColorsMatch(commands[0].fillColor, fillColor) &&
           ColorsMatch(commands[0].borderColor, borderColor) && commands[0].borderThickness == 2.0f &&
           commands[1].type == RenderCommandType::DrawText && commands[1].text == "Hello" &&
           commands[1].fontSize == 18.0f && ColorsMatch(commands[1].textColor, fillColor);
}

[[nodiscard]] bool TestClipCommands()
{
    using namespace greenfield;
    using greenfield::tests::RectanglesMatch;

    const Rect clipBounds{
        .position = Vec2{12.0f, 16.0f},
        .size = Vec2{80.0f, 90.0f},
    };

    RenderCommandList clipCommands;
    clipCommands.PushClip(clipBounds);
    clipCommands.PopClip();

    const auto commands = clipCommands.Commands();
    return clipCommands.Size() == 2U && commands[0].type == RenderCommandType::PushClip &&
           RectanglesMatch(commands[0].rectangle, clipBounds) && commands[1].type == RenderCommandType::PopClip;
}

[[nodiscard]] bool TestAppendAndClear()
{
    using namespace greenfield;

    RenderCommandList firstCommands;
    RenderCommandList secondCommands;
    firstCommands.AddFillRectangle(Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{10.0f, 10.0f}}, Color{});
    secondCommands.AddText("Text", Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{20.0f, 20.0f}}, 14.0f, Color{});

    firstCommands.Append(secondCommands);
    if (firstCommands.Size() != 2U || firstCommands.Commands()[1].type != RenderCommandType::DrawText)
    {
        return false;
    }

    firstCommands.Clear();
    return firstCommands.IsEmpty();
}

} // namespace

int main()
{
    if (!TestRectangleAndTextCommands() || !TestClipCommands() || !TestAppendAndClear())
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

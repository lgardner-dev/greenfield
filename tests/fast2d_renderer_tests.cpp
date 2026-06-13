#include <cstdlib>

#include "engine/render/fast2d/Fast2DRenderer.h"

namespace
{

[[nodiscard]] bool TestConstructionSucceeds()
{
    const greenfield::Fast2DRenderer renderer;
    return renderer.SubmittedCommandCount() == 0U && renderer.CompletedFrameCommandCount() == 0U;
}

[[nodiscard]] bool TestBeginFrameClearsSubmittedCommands()
{
    using namespace greenfield;

    Fast2DRenderer renderer;
    RenderCommandList renderCommands;
    renderCommands.AddFillRectangle(Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{10.0f, 10.0f}}, Color{});

    renderer.Submit(renderCommands);
    if (renderer.SubmittedCommandCount() != 1U)
    {
        return false;
    }

    renderer.BeginFrame();
    return renderer.SubmittedCommandCount() == 0U;
}

[[nodiscard]] bool TestSubmitRecordsAndAppendsCommands()
{
    using namespace greenfield;

    Fast2DRenderer renderer;
    RenderCommandList firstCommands;
    RenderCommandList secondCommands;
    firstCommands.AddFillRectangle(Rect{.position = Vec2{0.0f, 0.0f}, .size = Vec2{20.0f, 20.0f}}, Color{});
    secondCommands.AddText("Fast2D", Rect{.position = Vec2{2.0f, 4.0f}, .size = Vec2{80.0f, 20.0f}},
                           14.0f, Color{});

    renderer.Submit(firstCommands);
    renderer.Submit(secondCommands);

    const auto submittedCommands = renderer.SubmittedCommands();
    return renderer.SubmittedCommandCount() == 2U && submittedCommands[0].type == RenderCommandType::FillRectangle &&
           submittedCommands[1].type == RenderCommandType::DrawText && submittedCommands[1].text == "Fast2D";
}

[[nodiscard]] bool TestEndFrameCompletesWithoutPlatformDependencies()
{
    using namespace greenfield;

    Fast2DRenderer renderer;
    RenderCommandList renderCommands;
    renderCommands.PushClip(Rect{.position = Vec2{1.0f, 2.0f}, .size = Vec2{30.0f, 40.0f}});
    renderCommands.PopClip();

    renderer.BeginFrame();
    renderer.Submit(renderCommands);
    renderer.EndFrame();

    return renderer.CompletedFrameCommandCount() == 2U;
}

} // namespace

int main()
{
    if (!TestConstructionSucceeds() || !TestBeginFrameClearsSubmittedCommands() ||
        !TestSubmitRecordsAndAppendsCommands() || !TestEndFrameCompletesWithoutPlatformDependencies())
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

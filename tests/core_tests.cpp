#include <cstdlib>

#include "engine/core/Rect.h"
#include "engine/core/Vec2.h"
#include "engine/input/InputState.h"
#include "tests/TestHelpers.h"

namespace
{

[[nodiscard]] bool TestRectanglePointContainment()
{
    using namespace greenfield;

    const Rect rectangle{
        .position = Vec2{10.0f, 20.0f},
        .size = Vec2{100.0f, 50.0f},
    };

    return ContainsPoint(rectangle, Vec2{10.0f, 20.0f}) && !ContainsPoint(rectangle, Vec2{110.0f, 70.0f});
}

[[nodiscard]] bool TestRectangleIntersection()
{
    using namespace greenfield;
    using greenfield::tests::RectanglesMatch;

    const Rect bounds{
        .position = Vec2{10.0f, 20.0f},
        .size = Vec2{300.0f, 180.0f},
    };

    const Rect intersection = IntersectRectangles(bounds, Rect{.position = Vec2{250.0f, 100.0f}, .size = Vec2{120.0f, 140.0f}});
    const Rect emptyIntersection = IntersectRectangles(bounds, Rect{.position = Vec2{400.0f, 20.0f}, .size = Vec2{20.0f, 20.0f}});

    return RectanglesMatch(intersection, Rect{.position = Vec2{250.0f, 100.0f}, .size = Vec2{60.0f, 100.0f}}) &&
           RectanglesMatch(emptyIntersection, Rect{.position = Vec2{400.0f, 20.0f}, .size = Vec2{0.0f, 20.0f}});
}

[[nodiscard]] bool TestInputDefaults()
{
    const greenfield::InputState inputState{};
    return inputState.verticalScrollDelta == 0.0f;
}

} // namespace

int main()
{
    if (!TestRectanglePointContainment() || !TestRectangleIntersection() || !TestInputDefaults())
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

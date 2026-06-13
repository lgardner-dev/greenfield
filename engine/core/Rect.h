#pragma once

#include <algorithm>

#include "engine/core/Vec2.h"

namespace greenfield
{

struct Rect
{
    Vec2 position{};
    Vec2 size{};
};

[[nodiscard]] inline bool ContainsPoint(const Rect& rectangle, const Vec2& point) noexcept
{
    const float left = rectangle.position.x;
    const float top = rectangle.position.y;
    const float right = rectangle.position.x + rectangle.size.x;
    const float bottom = rectangle.position.y + rectangle.size.y;

    return point.x >= left && point.x < right && point.y >= top && point.y < bottom;
}

[[nodiscard]] inline Rect IntersectRectangles(const Rect& firstRectangle, const Rect& secondRectangle) noexcept
{
    const float left = std::max(firstRectangle.position.x, secondRectangle.position.x);
    const float top = std::max(firstRectangle.position.y, secondRectangle.position.y);
    const float right =
        std::min(firstRectangle.position.x + firstRectangle.size.x, secondRectangle.position.x + secondRectangle.size.x);
    const float bottom =
        std::min(firstRectangle.position.y + firstRectangle.size.y, secondRectangle.position.y + secondRectangle.size.y);

    return Rect{
        .position = Vec2{.x = left, .y = top},
        .size = Vec2{.x = std::max(0.0f, right - left), .y = std::max(0.0f, bottom - top)},
    };
}

} // namespace greenfield

#pragma once

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

} // namespace greenfield

#pragma once

#include "engine/core/Color.h"
#include "engine/core/Rect.h"

namespace greenfield::tests
{

[[nodiscard]] inline bool ColorsMatch(const Color& firstColor, const Color& secondColor) noexcept
{
    return firstColor.red == secondColor.red && firstColor.green == secondColor.green && firstColor.blue == secondColor.blue && firstColor.alpha == secondColor.alpha;
}

[[nodiscard]] inline bool RectanglesMatch(const Rect& firstRectangle, const Rect& secondRectangle) noexcept
{
    return firstRectangle.position.x == secondRectangle.position.x && firstRectangle.position.y == secondRectangle.position.y && firstRectangle.size.x == secondRectangle.size.x && firstRectangle.size.y == secondRectangle.size.y;
}

} // namespace greenfield::tests

#include "engine/visualization/VisualizationCommandList.h"

#include <algorithm>
#include <cmath>

namespace greenfield
{
namespace
{

[[nodiscard]] bool IsFinite(float value) noexcept
{
    return std::isfinite(value);
}

[[nodiscard]] bool IsFinite(Vec2 value) noexcept
{
    return IsFinite(value.x) && IsFinite(value.y);
}

[[nodiscard]] bool IsFinite(Rect rectangle) noexcept
{
    return IsFinite(rectangle.position) && IsFinite(rectangle.size);
}

[[nodiscard]] bool IsPositiveFinite(float value) noexcept
{
    return IsFinite(value) && value > 0.0f;
}

[[nodiscard]] Rect NormalizeClipBounds(Rect clipBounds) noexcept
{
    const float left = std::min(clipBounds.position.x, clipBounds.position.x + clipBounds.size.x);
    const float top = std::min(clipBounds.position.y, clipBounds.position.y + clipBounds.size.y);
    const float right = std::max(clipBounds.position.x, clipBounds.position.x + clipBounds.size.x);
    const float bottom = std::max(clipBounds.position.y, clipBounds.position.y + clipBounds.size.y);

    return Rect{
        .position = Vec2{.x = left, .y = top},
        .size = Vec2{.x = right - left, .y = bottom - top},
    };
}

[[nodiscard]] bool AllPointsAreFinite(std::span<const Vec2> points) noexcept
{
    return std::all_of(points.begin(), points.end(), [](Vec2 point) {
        return IsFinite(point);
    });
}

} // namespace

bool VisualizationCommandList::SetClipBounds(const Rect& clipBounds)
{
    if (!IsFinite(clipBounds))
    {
        return false;
    }

    _clipBounds = NormalizeClipBounds(clipBounds);
    return true;
}

Rect VisualizationCommandList::ClipBounds() const noexcept
{
    return _clipBounds;
}

bool VisualizationCommandList::AddLine(Vec2 start, Vec2 end, Color strokeColor, float strokeThickness)
{
    if (!IsFinite(start) || !IsFinite(end) || !IsPositiveFinite(strokeThickness))
    {
        return false;
    }

    _commands.push_back(VisualizationLineCommand{
        .start = start,
        .end = end,
        .strokeColor = strokeColor,
        .strokeThickness = strokeThickness,
    });
    return true;
}

bool VisualizationCommandList::AddPolyline(std::span<const Vec2> points, Color strokeColor, float strokeThickness)
{
    if (points.size() < 2 || !AllPointsAreFinite(points) || !IsPositiveFinite(strokeThickness))
    {
        return false;
    }

    _commands.push_back(VisualizationPolylineCommand{
        .points = std::vector<Vec2>(points.begin(), points.end()),
        .strokeColor = strokeColor,
        .strokeThickness = strokeThickness,
    });
    return true;
}

bool VisualizationCommandList::AddPointMarker(Vec2 center, float radius, Color fillColor)
{
    if (!IsFinite(center) || !IsPositiveFinite(radius))
    {
        return false;
    }

    _commands.push_back(VisualizationPointMarkerCommand{
        .center = center,
        .radius = radius,
        .fillColor = fillColor,
    });
    return true;
}

void VisualizationCommandList::Clear() noexcept
{
    _commands.clear();
}

std::size_t VisualizationCommandList::Size() const noexcept
{
    return _commands.size();
}

bool VisualizationCommandList::IsEmpty() const noexcept
{
    return _commands.empty();
}

std::span<const VisualizationCommand> VisualizationCommandList::Commands() const noexcept
{
    return std::span<const VisualizationCommand>(_commands);
}

} // namespace greenfield

#include "engine/visualization/Viewport2D.h"

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

[[nodiscard]] bool IsPositiveScale(float pixelsPerWorldUnit) noexcept
{
    return IsFinite(pixelsPerWorldUnit) && pixelsPerWorldUnit > 0.0f;
}

[[nodiscard]] Rect NormalizeScreenBounds(Rect screenBounds) noexcept
{
    if (!IsFinite(screenBounds))
    {
        return Rect{};
    }

    const float left = std::min(screenBounds.position.x, screenBounds.position.x + screenBounds.size.x);
    const float top = std::min(screenBounds.position.y, screenBounds.position.y + screenBounds.size.y);
    const float right = std::max(screenBounds.position.x, screenBounds.position.x + screenBounds.size.x);
    const float bottom = std::max(screenBounds.position.y, screenBounds.position.y + screenBounds.size.y);

    return Rect{
        .position = Vec2{.x = left, .y = top},
        .size = Vec2{.x = right - left, .y = bottom - top},
    };
}

[[nodiscard]] Vec2 CenterOf(Rect rectangle) noexcept
{
    return Vec2{
        .x = rectangle.position.x + (rectangle.size.x * 0.5f),
        .y = rectangle.position.y + (rectangle.size.y * 0.5f),
    };
}

} // namespace

Viewport2D::Viewport2D() noexcept = default;

Viewport2D::Viewport2D(Rect screenBounds, Vec2 worldCenter, float pixelsPerWorldUnit) noexcept
{
    SetScreenBounds(screenBounds);
    SetWorldCenter(worldCenter);
    SetPixelsPerWorldUnit(pixelsPerWorldUnit);
}

Rect Viewport2D::ScreenBounds() const noexcept
{
    return _screenBounds;
}

Vec2 Viewport2D::WorldCenter() const noexcept
{
    return _worldCenter;
}

float Viewport2D::PixelsPerWorldUnit() const noexcept
{
    return _pixelsPerWorldUnit;
}

void Viewport2D::SetScreenBounds(Rect screenBounds) noexcept
{
    if (!IsFinite(screenBounds))
    {
        return;
    }

    _screenBounds = NormalizeScreenBounds(screenBounds);
}

void Viewport2D::SetWorldCenter(Vec2 worldCenter) noexcept
{
    if (!IsFinite(worldCenter))
    {
        return;
    }

    _worldCenter = worldCenter;
}

void Viewport2D::SetPixelsPerWorldUnit(float pixelsPerWorldUnit) noexcept
{
    if (!IsPositiveScale(pixelsPerWorldUnit))
    {
        return;
    }

    _pixelsPerWorldUnit = pixelsPerWorldUnit;
}

Vec2 Viewport2D::WorldToScreen(Vec2 worldPoint) const noexcept
{
    const Vec2 screenCenter = CenterOf(_screenBounds);

    return Vec2{
        .x = screenCenter.x + ((worldPoint.x - _worldCenter.x) * _pixelsPerWorldUnit),
        .y = screenCenter.y - ((worldPoint.y - _worldCenter.y) * _pixelsPerWorldUnit),
    };
}

Vec2 Viewport2D::ScreenToWorld(Vec2 screenPoint) const noexcept
{
    const Vec2 screenCenter = CenterOf(_screenBounds);

    return Vec2{
        .x = _worldCenter.x + ((screenPoint.x - screenCenter.x) / _pixelsPerWorldUnit),
        .y = _worldCenter.y - ((screenPoint.y - screenCenter.y) / _pixelsPerWorldUnit),
    };
}

void Viewport2D::PanByScreenDelta(Vec2 screenDelta) noexcept
{
    if (!IsFinite(screenDelta))
    {
        return;
    }

    const Vec2 pannedWorldCenter{
        .x = _worldCenter.x - (screenDelta.x / _pixelsPerWorldUnit),
        .y = _worldCenter.y + (screenDelta.y / _pixelsPerWorldUnit),
    };

    if (!IsFinite(pannedWorldCenter))
    {
        return;
    }

    _worldCenter = pannedWorldCenter;
}

void Viewport2D::ZoomBy(float factor, Vec2 screenAnchor) noexcept
{
    if (!IsPositiveScale(factor) || !IsFinite(screenAnchor))
    {
        return;
    }

    const Vec2 anchorWorldPoint = ScreenToWorld(screenAnchor);
    const Vec2 screenCenter = CenterOf(_screenBounds);
    const float zoomedScale = _pixelsPerWorldUnit * factor;

    if (!IsPositiveScale(zoomedScale))
    {
        return;
    }

    const Vec2 zoomedWorldCenter{
        .x = anchorWorldPoint.x - ((screenAnchor.x - screenCenter.x) / zoomedScale),
        .y = anchorWorldPoint.y + ((screenAnchor.y - screenCenter.y) / zoomedScale),
    };

    if (!IsFinite(zoomedWorldCenter))
    {
        return;
    }

    _pixelsPerWorldUnit = zoomedScale;
    _worldCenter = zoomedWorldCenter;
}

} // namespace greenfield

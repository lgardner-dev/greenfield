#pragma once

#include "engine/core/Rect.h"
#include "engine/core/Vec2.h"

namespace greenfield
{

// Viewport2D maps a 2D world onto screen-space viewport bounds.
//
// Coordinate convention:
// - screen-space X increases right and Y increases down;
// - world-space X increases right and Y increases up;
// - scale is pixels per world unit and must remain positive and finite.
class Viewport2D
{
public:
    Viewport2D() noexcept;
    Viewport2D(Rect screenBounds, Vec2 worldCenter, float pixelsPerWorldUnit) noexcept;

    [[nodiscard]] Rect ScreenBounds() const noexcept;
    [[nodiscard]] Vec2 WorldCenter() const noexcept;
    [[nodiscard]] float PixelsPerWorldUnit() const noexcept;

    void SetScreenBounds(Rect screenBounds) noexcept;
    void SetWorldCenter(Vec2 worldCenter) noexcept;
    void SetPixelsPerWorldUnit(float pixelsPerWorldUnit) noexcept;

    [[nodiscard]] Vec2 WorldToScreen(Vec2 worldPoint) const noexcept;
    [[nodiscard]] Vec2 ScreenToWorld(Vec2 screenPoint) const noexcept;

    // Pans with direct-manipulation semantics: dragging content by a screen-space
    // delta would move rendered world content by that same screen-space delta.
    void PanByScreenDelta(Vec2 screenDelta) noexcept;

    // A factor greater than one zooms in. The world point under the screen-space
    // anchor remains under that anchor when the factor is positive and finite.
    void ZoomBy(float factor, Vec2 screenAnchor) noexcept;

private:
    Rect _screenBounds{};
    Vec2 _worldCenter{};
    float _pixelsPerWorldUnit{1.0f};
};

} // namespace greenfield

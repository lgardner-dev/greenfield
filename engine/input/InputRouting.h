#pragma once

#include <span>

#include "engine/core/Surface.h"
#include "engine/core/Vec2.h"

namespace greenfield
{

struct InteractionNode
{
    SurfaceId surfaceId{};
    Rect bounds{};
};

struct HitTestResult
{
    SurfaceId surfaceId{};
    bool hit{false};
};

[[nodiscard]] constexpr InteractionNode MakeInteractionNode(const Surface& surface) noexcept
{
    return InteractionNode{
        .surfaceId = surface.id,
        .bounds = surface.bounds.rectangle,
    };
}

[[nodiscard]] inline HitTestResult HitTestInteractionNode(const InteractionNode& node, const Vec2& point) noexcept
{
    if (!IsValidSurfaceId(node.surfaceId) || !ContainsPoint(node.bounds, point))
    {
        return HitTestResult{};
    }

    return HitTestResult{
        .surfaceId = node.surfaceId,
        .hit = true,
    };
}

[[nodiscard]] inline HitTestResult RouteInputPoint(std::span<const InteractionNode> nodes, const Vec2& point) noexcept
{
    for (const InteractionNode& node : nodes)
    {
        const HitTestResult hitTestResult = HitTestInteractionNode(node, point);
        if (hitTestResult.hit)
        {
            return hitTestResult;
        }
    }

    return HitTestResult{};
}

[[nodiscard]] inline HitTestResult RouteInputPoint(const InteractionNode& node, const Vec2& point) noexcept
{
    return HitTestInteractionNode(node, point);
}

} // namespace greenfield

#pragma once

#include <cstdint>

#include "engine/core/Rect.h"

namespace greenfield
{

struct SurfaceId
{
    std::uint64_t value{0};
};

struct SurfaceBounds
{
    Rect rectangle{};
};

struct Surface
{
    SurfaceId id{};
    SurfaceBounds bounds{};
};

[[nodiscard]] inline bool IsValidSurfaceId(SurfaceId surfaceId) noexcept
{
    return surfaceId.value != 0U;
}

[[nodiscard]] inline bool IsValidSurface(const Surface& surface) noexcept
{
    return IsValidSurfaceId(surface.id);
}

} // namespace greenfield

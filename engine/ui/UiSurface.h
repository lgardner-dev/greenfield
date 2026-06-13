#pragma once

#include "engine/core/Surface.h"
#include "engine/ui/Layout.h"

namespace greenfield
{

inline constexpr SurfaceId RootUiSurfaceId{1U};

class UiSurface
{
public:
    constexpr UiSurface() noexcept = default;

    constexpr UiSurface(SurfaceId id, const Rect& bounds) noexcept
        : _surface{
              .id = id,
              .bounds = SurfaceBounds{.rectangle = bounds},
          }
    {
    }

    [[nodiscard]] constexpr SurfaceId GetId() const noexcept
    {
        return _surface.id;
    }

    [[nodiscard]] constexpr Rect GetBounds() const noexcept
    {
        return _surface.bounds.rectangle;
    }

    [[nodiscard]] constexpr const Surface& GetSurface() const noexcept
    {
        return _surface;
    }

private:
    Surface _surface{};
};

[[nodiscard]] constexpr UiSurface MakeRootUiSurface(const Layout& layout) noexcept
{
    return UiSurface{RootUiSurfaceId, layout.bounds};
}

} // namespace greenfield

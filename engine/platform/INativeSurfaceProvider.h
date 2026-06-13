#pragma once

#include <cstdint>

namespace greenfield
{

enum class NativeSurfaceKind
{
    None,
    Wayland,
    X11,
};

struct NativeSurfaceDescriptor
{
    NativeSurfaceKind kind{NativeSurfaceKind::None};
    void* display{nullptr};
    void* surface{nullptr};
    std::uint64_t window{0};
};

class INativeSurfaceProvider
{
public:
    virtual ~INativeSurfaceProvider() = default;

    [[nodiscard]] virtual int GetSurfacePixelWidth() const noexcept = 0;
    [[nodiscard]] virtual int GetSurfacePixelHeight() const noexcept = 0;
    [[nodiscard]] virtual NativeSurfaceDescriptor GetNativeSurfaceDescriptor() const = 0;
};

} // namespace greenfield

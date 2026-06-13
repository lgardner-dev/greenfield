#pragma once

#include <optional>
#include <string_view>

namespace greenfield
{

enum class RendererBackendKind
{
    WebGpu,
    Fast2D,
};

[[nodiscard]] constexpr std::string_view ToString(RendererBackendKind rendererBackendKind) noexcept
{
    switch (rendererBackendKind)
    {
    case RendererBackendKind::WebGpu:
        return "webgpu";
    case RendererBackendKind::Fast2D:
        return "fast2d";
    }

    return {};
}

[[nodiscard]] constexpr std::optional<RendererBackendKind> ParseRendererBackendKind(std::string_view text) noexcept
{
    if (text == "webgpu")
    {
        return RendererBackendKind::WebGpu;
    }

    if (text == "fast2d")
    {
        return RendererBackendKind::Fast2D;
    }

    return std::nullopt;
}

} // namespace greenfield

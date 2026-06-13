#include <cstdlib>
#include <optional>

#include "engine/render/RendererBackendKind.h"

namespace
{

[[nodiscard]] bool TestRendererBackendKindNames()
{
    using namespace greenfield;

    return ToString(RendererBackendKind::WebGpu) == "webgpu" &&
           ToString(RendererBackendKind::Fast2D) == "fast2d";
}

[[nodiscard]] bool TestRendererBackendKindParsing()
{
    using namespace greenfield;

    const std::optional<RendererBackendKind> webGpuBackend = ParseRendererBackendKind("webgpu");
    const std::optional<RendererBackendKind> fast2DBackend = ParseRendererBackendKind("fast2d");
    const std::optional<RendererBackendKind> unknownBackend = ParseRendererBackendKind("software");

    return webGpuBackend == RendererBackendKind::WebGpu && fast2DBackend == RendererBackendKind::Fast2D &&
           !unknownBackend.has_value();
}

} // namespace

int main()
{
    if (!TestRendererBackendKindNames() || !TestRendererBackendKindParsing())
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

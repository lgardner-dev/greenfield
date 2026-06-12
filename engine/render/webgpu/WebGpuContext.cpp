#include "engine/render/webgpu/WebGpuContext.h"

#include "engine/platform/IWindow.h"

namespace greenfield
{

WebGpuContext::WebGpuContext(IWindow& window)
    : _window(&window)
    , _initialized(true)
{
}

bool WebGpuContext::IsInitialized() const noexcept
{
    return _initialized && _window != nullptr;
}

} // namespace greenfield

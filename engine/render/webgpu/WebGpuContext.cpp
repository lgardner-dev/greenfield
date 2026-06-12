#include "engine/render/webgpu/WebGpuContext.h"

#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

#include <SDL3/SDL.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_video.h>

#include "engine/platform/SdlWindow.h"

namespace greenfield
{
namespace
{

std::string ToString(wgpu::StringView message)
{
    const std::string_view view = static_cast<std::string_view>(message);
    return std::string(view);
}

void ThrowIfFalse(bool condition, const std::string& message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

wgpu::TextureFormat ChooseSurfaceFormat(const wgpu::SurfaceCapabilities& capabilities)
{
    ThrowIfFalse(capabilities.formatCount > 0, "WebGPU surface has no supported formats.");

    for (std::size_t index = 0; index < capabilities.formatCount; ++index)
    {
        if (capabilities.formats[index] == wgpu::TextureFormat::BGRA8Unorm)
        {
            return capabilities.formats[index];
        }
    }

    return capabilities.formats[0];
}

wgpu::CompositeAlphaMode ChooseAlphaMode(const wgpu::SurfaceCapabilities& capabilities)
{
    ThrowIfFalse(capabilities.alphaModeCount > 0, "WebGPU surface has no supported alpha modes.");
    return capabilities.alphaModes[0];
}

struct AdapterRequestResult
{
    wgpu::Adapter adapter;
    std::string errorMessage;
};

struct DeviceRequestResult
{
    wgpu::Device device;
    std::string errorMessage;
};

} // namespace

WebGpuContext::WebGpuContext(SdlWindow& window)
    : _window(&window)
{
    CreateInstance();
    CreateSurface();
    RequestAdapter();
    RequestDevice();
    ReconfigureIfNeeded();
    _initialized = true;
}

WebGpuContext::~WebGpuContext()
{
    ReleaseResources();
}

bool WebGpuContext::IsInitialized() const noexcept
{
    return _initialized && _window != nullptr && _device != nullptr && _queue != nullptr && _surface != nullptr;
}

wgpu::Device WebGpuContext::GetDevice() const noexcept
{
    return _device;
}

wgpu::Queue WebGpuContext::GetQueue() const noexcept
{
    return _queue;
}

wgpu::Surface WebGpuContext::GetSurface() const noexcept
{
    return _surface;
}

wgpu::TextureFormat WebGpuContext::GetSurfaceFormat() const noexcept
{
    return _surfaceFormat;
}

std::uint32_t WebGpuContext::GetSurfaceWidth() const noexcept
{
    return _surfaceWidth;
}

std::uint32_t WebGpuContext::GetSurfaceHeight() const noexcept
{
    return _surfaceHeight;
}

void WebGpuContext::ReconfigureIfNeeded()
{
    const auto width = static_cast<std::uint32_t>(_window->GetWidth());
    const auto height = static_cast<std::uint32_t>(_window->GetHeight());

    if (width == 0 || height == 0)
    {
        return;
    }

    if (width == _surfaceWidth && height == _surfaceHeight)
    {
        return;
    }

    ConfigureSurface(width, height);
}

void WebGpuContext::CreateInstance()
{
    static constexpr auto TimedWaitAnyFeature = wgpu::InstanceFeatureName::TimedWaitAny;

    const wgpu::InstanceDescriptor descriptor{
        .requiredFeatureCount = 1,
        .requiredFeatures = &TimedWaitAnyFeature,
    };

    _instance = wgpu::CreateInstance(&descriptor);
    ThrowIfFalse(_instance != nullptr, "Failed to create WebGPU instance.");
}

void WebGpuContext::CreateSurface()
{
    ThrowIfFalse(_window != nullptr && _window->GetNativeWindow() != nullptr, "Cannot create WebGPU surface without an SDL window.");

    _surface = CreateWaylandSurface();
    if (_surface != nullptr)
    {
        return;
    }

    _surface = CreateX11Surface();
    ThrowIfFalse(_surface != nullptr, "Failed to create WebGPU surface from SDL window properties.");
}

void WebGpuContext::RequestAdapter()
{
    wgpu::RequestAdapterOptions options{};
    options.powerPreference = wgpu::PowerPreference::HighPerformance;
    options.compatibleSurface = _surface;

    AdapterRequestResult result{};
    auto callback = [&result](wgpu::RequestAdapterStatus status, wgpu::Adapter adapter, wgpu::StringView message) {
        if (status != wgpu::RequestAdapterStatus::Success)
        {
            result.errorMessage = ToString(message);
            return;
        }

        result.adapter = adapter;
    };

    const wgpu::Future future = _instance.RequestAdapter(&options, wgpu::CallbackMode::WaitAnyOnly, callback);
    _instance.WaitAny(future, std::numeric_limits<std::uint64_t>::max());

    _adapter = result.adapter;
    ThrowIfFalse(_adapter != nullptr, "Failed to get a compatible WebGPU adapter: " + result.errorMessage);
}

void WebGpuContext::RequestDevice()
{
    wgpu::DeviceDescriptor descriptor{};
    descriptor.SetDeviceLostCallback(wgpu::CallbackMode::AllowSpontaneous,
                                     [](const wgpu::Device&, wgpu::DeviceLostReason, wgpu::StringView) {});

    DeviceRequestResult result{};
    auto callback = [&result](wgpu::RequestDeviceStatus status, wgpu::Device device, wgpu::StringView message) {
        if (status != wgpu::RequestDeviceStatus::Success)
        {
            result.errorMessage = ToString(message);
            return;
        }

        result.device = device;
    };

    const wgpu::Future future = _adapter.RequestDevice(&descriptor, wgpu::CallbackMode::WaitAnyOnly, callback);
    _instance.WaitAny(future, std::numeric_limits<std::uint64_t>::max());

    _device = result.device;
    ThrowIfFalse(_device != nullptr, "Failed to get WebGPU device: " + result.errorMessage);

    _queue = _device.GetQueue();
    ThrowIfFalse(_queue != nullptr, "Failed to get WebGPU device queue.");
}

void WebGpuContext::ReleaseResources() noexcept
{
    _initialized = false;

    if (_surface != nullptr)
    {
        _surface.Unconfigure();
    }

    _queue = nullptr;
    _surface = nullptr;
    _device = nullptr;
    _adapter = nullptr;
    _instance = nullptr;
    _window = nullptr;
    _surfaceWidth = 0;
    _surfaceHeight = 0;
    _surfaceFormat = wgpu::TextureFormat::Undefined;
    _alphaMode = wgpu::CompositeAlphaMode::Auto;
}

void WebGpuContext::ConfigureSurface(std::uint32_t width, std::uint32_t height)
{
    wgpu::SurfaceCapabilities capabilities{};
    const wgpu::Status status = _surface.GetCapabilities(_adapter, &capabilities);
    ThrowIfFalse(status == wgpu::Status::Success, "Failed to get WebGPU surface capabilities.");

    _surfaceFormat = ChooseSurfaceFormat(capabilities);
    _alphaMode = ChooseAlphaMode(capabilities);
    _surfaceWidth = width;
    _surfaceHeight = height;

    const wgpu::SurfaceConfiguration configuration{
        .device = _device,
        .format = _surfaceFormat,
        .usage = wgpu::TextureUsage::RenderAttachment,
        .width = _surfaceWidth,
        .height = _surfaceHeight,
        .alphaMode = _alphaMode,
        .presentMode = wgpu::PresentMode::Fifo,
    };

    _surface.Configure(&configuration);
}

wgpu::Surface WebGpuContext::CreateWaylandSurface() const
{
    SDL_PropertiesID properties = SDL_GetWindowProperties(_window->GetNativeWindow());
    ThrowIfFalse(properties != 0, "Failed to read SDL window properties: " + std::string(SDL_GetError()));

    void* waylandDisplay = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr);
    void* waylandSurface = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);
    if (waylandDisplay == nullptr || waylandSurface == nullptr)
    {
        return {};
    }

    wgpu::SurfaceSourceWaylandSurface source{};
    source.display = waylandDisplay;
    source.surface = waylandSurface;

    const wgpu::SurfaceDescriptor descriptor{
        .nextInChain = &source,
    };

    return _instance.CreateSurface(&descriptor);
}

wgpu::Surface WebGpuContext::CreateX11Surface() const
{
    SDL_PropertiesID properties = SDL_GetWindowProperties(_window->GetNativeWindow());
    ThrowIfFalse(properties != 0, "Failed to read SDL window properties: " + std::string(SDL_GetError()));

    void* x11Display = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
    const Sint64 x11Window = SDL_GetNumberProperty(properties, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
    if (x11Display == nullptr || x11Window == 0)
    {
        return {};
    }

    wgpu::SurfaceSourceXlibWindow source{};
    source.display = x11Display;
    source.window = static_cast<std::uint64_t>(x11Window);

    const wgpu::SurfaceDescriptor descriptor{
        .nextInChain = &source,
    };

    return _instance.CreateSurface(&descriptor);
}

} // namespace greenfield

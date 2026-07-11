#include "engine/render/webgpu/WebGpuContext.h"

#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

#include "engine/platform/INativeSurfaceProvider.h"

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

WebGpuContext::WebGpuContext(INativeSurfaceProvider& surfaceProvider)
    : _surfaceProvider(&surfaceProvider)
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
    return _initialized && _surfaceProvider != nullptr && _device != nullptr && _queue != nullptr && _surface != nullptr;
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
    const int surfacePixelWidth = _surfaceProvider->GetSurfacePixelWidth();
    const int surfacePixelHeight = _surfaceProvider->GetSurfacePixelHeight();
    if (surfacePixelWidth <= 0 || surfacePixelHeight <= 0)
    {
        return;
    }

    const auto width = static_cast<std::uint32_t>(surfacePixelWidth);
    const auto height = static_cast<std::uint32_t>(surfacePixelHeight);
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
    ThrowIfFalse(_surfaceProvider != nullptr, "Cannot create WebGPU surface without a native surface provider.");

    _surface = CreateSurfaceFromDescriptor(_surfaceProvider->GetNativeSurfaceDescriptor());
    ThrowIfFalse(_surface != nullptr, "Failed to create WebGPU surface from native surface properties.");
}

void WebGpuContext::RequestAdapter()
{
    wgpu::RequestAdapterOptions options{};
    options.powerPreference = wgpu::PowerPreference::HighPerformance;
    options.compatibleSurface = _surface;

    AdapterRequestResult adapterRequestResult{};
    auto callback = [&adapterRequestResult](wgpu::RequestAdapterStatus status, wgpu::Adapter adapter, wgpu::StringView message) {
        if (status != wgpu::RequestAdapterStatus::Success)
        {
            adapterRequestResult.errorMessage = ToString(message);
            return;
        }

        adapterRequestResult.adapter = adapter;
    };

    const wgpu::Future future = _instance.RequestAdapter(&options, wgpu::CallbackMode::WaitAnyOnly, callback);
    _instance.WaitAny(future, std::numeric_limits<std::uint64_t>::max());

    _adapter = adapterRequestResult.adapter;
    ThrowIfFalse(_adapter != nullptr,
                 "Failed to get a compatible WebGPU adapter: " + adapterRequestResult.errorMessage);
}

void WebGpuContext::RequestDevice()
{
    wgpu::DeviceDescriptor descriptor{};
    descriptor.SetDeviceLostCallback(wgpu::CallbackMode::AllowSpontaneous,
                                     [](const wgpu::Device&, wgpu::DeviceLostReason, wgpu::StringView) {});
    descriptor.SetUncapturedErrorCallback([](const wgpu::Device&, wgpu::ErrorType, wgpu::StringView message) {
        std::cerr << "WebGPU uncaptured error: " << ToString(message) << '\n';
    });

    DeviceRequestResult deviceRequestResult{};
    auto callback = [&deviceRequestResult](wgpu::RequestDeviceStatus status, wgpu::Device device, wgpu::StringView message) {
        if (status != wgpu::RequestDeviceStatus::Success)
        {
            deviceRequestResult.errorMessage = ToString(message);
            return;
        }

        deviceRequestResult.device = device;
    };

    const wgpu::Future future = _adapter.RequestDevice(&descriptor, wgpu::CallbackMode::WaitAnyOnly, callback);
    _instance.WaitAny(future, std::numeric_limits<std::uint64_t>::max());

    _device = deviceRequestResult.device;
    ThrowIfFalse(_device != nullptr, "Failed to get WebGPU device: " + deviceRequestResult.errorMessage);

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
    _surfaceProvider = nullptr;
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

wgpu::Surface WebGpuContext::CreateSurfaceFromDescriptor(const NativeSurfaceDescriptor& nativeSurface) const
{
    if (nativeSurface.kind == NativeSurfaceKind::Wayland)
    {
        return CreateWaylandSurface(nativeSurface);
    }

    if (nativeSurface.kind == NativeSurfaceKind::X11)
    {
        return CreateX11Surface(nativeSurface);
    }

    return {};
}

wgpu::Surface WebGpuContext::CreateWaylandSurface(const NativeSurfaceDescriptor& nativeSurface) const
{
    ThrowIfFalse(nativeSurface.display != nullptr && nativeSurface.surface != nullptr, "Cannot create a Wayland WebGPU surface without display and surface handles.");

    wgpu::SurfaceSourceWaylandSurface source{};
    source.display = nativeSurface.display;
    source.surface = nativeSurface.surface;

    const wgpu::SurfaceDescriptor descriptor{
        .nextInChain = &source,
    };

    return _instance.CreateSurface(&descriptor);
}

wgpu::Surface WebGpuContext::CreateX11Surface(const NativeSurfaceDescriptor& nativeSurface) const
{
    ThrowIfFalse(nativeSurface.display != nullptr && nativeSurface.window != 0, "Cannot create an X11 WebGPU surface without display and window handles.");

    wgpu::SurfaceSourceXlibWindow source{};
    source.display = nativeSurface.display;
    source.window = nativeSurface.window;

    const wgpu::SurfaceDescriptor descriptor{
        .nextInChain = &source,
    };

    return _instance.CreateSurface(&descriptor);
}

} // namespace greenfield

#pragma once

#include <cstdint>

#include <dawn/webgpu_cpp.h>

namespace greenfield
{

class INativeSurfaceProvider;
struct NativeSurfaceDescriptor;

class WebGpuContext
{
public:
    explicit WebGpuContext(INativeSurfaceProvider& surfaceProvider);
    ~WebGpuContext();

    WebGpuContext(const WebGpuContext&) = delete;
    WebGpuContext& operator=(const WebGpuContext&) = delete;
    WebGpuContext(WebGpuContext&&) = delete;
    WebGpuContext& operator=(WebGpuContext&&) = delete;

    [[nodiscard]] bool IsInitialized() const noexcept;
    [[nodiscard]] wgpu::Device GetDevice() const noexcept;
    [[nodiscard]] wgpu::Queue GetQueue() const noexcept;
    [[nodiscard]] wgpu::Surface GetSurface() const noexcept;
    [[nodiscard]] wgpu::TextureFormat GetSurfaceFormat() const noexcept;
    [[nodiscard]] std::uint32_t GetSurfaceWidth() const noexcept;
    [[nodiscard]] std::uint32_t GetSurfaceHeight() const noexcept;

    void ReconfigureIfNeeded();

private:
    void CreateInstance();
    void CreateSurface();
    void RequestAdapter();
    void RequestDevice();
    void ConfigureSurface(std::uint32_t width, std::uint32_t height);
    void ReleaseResources() noexcept;

    [[nodiscard]] wgpu::Surface CreateSurfaceFromDescriptor(const NativeSurfaceDescriptor& nativeSurface) const;
    [[nodiscard]] wgpu::Surface CreateWaylandSurface(const NativeSurfaceDescriptor& nativeSurface) const;
    [[nodiscard]] wgpu::Surface CreateX11Surface(const NativeSurfaceDescriptor& nativeSurface) const;

    INativeSurfaceProvider* _surfaceProvider{nullptr};
    wgpu::Instance _instance;
    wgpu::Surface _surface;
    wgpu::Adapter _adapter;
    wgpu::Device _device;
    wgpu::Queue _queue;
    wgpu::TextureFormat _surfaceFormat{wgpu::TextureFormat::Undefined};
    wgpu::CompositeAlphaMode _alphaMode{wgpu::CompositeAlphaMode::Auto};
    std::uint32_t _surfaceWidth{0};
    std::uint32_t _surfaceHeight{0};
    bool _initialized{false};
};

} // namespace greenfield

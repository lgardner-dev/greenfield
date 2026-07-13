#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <future>
#include <iomanip>
#include <iostream>
#include <latch>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <syncstream>
#include <thread>
#include <utility>
#include <vector>

#include <SDL3/SDL.h>
#include <SDL3/SDL_video.h>
#include <dawn/native/DawnNative.h>
#include <dawn/webgpu_cpp.h>

#include "engine/platform/INativeSurfaceProvider.h"
#include "engine/platform/SdlStartupPresenter.h"
#include "engine/platform/SdlWindow.h"

namespace
{

using Clock = std::chrono::steady_clock;

enum class ProbeMode
{
    Hardware,
    SwiftShader,
    Race,
    Inventory,
    PresentHardware,
    PresentSwiftShader,
};

enum class CandidateKind
{
    Hardware,
    SwiftShader,
};

struct ProbeOptions
{
    ProbeMode mode{ProbeMode::Race};
    int windowWidth{1280};
    int windowHeight{720};
    std::uint64_t timeoutMilliseconds{15000};
    std::uint64_t presentHoldMilliseconds{1000};
    int presentFrameCount{4};
    bool resizeExercise{false};
};

struct AdapterIdentity
{
    std::string vendor;
    std::string architecture;
    std::string device;
    std::string description;
    std::string backendType;
    std::string adapterType;
    std::uint32_t vendorId{0};
    std::uint32_t deviceId{0};
    std::size_t featureCount{0};
    std::uint32_t maxTextureDimension2D{0};
    std::uint32_t subgroupMinSize{0};
    std::uint32_t subgroupMaxSize{0};
};

struct AdapterInventoryRecord
{
    std::size_t index{0};
    dawn::native::Adapter nativeAdapter;
    wgpu::Adapter adapter;
    AdapterIdentity identity;
    bool isSwiftShader{false};
    bool isHardware{false};
    int hardwareRank{-1};
    std::string classificationReason;
};

struct CandidateTimings
{
    double instanceCreatedMilliseconds{0.0};
    double adapterRequestedMilliseconds{0.0};
    double adapterInfoQueriedMilliseconds{0.0};
    double deviceRequestedMilliseconds{0.0};
    double queueAcquiredMilliseconds{0.0};
    double shaderModuleCreatedMilliseconds{0.0};
    double pipelineCreatedMilliseconds{0.0};
    double resourcesCreatedMilliseconds{0.0};
    double commandEncodedMilliseconds{0.0};
    double firstQueueSubmittedMilliseconds{0.0};
    double queueCompletedMilliseconds{0.0};
    double readbackCompletedMilliseconds{0.0};
};

struct PresentationTimings
{
    double windowReadyMilliseconds{0.0};
    double startupFrameVisibleMilliseconds{0.0};
    double startupSurfaceReleasedMilliseconds{0.0};
    double webGpuSurfaceCreatedMilliseconds{0.0};
    double adapterSelectedMilliseconds{0.0};
    double deviceRequestedMilliseconds{0.0};
    double queueAcquiredMilliseconds{0.0};
    double surfaceCapabilitiesMilliseconds{0.0};
    double surfaceConfiguredMilliseconds{0.0};
    double shaderModuleCreatedMilliseconds{0.0};
    double pipelineCreatedMilliseconds{0.0};
    double firstFrameAcquiredMilliseconds{0.0};
    double commandEncodedMilliseconds{0.0};
    double firstQueueSubmittedMilliseconds{0.0};
    double firstPresentedMilliseconds{0.0};
    double queueCompletedMilliseconds{0.0};
    double postPresentPumpCompletedMilliseconds{0.0};
};

struct ResizeStepResult
{
    std::string label;
    int requestedWidth{0};
    int requestedHeight{0};
    int configuredWidth{0};
    int configuredHeight{0};
    int framesPresented{0};
    bool resizeEventReceived{false};
    bool pixelSizeEventReceived{false};
    bool setWindowSizeSucceeded{false};
    bool syncWindowSucceeded{false};
    bool reachedRequestedSize{false};
    std::vector<std::string> surfaceStatuses;
};

struct CandidateResult
{
    CandidateKind candidateKind{CandidateKind::Hardware};
    bool success{false};
    std::string failureStage;
    std::string failureMessage;
    AdapterIdentity adapterIdentity;
    std::size_t adapterIndex{std::numeric_limits<std::size_t>::max()};
    std::string selectionReason;
    CandidateTimings timings;
    bool renderVerified{false};
    std::string renderVerificationMessage;
    std::vector<std::string> deviceMessages;
};

struct PresentationResult
{
    CandidateKind candidateKind{CandidateKind::Hardware};
    bool success{false};
    std::string failureStage;
    std::string failureMessage;
    AdapterIdentity adapterIdentity;
    std::size_t adapterIndex{std::numeric_limits<std::size_t>::max()};
    std::string selectionReason;
    std::string currentVideoDriver;
    std::vector<std::string> availableVideoDrivers;
    greenfield::NativeSurfaceKind nativeSurfaceKind{greenfield::NativeSurfaceKind::None};
    int surfacePixelWidth{0};
    int surfacePixelHeight{0};
    bool hadSdlWindowSurfaceBeforeRelease{false};
    bool hasSdlWindowSurfaceAfterRelease{false};
    wgpu::TextureFormat surfaceFormat{wgpu::TextureFormat::Undefined};
    wgpu::CompositeAlphaMode alphaMode{wgpu::CompositeAlphaMode::Auto};
    wgpu::PresentMode presentMode{wgpu::PresentMode::Fifo};
    wgpu::SurfaceGetCurrentTextureStatus currentTextureStatus{wgpu::SurfaceGetCurrentTextureStatus::Error};
    wgpu::Status presentStatus{wgpu::Status::Error};
    int successfulFrameCount{0};
    int resizeEventsReceived{0};
    int pixelSizeEventsReceived{0};
    bool sawUnexpectedSurfaceState{false};
    PresentationTimings timings;
    std::vector<ResizeStepResult> resizeSteps;
    std::vector<std::string> deviceMessages;
};

struct StartupFrameResult
{
    bool success{false};
    std::string failureMessage;
    double windowReadyMilliseconds{0.0};
    double startupFrameVisibleMilliseconds{0.0};
};

struct RequestAdapterResult
{
    wgpu::Adapter adapter;
    std::string message;
    wgpu::RequestAdapterStatus status{wgpu::RequestAdapterStatus::Error};
};

struct RequestDeviceResult
{
    wgpu::Device device;
    std::string message;
    wgpu::RequestDeviceStatus status{wgpu::RequestDeviceStatus::Error};
};

struct QueueDoneResult
{
    std::string message;
    wgpu::QueueWorkDoneStatus status{wgpu::QueueWorkDoneStatus::Error};
};

struct MapResult
{
    std::string message;
    wgpu::MapAsyncStatus status{wgpu::MapAsyncStatus::Error};
};

[[nodiscard]] std::string ToString(wgpu::StringView value)
{
    const std::string_view view = static_cast<std::string_view>(value);
    return std::string(view);
}

[[nodiscard]] std::string ToLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

[[nodiscard]] std::string QuoteForMachineOutput(std::string_view value)
{
    std::string result;
    result.reserve(value.size() + 2U);
    result.push_back('"');
    for (const char character : value)
    {
        if (character == '"' || character == '\\')
        {
            result.push_back('\\');
        }
        result.push_back(character);
    }
    result.push_back('"');
    return result;
}

[[nodiscard]] std::string ToString(wgpu::BackendType backendType)
{
    switch (backendType)
    {
    case wgpu::BackendType::Null:
        return "null";
    case wgpu::BackendType::WebGPU:
        return "webgpu";
    case wgpu::BackendType::D3D11:
        return "d3d11";
    case wgpu::BackendType::D3D12:
        return "d3d12";
    case wgpu::BackendType::Metal:
        return "metal";
    case wgpu::BackendType::Vulkan:
        return "vulkan";
    case wgpu::BackendType::OpenGL:
        return "opengl";
    case wgpu::BackendType::OpenGLES:
        return "opengles";
    case wgpu::BackendType::Undefined:
    default:
        return "undefined";
    }
}

[[nodiscard]] std::string ToString(wgpu::AdapterType adapterType)
{
    switch (adapterType)
    {
    case wgpu::AdapterType::DiscreteGPU:
        return "discrete";
    case wgpu::AdapterType::IntegratedGPU:
        return "integrated";
    case wgpu::AdapterType::CPU:
        return "cpu";
    case wgpu::AdapterType::Unknown:
    default:
        return "unknown";
    }
}

[[nodiscard]] std::string ToString(CandidateKind candidateKind)
{
    return candidateKind == CandidateKind::Hardware ? "hardware" : "swiftshader";
}

[[nodiscard]] std::string ToString(greenfield::NativeSurfaceKind surfaceKind)
{
    switch (surfaceKind)
    {
    case greenfield::NativeSurfaceKind::Wayland:
        return "wayland";
    case greenfield::NativeSurfaceKind::X11:
        return "x11";
    case greenfield::NativeSurfaceKind::None:
    default:
        return "none";
    }
}

[[nodiscard]] std::string ToString(wgpu::TextureFormat textureFormat)
{
    switch (textureFormat)
    {
    case wgpu::TextureFormat::BGRA8Unorm:
        return "bgra8unorm";
    case wgpu::TextureFormat::RGBA8Unorm:
        return "rgba8unorm";
    case wgpu::TextureFormat::BGRA8UnormSrgb:
        return "bgra8unorm_srgb";
    case wgpu::TextureFormat::RGBA8UnormSrgb:
        return "rgba8unorm_srgb";
    case wgpu::TextureFormat::Undefined:
    default:
        return "undefined";
    }
}

[[nodiscard]] std::string ToString(wgpu::CompositeAlphaMode alphaMode)
{
    switch (alphaMode)
    {
    case wgpu::CompositeAlphaMode::Opaque:
        return "opaque";
    case wgpu::CompositeAlphaMode::Premultiplied:
        return "premultiplied";
    case wgpu::CompositeAlphaMode::Unpremultiplied:
        return "unpremultiplied";
    case wgpu::CompositeAlphaMode::Inherit:
        return "inherit";
    case wgpu::CompositeAlphaMode::Auto:
    default:
        return "auto";
    }
}

[[nodiscard]] std::string ToString(wgpu::PresentMode presentMode)
{
    switch (presentMode)
    {
    case wgpu::PresentMode::Fifo:
        return "fifo";
    case wgpu::PresentMode::FifoRelaxed:
        return "fifo_relaxed";
    case wgpu::PresentMode::Immediate:
        return "immediate";
    case wgpu::PresentMode::Mailbox:
        return "mailbox";
    case wgpu::PresentMode::Undefined:
    default:
        return "undefined";
    }
}

[[nodiscard]] std::string ToString(wgpu::SurfaceGetCurrentTextureStatus status)
{
    switch (status)
    {
    case wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal:
        return "success_optimal";
    case wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal:
        return "success_suboptimal";
    case wgpu::SurfaceGetCurrentTextureStatus::Timeout:
        return "timeout";
    case wgpu::SurfaceGetCurrentTextureStatus::Outdated:
        return "outdated";
    case wgpu::SurfaceGetCurrentTextureStatus::Lost:
        return "lost";
    case wgpu::SurfaceGetCurrentTextureStatus::Error:
    default:
        return "error";
    }
}

[[nodiscard]] std::string ToString(wgpu::Status status)
{
    switch (status)
    {
    case wgpu::Status::Success:
        return "success";
    case wgpu::Status::Error:
    default:
        return "error";
    }
}

[[nodiscard]] std::string ToString(ProbeMode mode)
{
    switch (mode)
    {
    case ProbeMode::Hardware:
        return "hardware";
    case ProbeMode::SwiftShader:
        return "swiftshader";
    case ProbeMode::Race:
        return "race";
    case ProbeMode::Inventory:
        return "inventory";
    case ProbeMode::PresentHardware:
        return "present-hardware";
    case ProbeMode::PresentSwiftShader:
        return "present-swiftshader";
    }

    return "race";
}

[[nodiscard]] std::string ToString(wgpu::QueueWorkDoneStatus status)
{
    switch (status)
    {
    case wgpu::QueueWorkDoneStatus::Success:
        return "success";
    case wgpu::QueueWorkDoneStatus::CallbackCancelled:
        return "callback_cancelled";
    case wgpu::QueueWorkDoneStatus::Error:
    default:
        return "error";
    }
}

[[nodiscard]] double MillisecondsSince(Clock::time_point epoch, Clock::time_point timePoint)
{
    return std::chrono::duration<double, std::milli>(timePoint - epoch).count();
}

[[nodiscard]] std::uint64_t TimeoutNanoseconds(const ProbeOptions& options)
{
    return options.timeoutMilliseconds * 1000ULL * 1000ULL;
}

[[nodiscard]] bool ContainsSwiftShaderIdentity(const AdapterIdentity& identity)
{
    const std::string combined =
        ToLower(identity.vendor + " " + identity.architecture + " " + identity.device + " " + identity.description);
    return combined.find("swiftshader") != std::string::npos ||
           (identity.vendorId == 0x1AE0U && identity.deviceId == 0xC0DEU);
}

[[nodiscard]] bool ContainsKnownSoftwareIdentity(const AdapterIdentity& identity)
{
    const std::string combined =
        ToLower(identity.vendor + " " + identity.architecture + " " + identity.device + " " + identity.description);
    return combined.find("swiftshader") != std::string::npos || combined.find("lavapipe") != std::string::npos ||
           combined.find("llvmpipe") != std::string::npos || combined.find("software rasterizer") != std::string::npos ||
           combined.find("microsoft basic render") != std::string::npos || combined.find("warp") != std::string::npos;
}

[[nodiscard]] int HardwareRankFor(const AdapterIdentity& identity)
{
    if (identity.adapterType == "cpu" || ContainsKnownSoftwareIdentity(identity))
    {
        return -1;
    }

    if (identity.adapterType == "discrete")
    {
        return 300;
    }

    if (identity.adapterType == "integrated")
    {
        return 200;
    }

    if (identity.adapterType == "unknown")
    {
        return 100;
    }

    return -1;
}

[[nodiscard]] std::string ClassifyAdapter(const AdapterIdentity& identity, bool isSwiftShader, int hardwareRank)
{
    if (isSwiftShader)
    {
        return "swiftshader_identity_match";
    }

    if (identity.adapterType == "cpu")
    {
        return "cpu_adapter";
    }

    if (ContainsKnownSoftwareIdentity(identity))
    {
        return "known_software_identity";
    }

    if (hardwareRank == 300)
    {
        return "discrete_hardware";
    }

    if (hardwareRank == 200)
    {
        return "integrated_hardware";
    }

    if (hardwareRank == 100)
    {
        return "unknown_non_cpu_hardware";
    }

    return "unsupported_adapter";
}

[[nodiscard]] AdapterIdentity QueryAdapterIdentity(const wgpu::Adapter& adapter)
{
    wgpu::AdapterInfo adapterInfo{};
    const wgpu::Status adapterStatus = adapter.GetInfo(&adapterInfo);
    if (adapterStatus != wgpu::Status::Success)
    {
        throw std::runtime_error("Adapter info query failed.");
    }

    AdapterIdentity identity{
        .vendor = ToString(adapterInfo.vendor),
        .architecture = ToString(adapterInfo.architecture),
        .device = ToString(adapterInfo.device),
        .description = ToString(adapterInfo.description),
        .backendType = ToString(adapterInfo.backendType),
        .adapterType = ToString(adapterInfo.adapterType),
        .vendorId = adapterInfo.vendorID,
        .deviceId = adapterInfo.deviceID,
        .subgroupMinSize = adapterInfo.subgroupMinSize,
        .subgroupMaxSize = adapterInfo.subgroupMaxSize,
    };

    wgpu::SupportedFeatures features{};
    adapter.GetFeatures(&features);
    identity.featureCount = features.featureCount;

    wgpu::Limits limits{};
    if (adapter.GetLimits(&limits) == wgpu::Status::Success)
    {
        identity.maxTextureDimension2D = limits.maxTextureDimension2D;
    }

    return identity;
}

void MarkFailure(CandidateResult& result, std::string stage, std::string message)
{
    result.success = false;
    result.failureStage = std::move(stage);
    result.failureMessage = std::move(message);
}

void MarkFailure(PresentationResult& result, std::string stage, std::string message)
{
    result.success = false;
    result.failureStage = std::move(stage);
    result.failureMessage = std::move(message);
}

void PrintSelection(CandidateKind candidateKind, const AdapterInventoryRecord& selectedAdapter)
{
    const AdapterIdentity& identity = selectedAdapter.identity;
    std::osyncstream(std::cout) << "PROBE_SELECTION"
                                << " candidate=" << ToString(candidateKind)
                                << " adapter_index=" << selectedAdapter.index
                                << " reason=" << selectedAdapter.classificationReason
                                << " backend=" << identity.backendType
                                << " adapter_type=" << identity.adapterType
                                << " vendor_id=" << identity.vendorId
                                << " device_id=" << identity.deviceId
                                << " name=" << QuoteForMachineOutput(identity.device)
                                << " description=" << QuoteForMachineOutput(identity.description) << '\n';
}

[[nodiscard]] bool IsPresentationMode(ProbeMode mode)
{
    return mode == ProbeMode::PresentHardware || mode == ProbeMode::PresentSwiftShader;
}

[[nodiscard]] CandidateKind GetPresentationCandidateKind(ProbeMode mode)
{
    return mode == ProbeMode::PresentSwiftShader ? CandidateKind::SwiftShader : CandidateKind::Hardware;
}

[[nodiscard]] bool IsUsableSurfaceTextureStatus(wgpu::SurfaceGetCurrentTextureStatus status)
{
    return status == wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal ||
           status == wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal;
}

[[nodiscard]] bool IsUnexpectedSurfaceTextureStatus(wgpu::SurfaceGetCurrentTextureStatus status)
{
    return status != wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal;
}

[[nodiscard]] std::vector<std::string> GetAvailableSdlVideoDrivers()
{
    std::vector<std::string> videoDrivers;
    const int driverCount = SDL_GetNumVideoDrivers();
    for (int index = 0; index < driverCount; ++index)
    {
        const char* driverName = SDL_GetVideoDriver(index);
        if (driverName != nullptr)
        {
            videoDrivers.emplace_back(driverName);
        }
    }
    return videoDrivers;
}

[[nodiscard]] std::string GetCurrentSdlVideoDriver()
{
    const char* driverName = SDL_GetCurrentVideoDriver();
    return driverName == nullptr ? "none" : std::string(driverName);
}

struct ResizeEventCounter
{
    SDL_WindowID windowId{0};
    int resizeEventsReceived{0};
    int pixelSizeEventsReceived{0};
};

bool CountResizeEvents(void* userData, SDL_Event* event)
{
    auto* counter = static_cast<ResizeEventCounter*>(userData);
    if (counter == nullptr || event == nullptr || event->window.windowID != counter->windowId)
    {
        return true;
    }

    if (event->type == SDL_EVENT_WINDOW_RESIZED)
    {
        ++counter->resizeEventsReceived;
    }
    else if (event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
    {
        ++counter->pixelSizeEventsReceived;
    }

    return true;
}

class ScopedSdlEventWatch
{
public:
    explicit ScopedSdlEventWatch(ResizeEventCounter& counter) noexcept
        : _counter(&counter)
    {
        SDL_AddEventWatch(CountResizeEvents, _counter);
    }

    ~ScopedSdlEventWatch()
    {
        SDL_RemoveEventWatch(CountResizeEvents, _counter);
    }

    ScopedSdlEventWatch(const ScopedSdlEventWatch&) = delete;
    ScopedSdlEventWatch& operator=(const ScopedSdlEventWatch&) = delete;

private:
    ResizeEventCounter* _counter{nullptr};
};

[[nodiscard]] bool WaitForFuture(const wgpu::Instance& instance, const wgpu::Future& future, std::uint64_t timeoutNanoseconds)
{
    return instance.WaitAny(future, timeoutNanoseconds) == wgpu::WaitStatus::Success;
}

[[nodiscard]] wgpu::InstanceDescriptor MakeProbeInstanceDescriptor()
{
    static constexpr auto TimedWaitAnyFeature = wgpu::InstanceFeatureName::TimedWaitAny;
    return wgpu::InstanceDescriptor{
        .requiredFeatureCount = 1,
        .requiredFeatures = &TimedWaitAnyFeature,
    };
}

[[nodiscard]] wgpu::RequestAdapterOptions MakeVulkanEnumerationOptions()
{
    wgpu::RequestAdapterOptions options{};
    options.backendType = wgpu::BackendType::Vulkan;
    return options;
}

[[nodiscard]] std::vector<AdapterInventoryRecord> EnumerateVulkanAdapters(const dawn::native::Instance& nativeInstance)
{
    const wgpu::RequestAdapterOptions options = MakeVulkanEnumerationOptions();
    const std::vector<dawn::native::Adapter> nativeAdapters = nativeInstance.EnumerateAdapters(&options);

    std::vector<AdapterInventoryRecord> records;
    records.reserve(nativeAdapters.size());
    for (std::size_t index = 0; index < nativeAdapters.size(); ++index)
    {
        wgpu::Adapter adapter{nativeAdapters[index].Get()};
        AdapterIdentity identity = QueryAdapterIdentity(adapter);
        const bool isSwiftShader = ContainsSwiftShaderIdentity(identity);
        const int hardwareRank = HardwareRankFor(identity);

        records.push_back(AdapterInventoryRecord{
            .index = index,
            .nativeAdapter = nativeAdapters[index],
            .adapter = std::move(adapter),
            .identity = std::move(identity),
            .isSwiftShader = isSwiftShader,
            .isHardware = hardwareRank >= 0,
            .hardwareRank = hardwareRank,
            .classificationReason = {},
        });
        records.back().classificationReason =
            ClassifyAdapter(records.back().identity, records.back().isSwiftShader, records.back().hardwareRank);
    }

    return records;
}

[[nodiscard]] bool HardwareRecordComesBefore(const AdapterInventoryRecord& left, const AdapterInventoryRecord& right)
{
    if (left.hardwareRank != right.hardwareRank)
    {
        return left.hardwareRank > right.hardwareRank;
    }

    if (left.identity.maxTextureDimension2D != right.identity.maxTextureDimension2D)
    {
        return left.identity.maxTextureDimension2D > right.identity.maxTextureDimension2D;
    }

    if (left.identity.vendorId != right.identity.vendorId)
    {
        return left.identity.vendorId < right.identity.vendorId;
    }

    if (left.identity.deviceId != right.identity.deviceId)
    {
        return left.identity.deviceId < right.identity.deviceId;
    }

    return left.index < right.index;
}

[[nodiscard]] std::optional<std::size_t> SelectHardwareAdapterIndex(const std::vector<AdapterInventoryRecord>& records)
{
    std::optional<std::size_t> selectedIndex;
    for (std::size_t index = 0; index < records.size(); ++index)
    {
        if (!records[index].isHardware)
        {
            continue;
        }

        if (!selectedIndex.has_value() || HardwareRecordComesBefore(records[index], records[*selectedIndex]))
        {
            selectedIndex = index;
        }
    }

    return selectedIndex;
}

[[nodiscard]] std::optional<std::size_t> SelectSwiftShaderAdapterIndex(const std::vector<AdapterInventoryRecord>& records)
{
    for (std::size_t index = 0; index < records.size(); ++index)
    {
        if (records[index].isSwiftShader)
        {
            return index;
        }
    }

    return std::nullopt;
}

[[nodiscard]] std::string BuildSelectionFailureMessage(CandidateKind candidateKind,
                                                       const std::vector<AdapterInventoryRecord>& records)
{
    std::ostringstream stream;
    stream << "Enumerated " << records.size() << " Vulkan adapter(s), but no "
           << (candidateKind == CandidateKind::Hardware ? "hardware" : "SwiftShader") << " adapter matched.";
    return stream.str();
}

[[nodiscard]] wgpu::ShaderModule CreateProbeShaderModule(const wgpu::Device& device)
{
    wgpu::ShaderSourceWGSL shaderSource{};
    shaderSource.code = R"(
@vertex
fn VertexMain(@builtin(vertex_index) vertexIndex: u32) -> @builtin(position) vec4f {
    let positions = array<vec2f, 3>(
        vec2f(-0.75, -0.75),
        vec2f(0.75, -0.75),
        vec2f(0.0, 0.75)
    );
    return vec4f(positions[vertexIndex], 0.0, 1.0);
}

@fragment
fn FragmentMain() -> @location(0) vec4f {
    return vec4f(0.95, 0.25, 0.10, 1.0);
}
)";

    const wgpu::ShaderModuleDescriptor shaderModuleDescriptor{
        .nextInChain = &shaderSource,
        .label = "Renderer startup probe shader",
    };
    return device.CreateShaderModule(&shaderModuleDescriptor);
}

[[nodiscard]] wgpu::RenderPipeline CreateProbePipeline(const wgpu::Device& device,
                                                       const wgpu::ShaderModule& shaderModule,
                                                       wgpu::TextureFormat textureFormat)
{
    const wgpu::ColorTargetState colorTargetState{
        .format = textureFormat,
        .writeMask = wgpu::ColorWriteMask::All,
    };

    const wgpu::FragmentState fragmentState{
        .module = shaderModule,
        .entryPoint = "FragmentMain",
        .targetCount = 1,
        .targets = &colorTargetState,
    };

    wgpu::RenderPipelineDescriptor pipelineDescriptor{};
    pipelineDescriptor.label = "Renderer startup probe pipeline";
    pipelineDescriptor.vertex.module = shaderModule;
    pipelineDescriptor.vertex.entryPoint = "VertexMain";
    pipelineDescriptor.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    pipelineDescriptor.fragment = &fragmentState;

    return device.CreateRenderPipeline(&pipelineDescriptor);
}

[[nodiscard]] wgpu::Texture CreateRenderTarget(const wgpu::Device& device)
{
    const wgpu::TextureDescriptor textureDescriptor{
        .label = "Renderer startup probe render target",
        .usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc,
        .dimension = wgpu::TextureDimension::e2D,
        .size =
            wgpu::Extent3D{
                .width = 64,
                .height = 64,
                .depthOrArrayLayers = 1,
            },
        .format = wgpu::TextureFormat::RGBA8Unorm,
        .mipLevelCount = 1,
        .sampleCount = 1,
    };
    return device.CreateTexture(&textureDescriptor);
}

[[nodiscard]] wgpu::TextureFormat ChooseSurfaceFormat(const wgpu::SurfaceCapabilities& capabilities)
{
    if (capabilities.formatCount == 0U)
    {
        return wgpu::TextureFormat::Undefined;
    }

    for (std::size_t index = 0; index < capabilities.formatCount; ++index)
    {
        if (capabilities.formats[index] == wgpu::TextureFormat::BGRA8Unorm)
        {
            return capabilities.formats[index];
        }
    }

    return capabilities.formats[0];
}

[[nodiscard]] wgpu::CompositeAlphaMode ChooseAlphaMode(const wgpu::SurfaceCapabilities& capabilities)
{
    if (capabilities.alphaModeCount == 0U)
    {
        return wgpu::CompositeAlphaMode::Auto;
    }

    return capabilities.alphaModes[0];
}

[[nodiscard]] wgpu::Surface CreateNativeWebGpuSurface(const wgpu::Instance& instance,
                                                     const greenfield::NativeSurfaceDescriptor& nativeSurface)
{
    if (nativeSurface.kind == greenfield::NativeSurfaceKind::Wayland)
    {
        wgpu::SurfaceSourceWaylandSurface source{};
        source.display = nativeSurface.display;
        source.surface = nativeSurface.surface;

        const wgpu::SurfaceDescriptor descriptor{
            .nextInChain = &source,
            .label = "Renderer startup probe Wayland surface",
        };
        return instance.CreateSurface(&descriptor);
    }

    if (nativeSurface.kind == greenfield::NativeSurfaceKind::X11)
    {
        wgpu::SurfaceSourceXlibWindow source{};
        source.display = nativeSurface.display;
        source.window = nativeSurface.window;

        const wgpu::SurfaceDescriptor descriptor{
            .nextInChain = &source,
            .label = "Renderer startup probe X11 surface",
        };
        return instance.CreateSurface(&descriptor);
    }

    return {};
}

[[nodiscard]] wgpu::Buffer CreateReadbackBuffer(const wgpu::Device& device, std::uint64_t byteCount)
{
    const wgpu::BufferDescriptor bufferDescriptor{
        .label = "Renderer startup probe readback buffer",
        .usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead,
        .size = byteCount,
    };
    return device.CreateBuffer(&bufferDescriptor);
}

void SubmitProbeRenderPass(const wgpu::Device& device,
                           const wgpu::Queue& queue,
                           const wgpu::RenderPipeline& pipeline,
                           const wgpu::Texture& renderTarget)
{
    wgpu::TextureView textureView = renderTarget.CreateView();
    wgpu::RenderPassColorAttachment colorAttachment{};
    colorAttachment.view = textureView;
    colorAttachment.loadOp = wgpu::LoadOp::Clear;
    colorAttachment.storeOp = wgpu::StoreOp::Store;
    colorAttachment.clearValue = wgpu::Color{
        .r = 0.10,
        .g = 0.20,
        .b = 0.30,
        .a = 1.0,
    };

    const wgpu::RenderPassDescriptor renderPassDescriptor{
        .label = "Renderer startup probe render pass",
        .colorAttachmentCount = 1,
        .colorAttachments = &colorAttachment,
    };

    wgpu::CommandEncoder commandEncoder = device.CreateCommandEncoder();
    wgpu::RenderPassEncoder renderPass = commandEncoder.BeginRenderPass(&renderPassDescriptor);
    renderPass.SetPipeline(pipeline);
    renderPass.Draw(3);
    renderPass.End();

    wgpu::CommandBuffer commandBuffer = commandEncoder.Finish();
    queue.Submit(1, &commandBuffer);
}

void SubmitPresentationFrame(const wgpu::Device& device,
                             const wgpu::Queue& queue,
                             const wgpu::RenderPipeline& pipeline,
                             const wgpu::Texture& surfaceTexture)
{
    wgpu::TextureView textureView = surfaceTexture.CreateView();
    wgpu::RenderPassColorAttachment colorAttachment{};
    colorAttachment.view = textureView;
    colorAttachment.loadOp = wgpu::LoadOp::Clear;
    colorAttachment.storeOp = wgpu::StoreOp::Store;
    colorAttachment.clearValue = wgpu::Color{
        .r = 0.02,
        .g = 0.19,
        .b = 0.34,
        .a = 1.0,
    };

    const wgpu::RenderPassDescriptor renderPassDescriptor{
        .label = "Renderer startup probe native presentation render pass",
        .colorAttachmentCount = 1,
        .colorAttachments = &colorAttachment,
    };

    wgpu::CommandEncoder commandEncoder = device.CreateCommandEncoder();
    wgpu::RenderPassEncoder renderPass = commandEncoder.BeginRenderPass(&renderPassDescriptor);
    renderPass.SetPipeline(pipeline);
    renderPass.Draw(3);
    renderPass.End();

    wgpu::CommandBuffer commandBuffer = commandEncoder.Finish();
    queue.Submit(1, &commandBuffer);
}

[[nodiscard]] QueueDoneResult WaitForQueue(const wgpu::Instance& instance,
                                           const wgpu::Queue& queue,
                                           std::uint64_t timeoutNanoseconds);

void ConfigureNativeSurface(wgpu::Surface& surface,
                            const wgpu::Device& device,
                            wgpu::TextureFormat surfaceFormat,
                            wgpu::CompositeAlphaMode alphaMode,
                            wgpu::PresentMode presentMode,
                            std::uint32_t width,
                            std::uint32_t height)
{
    const wgpu::SurfaceConfiguration configuration{
        .device = device,
        .format = surfaceFormat,
        .usage = wgpu::TextureUsage::RenderAttachment,
        .width = width,
        .height = height,
        .alphaMode = alphaMode,
        .presentMode = presentMode,
    };
    surface.Configure(&configuration);
}

struct PresentedFrameResult
{
    wgpu::SurfaceGetCurrentTextureStatus textureStatus{wgpu::SurfaceGetCurrentTextureStatus::Error};
    wgpu::Status presentStatus{wgpu::Status::Error};
    wgpu::QueueWorkDoneStatus queueStatus{wgpu::QueueWorkDoneStatus::Error};
    double acquiredMilliseconds{0.0};
    double commandSubmittedMilliseconds{0.0};
    double presentedMilliseconds{0.0};
    double queueCompletedMilliseconds{0.0};
    std::string queueMessage;
};

[[nodiscard]] PresentedFrameResult PresentCheckedFrame(const wgpu::Instance& instance,
                                                       const wgpu::Device& device,
                                                       const wgpu::Queue& queue,
                                                       wgpu::Surface& surface,
                                                       const wgpu::RenderPipeline& pipeline,
                                                       const ProbeOptions& options,
                                                       Clock::time_point epoch)
{
    PresentedFrameResult result{};

    wgpu::SurfaceTexture surfaceTexture{};
    surface.GetCurrentTexture(&surfaceTexture);
    result.textureStatus = surfaceTexture.status;
    result.acquiredMilliseconds = MillisecondsSince(epoch, Clock::now());
    if (!IsUsableSurfaceTextureStatus(surfaceTexture.status) || surfaceTexture.texture == nullptr)
    {
        return result;
    }

    SubmitPresentationFrame(device, queue, pipeline, surfaceTexture.texture);
    result.commandSubmittedMilliseconds = MillisecondsSince(epoch, Clock::now());
    result.presentStatus = surface.Present();
    result.presentedMilliseconds = MillisecondsSince(epoch, Clock::now());
    surfaceTexture.texture = nullptr;
    if (result.presentStatus != wgpu::Status::Success)
    {
        return result;
    }

    QueueDoneResult queueDoneResult = WaitForQueue(instance, queue, TimeoutNanoseconds(options));
    result.queueCompletedMilliseconds = MillisecondsSince(epoch, Clock::now());
    result.queueStatus = queueDoneResult.status;
    result.queueMessage = std::move(queueDoneResult.message);
    return result;
}

[[nodiscard]] QueueDoneResult WaitForQueue(const wgpu::Instance& instance,
                                           const wgpu::Queue& queue,
                                           std::uint64_t timeoutNanoseconds)
{
    QueueDoneResult result{};
    auto callback = [&result](wgpu::QueueWorkDoneStatus status, wgpu::StringView message) {
        result.status = status;
        result.message = ToString(message);
    };

    const wgpu::Future future = queue.OnSubmittedWorkDone(wgpu::CallbackMode::WaitAnyOnly, callback);
    if (!WaitForFuture(instance, future, timeoutNanoseconds))
    {
        result.status = wgpu::QueueWorkDoneStatus::Error;
        result.message = "Timed out while waiting for queue work completion.";
    }

    return result;
}

[[nodiscard]] std::optional<std::size_t> SelectAdapterIndex(CandidateKind candidateKind,
                                                            const std::vector<AdapterInventoryRecord>& records)
{
    if (candidateKind == CandidateKind::Hardware)
    {
        return SelectHardwareAdapterIndex(records);
    }

    return SelectSwiftShaderAdapterIndex(records);
}

[[nodiscard]] bool FramePresentedSuccessfully(const PresentedFrameResult& frameResult)
{
    return IsUsableSurfaceTextureStatus(frameResult.textureStatus) && frameResult.presentStatus == wgpu::Status::Success &&
           frameResult.queueStatus == wgpu::QueueWorkDoneStatus::Success;
}

void RecordPresentedFrame(PresentationResult& result,
                          ResizeStepResult* resizeStep,
                          const PresentedFrameResult& frameResult)
{
    if (result.successfulFrameCount == 0)
    {
        result.currentTextureStatus = frameResult.textureStatus;
        result.presentStatus = frameResult.presentStatus;
        result.timings.firstFrameAcquiredMilliseconds = frameResult.acquiredMilliseconds;
        result.timings.commandEncodedMilliseconds = frameResult.commandSubmittedMilliseconds;
        result.timings.firstQueueSubmittedMilliseconds = frameResult.commandSubmittedMilliseconds;
        result.timings.firstPresentedMilliseconds = frameResult.presentedMilliseconds;
        result.timings.queueCompletedMilliseconds = frameResult.queueCompletedMilliseconds;
    }

    if (IsUnexpectedSurfaceTextureStatus(frameResult.textureStatus))
    {
        result.sawUnexpectedSurfaceState = true;
    }

    ++result.successfulFrameCount;
    if (resizeStep != nullptr)
    {
        ++resizeStep->framesPresented;
        resizeStep->surfaceStatuses.push_back(ToString(frameResult.textureStatus));
    }
}

[[nodiscard]] bool PresentFrameSequence(const wgpu::Instance& instance,
                                        const wgpu::Device& device,
                                        const wgpu::Queue& queue,
                                        wgpu::Surface& surface,
                                        const wgpu::RenderPipeline& pipeline,
                                        const ProbeOptions& options,
                                        Clock::time_point epoch,
                                        int frameCount,
                                        PresentationResult& result,
                                        ResizeStepResult* resizeStep = nullptr)
{
    for (int frameIndex = 0; frameIndex < frameCount; ++frameIndex)
    {
        PresentedFrameResult frameResult = PresentCheckedFrame(instance, device, queue, surface, pipeline, options, epoch);
        if (!FramePresentedSuccessfully(frameResult))
        {
            result.currentTextureStatus = frameResult.textureStatus;
            result.presentStatus = frameResult.presentStatus;
            result.sawUnexpectedSurfaceState = result.sawUnexpectedSurfaceState ||
                                               IsUnexpectedSurfaceTextureStatus(frameResult.textureStatus);
            MarkFailure(result,
                        "present_frame",
                        "Frame presentation failed: texture_status=" + ToString(frameResult.textureStatus) +
                            " present_status=" + ToString(frameResult.presentStatus) +
                            " queue_status=" + ToString(frameResult.queueStatus) + " queue_message=\"" +
                            frameResult.queueMessage + "\".");
            return false;
        }

        RecordPresentedFrame(result, resizeStep, frameResult);
    }

    return true;
}

[[nodiscard]] bool WaitForWindowSize(greenfield::SdlWindow& window,
                                     int width,
                                     int height,
                                     const ProbeOptions& options)
{
    const auto waitStart = Clock::now();
    while (!window.ShouldClose())
    {
        window.PollEvents();
        if (window.GetWidth() == width && window.GetHeight() == height)
        {
            return true;
        }

        if (std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - waitStart).count() >=
            static_cast<long long>(options.timeoutMilliseconds))
        {
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }

    return false;
}

[[nodiscard]] ResizeStepResult RunResizeStep(greenfield::SdlWindow& window,
                                             ResizeEventCounter& resizeEventCounter,
                                             wgpu::Surface& surface,
                                             const wgpu::Device& device,
                                             const wgpu::Queue& queue,
                                             const wgpu::Instance& instance,
                                             const wgpu::RenderPipeline& pipeline,
                                             const ProbeOptions& options,
                                             Clock::time_point epoch,
                                             wgpu::TextureFormat surfaceFormat,
                                             wgpu::CompositeAlphaMode alphaMode,
                                             wgpu::PresentMode presentMode,
                                             std::string label,
                                             int width,
                                             int height,
                                             PresentationResult& result)
{
    ResizeStepResult step{};
    step.label = std::move(label);
    step.requestedWidth = width;
    step.requestedHeight = height;
    const int previousResizeEvents = resizeEventCounter.resizeEventsReceived;
    const int previousPixelSizeEvents = resizeEventCounter.pixelSizeEventsReceived;

    step.setWindowSizeSucceeded = SDL_SetWindowSize(window.GetNativeWindow(), width, height);
    step.syncWindowSucceeded = SDL_SyncWindow(window.GetNativeWindow());
    step.reachedRequestedSize = WaitForWindowSize(window, width, height, options);
    step.resizeEventReceived = resizeEventCounter.resizeEventsReceived > previousResizeEvents;
    step.pixelSizeEventReceived = resizeEventCounter.pixelSizeEventsReceived > previousPixelSizeEvents;
    result.resizeEventsReceived = resizeEventCounter.resizeEventsReceived;
    result.pixelSizeEventsReceived = resizeEventCounter.pixelSizeEventsReceived;

    if (!step.setWindowSizeSucceeded)
    {
        MarkFailure(result, "resize_set_window_size", "SDL_SetWindowSize failed: " + std::string(SDL_GetError()));
        return step;
    }

    if (!step.syncWindowSucceeded)
    {
        MarkFailure(result, "resize_sync_window", "SDL_SyncWindow failed: " + std::string(SDL_GetError()));
        return step;
    }

    if (!step.reachedRequestedSize)
    {
        MarkFailure(result, "resize_wait", "Timed out waiting for SDL to report the requested window size.");
        return step;
    }

    surface.Unconfigure();
    step.configuredWidth = window.GetSurfacePixelWidth();
    step.configuredHeight = window.GetSurfacePixelHeight();
    ConfigureNativeSurface(surface,
                           device,
                           surfaceFormat,
                           alphaMode,
                           presentMode,
                           static_cast<std::uint32_t>(step.configuredWidth),
                           static_cast<std::uint32_t>(step.configuredHeight));

    PresentFrameSequence(instance, device, queue, surface, pipeline, options, epoch, options.presentFrameCount, result, &step);
    return step;
}

[[nodiscard]] PresentationResult RunPresentationCandidate(CandidateKind candidateKind,
                                                         const ProbeOptions& options,
                                                         Clock::time_point epoch)
{
    PresentationResult result{};
    result.candidateKind = candidateKind;

    try
    {
        greenfield::SdlWindow window{"Greenfield Native Presentation Probe", options.windowWidth, options.windowHeight};
        result.timings.windowReadyMilliseconds = MillisecondsSince(epoch, Clock::now());
        result.currentVideoDriver = GetCurrentSdlVideoDriver();
        result.availableVideoDrivers = GetAvailableSdlVideoDrivers();
        result.surfacePixelWidth = window.GetSurfacePixelWidth();
        result.surfacePixelHeight = window.GetSurfacePixelHeight();
        ResizeEventCounter resizeEventCounter{.windowId = SDL_GetWindowID(window.GetNativeWindow())};
        ScopedSdlEventWatch resizeEventWatch{resizeEventCounter};

        {
            greenfield::SdlStartupPresenter startupPresenter{window};
            startupPresenter.DrawFrame();
            result.timings.startupFrameVisibleMilliseconds = MillisecondsSince(epoch, Clock::now());
            result.hadSdlWindowSurfaceBeforeRelease = SDL_WindowHasSurface(window.GetNativeWindow());
        }

        result.timings.startupSurfaceReleasedMilliseconds = MillisecondsSince(epoch, Clock::now());
        result.hasSdlWindowSurfaceAfterRelease = SDL_WindowHasSurface(window.GetNativeWindow());
        const greenfield::NativeSurfaceDescriptor nativeSurface = window.GetNativeSurfaceDescriptor();
        result.nativeSurfaceKind = nativeSurface.kind;
        if (nativeSurface.kind == greenfield::NativeSurfaceKind::None)
        {
            MarkFailure(result, "native_surface", "SDL window did not expose Wayland or X11 native surface handles.");
            return result;
        }

        const wgpu::InstanceDescriptor instanceDescriptor = MakeProbeInstanceDescriptor();
        dawn::native::Instance nativeInstance{&instanceDescriptor};
        wgpu::Instance instance{nativeInstance.Get()};
        if (instance == nullptr)
        {
            MarkFailure(result, "instance", "Failed to create WebGPU instance.");
            return result;
        }

        wgpu::Surface surface = CreateNativeWebGpuSurface(instance, nativeSurface);
        if (surface == nullptr)
        {
            MarkFailure(result, "surface", "Failed to create WebGPU surface from SDL native handles.");
            return result;
        }
        result.timings.webGpuSurfaceCreatedMilliseconds = MillisecondsSince(epoch, Clock::now());

        std::vector<AdapterInventoryRecord> adapterRecords = EnumerateVulkanAdapters(nativeInstance);
        const std::optional<std::size_t> selectedAdapterIndex = SelectAdapterIndex(candidateKind, adapterRecords);
        if (!selectedAdapterIndex.has_value())
        {
            MarkFailure(result, "adapter_selection", BuildSelectionFailureMessage(candidateKind, adapterRecords));
            return result;
        }

        const AdapterInventoryRecord& selectedAdapter = adapterRecords[*selectedAdapterIndex];
        result.adapterIdentity = selectedAdapter.identity;
        result.adapterIndex = selectedAdapter.index;
        result.selectionReason = selectedAdapter.classificationReason;
        result.timings.adapterSelectedMilliseconds = MillisecondsSince(epoch, Clock::now());
        PrintSelection(candidateKind, selectedAdapter);

        wgpu::DeviceDescriptor deviceDescriptor{};
        deviceDescriptor.SetDeviceLostCallback(
            wgpu::CallbackMode::AllowSpontaneous,
            [](const wgpu::Device&, wgpu::DeviceLostReason, wgpu::StringView message) {
                const std::string messageText = ToString(message);
                if (messageText != "Device was destroyed.")
                {
                    std::cerr << "WebGPU device lost during native presentation probe: " << messageText << '\n';
                }
            });
        deviceDescriptor.SetUncapturedErrorCallback([](const wgpu::Device&, wgpu::ErrorType, wgpu::StringView message) {
            std::cerr << "WebGPU uncaptured error during native presentation probe: " << ToString(message) << '\n';
        });

        RequestDeviceResult deviceRequestResult{};
        auto deviceCallback =
            [&deviceRequestResult](wgpu::RequestDeviceStatus status, wgpu::Device device, wgpu::StringView message) {
                deviceRequestResult.status = status;
                deviceRequestResult.device = device;
                deviceRequestResult.message = ToString(message);
            };

        const wgpu::Future deviceFuture =
            selectedAdapter.adapter.RequestDevice(&deviceDescriptor, wgpu::CallbackMode::WaitAnyOnly, deviceCallback);
        if (!WaitForFuture(instance, deviceFuture, TimeoutNanoseconds(options)))
        {
            MarkFailure(result, "device", "Timed out while requesting device.");
            return result;
        }
        result.timings.deviceRequestedMilliseconds = MillisecondsSince(epoch, Clock::now());
        if (deviceRequestResult.status != wgpu::RequestDeviceStatus::Success || deviceRequestResult.device == nullptr)
        {
            MarkFailure(result, "device", "Device request failed: " + deviceRequestResult.message);
            return result;
        }

        wgpu::Queue queue = deviceRequestResult.device.GetQueue();
        if (queue == nullptr)
        {
            MarkFailure(result, "queue", "Device returned a null queue.");
            return result;
        }
        result.timings.queueAcquiredMilliseconds = MillisecondsSince(epoch, Clock::now());

        wgpu::SurfaceCapabilities capabilities{};
        if (surface.GetCapabilities(selectedAdapter.adapter, &capabilities) != wgpu::Status::Success)
        {
            MarkFailure(result, "surface_capabilities", "Failed to query native WebGPU surface capabilities.");
            return result;
        }
        result.timings.surfaceCapabilitiesMilliseconds = MillisecondsSince(epoch, Clock::now());

        result.surfaceFormat = ChooseSurfaceFormat(capabilities);
        result.alphaMode = ChooseAlphaMode(capabilities);
        if (result.surfaceFormat == wgpu::TextureFormat::Undefined)
        {
            MarkFailure(result, "surface_capabilities", "Native WebGPU surface reported no usable formats.");
            return result;
        }

        ConfigureNativeSurface(surface,
                               deviceRequestResult.device,
                               result.surfaceFormat,
                               result.alphaMode,
                               result.presentMode,
                               static_cast<std::uint32_t>(result.surfacePixelWidth),
                               static_cast<std::uint32_t>(result.surfacePixelHeight));
        result.timings.surfaceConfiguredMilliseconds = MillisecondsSince(epoch, Clock::now());

        wgpu::ShaderModule shaderModule = CreateProbeShaderModule(deviceRequestResult.device);
        if (shaderModule == nullptr)
        {
            MarkFailure(result, "shader", "Failed to create shader module.");
            return result;
        }
        result.timings.shaderModuleCreatedMilliseconds = MillisecondsSince(epoch, Clock::now());

        wgpu::RenderPipeline pipeline = CreateProbePipeline(deviceRequestResult.device, shaderModule, result.surfaceFormat);
        if (pipeline == nullptr)
        {
            MarkFailure(result, "pipeline", "Failed to create render pipeline.");
            return result;
        }
        result.timings.pipelineCreatedMilliseconds = MillisecondsSince(epoch, Clock::now());

        if (!PresentFrameSequence(instance,
                                  deviceRequestResult.device,
                                  queue,
                                  surface,
                                  pipeline,
                                  options,
                                  epoch,
                                  options.presentFrameCount,
                                  result))
        {
            return result;
        }

        if (options.resizeExercise)
        {
            result.resizeSteps.push_back(RunResizeStep(window,
                                                       resizeEventCounter,
                                                       surface,
                                                       deviceRequestResult.device,
                                                       queue,
                                                       instance,
                                                       pipeline,
                                                       options,
                                                       epoch,
                                                       result.surfaceFormat,
                                                       result.alphaMode,
                                                       result.presentMode,
                                                       "smaller",
                                                       640,
                                                       360,
                                                       result));
            if (!result.failureStage.empty())
            {
                return result;
            }

            result.resizeSteps.push_back(RunResizeStep(window,
                                                       resizeEventCounter,
                                                       surface,
                                                       deviceRequestResult.device,
                                                       queue,
                                                       instance,
                                                       pipeline,
                                                       options,
                                                       epoch,
                                                       result.surfaceFormat,
                                                       result.alphaMode,
                                                       result.presentMode,
                                                       "larger",
                                                       1280,
                                                       720,
                                                       result));
            if (!result.failureStage.empty())
            {
                return result;
            }
        }
        else
        {
            const auto holdStart = Clock::now();
            while (!window.ShouldClose() &&
                   std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - holdStart).count() <
                       static_cast<long long>(options.presentHoldMilliseconds))
            {
                window.PollEvents();
                std::this_thread::sleep_for(std::chrono::milliseconds(8));
            }
        }

        result.resizeEventsReceived = resizeEventCounter.resizeEventsReceived;
        result.pixelSizeEventsReceived = resizeEventCounter.pixelSizeEventsReceived;
        result.timings.postPresentPumpCompletedMilliseconds = MillisecondsSince(epoch, Clock::now());
        surface.Unconfigure();
        surface = nullptr;
        result.success = true;
    }
    catch (const std::exception& exception)
    {
        MarkFailure(result, "exception", exception.what());
    }

    return result;
}

[[nodiscard]] bool VerifyReadbackPixels(const std::uint8_t* pixels, std::uint32_t rowPitch, std::uint32_t height)
{
    for (std::uint32_t row = 0; row < height; ++row)
    {
        const std::uint8_t* rowPixels = pixels + row * rowPitch;
        for (std::uint32_t column = 0; column < 64; ++column)
        {
            const std::uint8_t* pixel = rowPixels + column * 4U;
            if (pixel[0] > 180U && pixel[1] < 120U && pixel[2] < 100U && pixel[3] > 200U)
            {
                return true;
            }
        }
    }

    return false;
}

[[nodiscard]] bool VerifyRenderTarget(const wgpu::Instance& instance,
                                      const wgpu::Device& device,
                                      const wgpu::Queue& queue,
                                      const wgpu::Texture& renderTarget,
                                      const ProbeOptions& options,
                                      std::string& message)
{
    constexpr std::uint32_t TextureWidth = 64;
    constexpr std::uint32_t TextureHeight = 64;
    constexpr std::uint32_t ReadbackRowPitch = 256;
    constexpr std::uint64_t ReadbackByteCount = ReadbackRowPitch * TextureHeight;

    wgpu::Buffer readbackBuffer = CreateReadbackBuffer(device, ReadbackByteCount);
    const wgpu::TexelCopyTextureInfo source{
        .texture = renderTarget,
        .mipLevel = 0,
        .origin = {},
        .aspect = wgpu::TextureAspect::All,
    };
    const wgpu::TexelCopyBufferInfo destination{
        .layout =
            wgpu::TexelCopyBufferLayout{
                .offset = 0,
                .bytesPerRow = ReadbackRowPitch,
                .rowsPerImage = TextureHeight,
            },
        .buffer = readbackBuffer,
    };
    const wgpu::Extent3D copySize{
        .width = TextureWidth,
        .height = TextureHeight,
        .depthOrArrayLayers = 1,
    };

    wgpu::CommandEncoder commandEncoder = device.CreateCommandEncoder();
    commandEncoder.CopyTextureToBuffer(&source, &destination, &copySize);
    wgpu::CommandBuffer commandBuffer = commandEncoder.Finish();
    queue.Submit(1, &commandBuffer);

    QueueDoneResult queueDoneResult = WaitForQueue(instance, queue, TimeoutNanoseconds(options));
    if (queueDoneResult.status != wgpu::QueueWorkDoneStatus::Success)
    {
        message = "Readback copy did not complete: " + queueDoneResult.message;
        return false;
    }

    MapResult mapResult{};
    auto mapCallback = [&mapResult](wgpu::MapAsyncStatus status, wgpu::StringView callbackMessage) {
        mapResult.status = status;
        mapResult.message = ToString(callbackMessage);
    };

    const wgpu::Future mapFuture =
        readbackBuffer.MapAsync(wgpu::MapMode::Read, 0, ReadbackByteCount, wgpu::CallbackMode::WaitAnyOnly, mapCallback);
    if (!WaitForFuture(instance, mapFuture, TimeoutNanoseconds(options)))
    {
        message = "Timed out while mapping readback buffer.";
        return false;
    }

    if (mapResult.status != wgpu::MapAsyncStatus::Success)
    {
        message = "Readback map failed: " + mapResult.message;
        return false;
    }

    const auto* mappedPixels = static_cast<const std::uint8_t*>(readbackBuffer.GetConstMappedRange(0, ReadbackByteCount));
    const bool verified = mappedPixels != nullptr && VerifyReadbackPixels(mappedPixels, ReadbackRowPitch, TextureHeight);
    readbackBuffer.Unmap();

    if (!verified)
    {
        message = "Readback completed but did not find the deterministic triangle color.";
        return false;
    }

    message = "Readback found the deterministic triangle color.";
    return true;
}

[[nodiscard]] CandidateResult RunCandidate(CandidateKind candidateKind,
                                           ProbeOptions options,
                                           Clock::time_point epoch,
                                           std::latch& startLatch)
{
    CandidateResult result{};
    result.candidateKind = candidateKind;

    startLatch.arrive_and_wait();

    try
    {
        const wgpu::InstanceDescriptor instanceDescriptor = MakeProbeInstanceDescriptor();
        dawn::native::Instance nativeInstance{&instanceDescriptor};
        wgpu::Instance instance{nativeInstance.Get()};
        if (instance == nullptr)
        {
            MarkFailure(result, "instance", "Failed to create WebGPU instance.");
            return result;
        }
        result.timings.instanceCreatedMilliseconds = MillisecondsSince(epoch, Clock::now());

        const auto adapterSelectionStart = Clock::now();
        std::vector<AdapterInventoryRecord> adapterRecords = EnumerateVulkanAdapters(nativeInstance);
        const std::optional<std::size_t> selectedAdapterIndex =
            candidateKind == CandidateKind::Hardware ? SelectHardwareAdapterIndex(adapterRecords)
                                                     : SelectSwiftShaderAdapterIndex(adapterRecords);
        result.timings.adapterRequestedMilliseconds =
            std::chrono::duration<double, std::milli>(Clock::now() - adapterSelectionStart).count();

        if (!selectedAdapterIndex.has_value())
        {
            MarkFailure(result, "adapter_selection", BuildSelectionFailureMessage(candidateKind, adapterRecords));
            return result;
        }

        const AdapterInventoryRecord& selectedAdapter = adapterRecords[*selectedAdapterIndex];
        if (selectedAdapter.adapter == nullptr)
        {
            MarkFailure(result, "adapter_selection", "Selected adapter record carried a null adapter.");
            return result;
        }

        result.adapterIdentity = selectedAdapter.identity;
        result.adapterIndex = selectedAdapter.index;
        result.selectionReason = selectedAdapter.classificationReason;
        result.timings.adapterInfoQueriedMilliseconds = MillisecondsSince(epoch, Clock::now());
        PrintSelection(candidateKind, selectedAdapter);

        wgpu::DeviceDescriptor deviceDescriptor{};
        deviceDescriptor.SetDeviceLostCallback(
            wgpu::CallbackMode::AllowSpontaneous,
            [](const wgpu::Device&, wgpu::DeviceLostReason, wgpu::StringView message) {
                const std::string messageText = ToString(message);
                if (messageText != "Device was destroyed.")
                {
                    std::cerr << "WebGPU device lost during startup probe: " << messageText << '\n';
                }
            });
        deviceDescriptor.SetUncapturedErrorCallback([](const wgpu::Device&, wgpu::ErrorType, wgpu::StringView message) {
            std::cerr << "WebGPU uncaptured error during startup probe: " << ToString(message) << '\n';
        });

        const auto deviceRequestStart = Clock::now();
        RequestDeviceResult deviceRequestResult{};
        auto deviceCallback =
            [&deviceRequestResult](wgpu::RequestDeviceStatus status, wgpu::Device device, wgpu::StringView message) {
                deviceRequestResult.status = status;
                deviceRequestResult.device = device;
                deviceRequestResult.message = ToString(message);
            };

        const wgpu::Future deviceFuture =
            selectedAdapter.adapter.RequestDevice(&deviceDescriptor, wgpu::CallbackMode::WaitAnyOnly, deviceCallback);
        if (!WaitForFuture(instance, deviceFuture, TimeoutNanoseconds(options)))
        {
            MarkFailure(result, "device", "Timed out while requesting device.");
            return result;
        }
        result.timings.deviceRequestedMilliseconds =
            std::chrono::duration<double, std::milli>(Clock::now() - deviceRequestStart).count();

        if (deviceRequestResult.status != wgpu::RequestDeviceStatus::Success || deviceRequestResult.device == nullptr)
        {
            MarkFailure(result, "device", "Device request failed: " + deviceRequestResult.message);
            return result;
        }

        wgpu::Queue queue = deviceRequestResult.device.GetQueue();
        if (queue == nullptr)
        {
            MarkFailure(result, "queue", "Device returned a null queue.");
            return result;
        }
        result.timings.queueAcquiredMilliseconds = MillisecondsSince(epoch, Clock::now());

        wgpu::ShaderModule shaderModule = CreateProbeShaderModule(deviceRequestResult.device);
        if (shaderModule == nullptr)
        {
            MarkFailure(result, "shader", "Failed to create shader module.");
            return result;
        }
        result.timings.shaderModuleCreatedMilliseconds = MillisecondsSince(epoch, Clock::now());

        wgpu::RenderPipeline pipeline =
            CreateProbePipeline(deviceRequestResult.device, shaderModule, wgpu::TextureFormat::RGBA8Unorm);
        if (pipeline == nullptr)
        {
            MarkFailure(result, "pipeline", "Failed to create render pipeline.");
            return result;
        }
        result.timings.pipelineCreatedMilliseconds = MillisecondsSince(epoch, Clock::now());

        wgpu::Texture renderTarget = CreateRenderTarget(deviceRequestResult.device);
        if (renderTarget == nullptr)
        {
            MarkFailure(result, "resources", "Failed to create offscreen render target.");
            return result;
        }
        result.timings.resourcesCreatedMilliseconds = MillisecondsSince(epoch, Clock::now());

        SubmitProbeRenderPass(deviceRequestResult.device, queue, pipeline, renderTarget);
        result.timings.commandEncodedMilliseconds = MillisecondsSince(epoch, Clock::now());
        result.timings.firstQueueSubmittedMilliseconds = result.timings.commandEncodedMilliseconds;

        QueueDoneResult queueDoneResult = WaitForQueue(instance, queue, TimeoutNanoseconds(options));
        result.timings.queueCompletedMilliseconds = MillisecondsSince(epoch, Clock::now());
        if (queueDoneResult.status != wgpu::QueueWorkDoneStatus::Success)
        {
            MarkFailure(result, "queue_complete", "Queue completion failed: " + queueDoneResult.message);
            return result;
        }

        result.renderVerified =
            VerifyRenderTarget(instance, deviceRequestResult.device, queue, renderTarget, options, result.renderVerificationMessage);
        result.timings.readbackCompletedMilliseconds = MillisecondsSince(epoch, Clock::now());

        if (!result.renderVerified)
        {
            MarkFailure(result, "readback", result.renderVerificationMessage);
            return result;
        }

        result.success = true;
    }
    catch (const std::exception& exception)
    {
        MarkFailure(result, "exception", exception.what());
    }

    return result;
}

[[nodiscard]] StartupFrameResult RunStartupLane(const ProbeOptions& options,
                                                Clock::time_point epoch,
                                                std::latch& startLatch,
                                                const std::vector<std::future<CandidateResult>*>& futures)
{
    StartupFrameResult result{};
    startLatch.arrive_and_wait();

    try
    {
        greenfield::SdlWindow window{"Greenfield Renderer Startup Probe", options.windowWidth, options.windowHeight};
        result.windowReadyMilliseconds = MillisecondsSince(epoch, Clock::now());

        greenfield::SdlStartupPresenter startupPresenter{window};
        startupPresenter.DrawFrame();
        result.startupFrameVisibleMilliseconds = MillisecondsSince(epoch, Clock::now());
        result.success = true;

        bool allCandidatesDone = false;
        while (!window.ShouldClose() && !allCandidatesDone)
        {
            window.PollEvents();
            allCandidatesDone = std::all_of(futures.begin(), futures.end(), [](std::future<CandidateResult>* future) {
                return future->wait_for(std::chrono::milliseconds(0)) == std::future_status::ready;
            });
            std::this_thread::sleep_for(std::chrono::milliseconds(8));
        }
    }
    catch (const std::exception& exception)
    {
        result.success = false;
        result.failureMessage = exception.what();
    }

    return result;
}

[[nodiscard]] std::vector<CandidateKind> GetCandidatesForMode(ProbeMode mode)
{
    if (mode == ProbeMode::Hardware)
    {
        return {CandidateKind::Hardware};
    }

    if (mode == ProbeMode::SwiftShader)
    {
        return {CandidateKind::SwiftShader};
    }

    if (mode == ProbeMode::Inventory)
    {
        return {};
    }

    return {CandidateKind::Hardware, CandidateKind::SwiftShader};
}

[[nodiscard]] std::optional<ProbeMode> ParseMode(std::string_view value)
{
    if (value == "hardware")
    {
        return ProbeMode::Hardware;
    }

    if (value == "swiftshader")
    {
        return ProbeMode::SwiftShader;
    }

    if (value == "race")
    {
        return ProbeMode::Race;
    }

    if (value == "inventory")
    {
        return ProbeMode::Inventory;
    }

    if (value == "present-hardware")
    {
        return ProbeMode::PresentHardware;
    }

    if (value == "present-swiftshader")
    {
        return ProbeMode::PresentSwiftShader;
    }

    return std::nullopt;
}

[[nodiscard]] bool ParseWindowSize(std::string_view value, int& width, int& height)
{
    const std::size_t separator = value.find('x');
    if (separator == std::string_view::npos)
    {
        return false;
    }

    width = std::stoi(std::string(value.substr(0, separator)));
    height = std::stoi(std::string(value.substr(separator + 1)));
    return width > 0 && height > 0;
}

[[nodiscard]] ProbeOptions ParseOptions(int argc, char** argv)
{
    ProbeOptions options{};
    for (int index = 1; index < argc; ++index)
    {
        const std::string_view argument{argv[index]};
        if (argument.rfind("--mode=", 0) == 0)
        {
            std::optional<ProbeMode> parsedMode = ParseMode(argument.substr(7));
            if (!parsedMode.has_value())
            {
                throw std::invalid_argument("Expected --mode=hardware, --mode=swiftshader, --mode=race, "
                                            "--mode=inventory, --mode=present-hardware, or --mode=present-swiftshader.");
            }
            options.mode = *parsedMode;
        }
        else if (argument.rfind("--window-size=", 0) == 0)
        {
            if (!ParseWindowSize(argument.substr(14), options.windowWidth, options.windowHeight))
            {
                throw std::invalid_argument("Expected --window-size=<width>x<height>.");
            }
        }
        else if (argument.rfind("--timeout-ms=", 0) == 0)
        {
            options.timeoutMilliseconds = static_cast<std::uint64_t>(std::stoull(std::string(argument.substr(13))));
        }
        else if (argument.rfind("--present-hold-ms=", 0) == 0)
        {
            options.presentHoldMilliseconds =
                static_cast<std::uint64_t>(std::stoull(std::string(argument.substr(18))));
        }
        else if (argument.rfind("--present-frame-count=", 0) == 0)
        {
            options.presentFrameCount = std::stoi(std::string(argument.substr(22)));
            if (options.presentFrameCount <= 0)
            {
                throw std::invalid_argument("Expected --present-frame-count to be greater than zero.");
            }
        }
        else if (argument == "--resize-exercise")
        {
            options.resizeExercise = true;
        }
        else
        {
            throw std::invalid_argument("Unknown option: " + std::string(argument));
        }
    }

    return options;
}

void PrintHumanResult(const CandidateResult& result)
{
    std::cout << "Candidate " << ToString(result.candidateKind) << ": " << (result.success ? "ready" : "failed")
              << '\n';
    std::cout << "  adapter: index=";
    if (result.adapterIndex == std::numeric_limits<std::size_t>::max())
    {
        std::cout << "none";
    }
    else
    {
        std::cout << result.adapterIndex;
    }
    std::cout << " selection_reason=" << (result.selectionReason.empty() ? "none" : result.selectionReason)
              << " backend=" << result.adapterIdentity.backendType
              << " type=" << result.adapterIdentity.adapterType << " vendor=\"" << result.adapterIdentity.vendor
              << "\" device=\"" << result.adapterIdentity.device << "\" description=\""
              << result.adapterIdentity.description << "\" vendor_id=0x" << std::hex << result.adapterIdentity.vendorId
              << " device_id=0x" << result.adapterIdentity.deviceId << std::dec << '\n';
    std::cout << "  first_submit_ms=" << result.timings.firstQueueSubmittedMilliseconds
              << " queue_done_ms=" << result.timings.queueCompletedMilliseconds
              << " readback_done_ms=" << result.timings.readbackCompletedMilliseconds << '\n';
    std::cout << "  verification: " << (result.renderVerified ? "passed" : "failed") << " ("
              << result.renderVerificationMessage << ")\n";
    if (!result.success)
    {
        std::cout << "  failure: stage=" << result.failureStage << " message=\"" << result.failureMessage << "\"\n";
    }
    for (const std::string& message : result.deviceMessages)
    {
        std::cout << "  device_message: " << message << '\n';
    }
}

void PrintMachineResult(ProbeMode mode, const CandidateResult& result)
{
    std::cout << "PROBE_RESULT"
              << " mode=" << ToString(mode)
              << " candidate=" << ToString(result.candidateKind)
              << " success=" << (result.success ? "true" : "false")
              << " failure_stage=" << (result.failureStage.empty() ? "none" : result.failureStage)
              << " adapter_index=";
    if (result.adapterIndex == std::numeric_limits<std::size_t>::max())
    {
        std::cout << "none";
    }
    else
    {
        std::cout << result.adapterIndex;
    }
    std::cout << " selection_reason=" << (result.selectionReason.empty() ? "none" : result.selectionReason)
              << " backend=" << result.adapterIdentity.backendType
              << " adapter_type=" << result.adapterIdentity.adapterType
              << " vendor_id=" << result.adapterIdentity.vendorId
              << " device_id=" << result.adapterIdentity.deviceId
              << " feature_count=" << result.adapterIdentity.featureCount
              << " max_texture_2d=" << result.adapterIdentity.maxTextureDimension2D
              << " instance_created_ms=" << result.timings.instanceCreatedMilliseconds
              << " adapter_request_ms=" << result.timings.adapterRequestedMilliseconds
              << " adapter_info_ms=" << result.timings.adapterInfoQueriedMilliseconds
              << " device_request_ms=" << result.timings.deviceRequestedMilliseconds
              << " queue_acquired_ms=" << result.timings.queueAcquiredMilliseconds
              << " shader_ready_ms=" << result.timings.shaderModuleCreatedMilliseconds
              << " pipeline_ready_ms=" << result.timings.pipelineCreatedMilliseconds
              << " resources_ready_ms=" << result.timings.resourcesCreatedMilliseconds
              << " command_encoded_ms=" << result.timings.commandEncodedMilliseconds
              << " preliminary_ready_ms=" << result.timings.firstQueueSubmittedMilliseconds
              << " queue_complete_ms=" << result.timings.queueCompletedMilliseconds
              << " readback_complete_ms=" << result.timings.readbackCompletedMilliseconds
              << " render_verified=" << (result.renderVerified ? "true" : "false") << '\n';
}

[[nodiscard]] std::string JoinVideoDrivers(const std::vector<std::string>& videoDrivers)
{
    std::ostringstream stream;
    for (std::size_t index = 0; index < videoDrivers.size(); ++index)
    {
        if (index > 0U)
        {
            stream << ',';
        }
        stream << videoDrivers[index];
    }
    return stream.str();
}

void PrintPresentationResult(ProbeMode mode, const PresentationResult& result)
{
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "Greenfield renderer startup probe\n";
    std::cout << "mode=" << ToString(mode) << " candidate=" << ToString(result.candidateKind) << '\n';
    std::cout << "SDL video: current=" << result.currentVideoDriver
              << " available=" << JoinVideoDrivers(result.availableVideoDrivers) << '\n';
    std::cout << "Native surface: kind=" << ToString(result.nativeSurfaceKind)
              << " size=" << result.surfacePixelWidth << 'x' << result.surfacePixelHeight
              << " sdl_surface_before_release=" << (result.hadSdlWindowSurfaceBeforeRelease ? "true" : "false")
              << " sdl_surface_after_release=" << (result.hasSdlWindowSurfaceAfterRelease ? "true" : "false")
              << '\n';
    std::cout << "Presentation candidate " << ToString(result.candidateKind) << ": "
              << (result.success ? "presented" : "failed") << '\n';
    std::cout << "  adapter: index=";
    if (result.adapterIndex == std::numeric_limits<std::size_t>::max())
    {
        std::cout << "none";
    }
    else
    {
        std::cout << result.adapterIndex;
    }
    std::cout << " selection_reason=" << (result.selectionReason.empty() ? "none" : result.selectionReason)
              << " backend=" << result.adapterIdentity.backendType
              << " type=" << result.adapterIdentity.adapterType
              << " vendor=\"" << result.adapterIdentity.vendor
              << "\" device=\"" << result.adapterIdentity.device
              << "\" description=\"" << result.adapterIdentity.description << "\"\n";
    std::cout << "  surface_format=" << ToString(result.surfaceFormat)
              << " alpha_mode=" << ToString(result.alphaMode)
              << " present_mode=" << ToString(result.presentMode)
              << " current_texture_status=" << ToString(result.currentTextureStatus)
              << " present_status=" << ToString(result.presentStatus)
              << " successful_frames=" << result.successfulFrameCount
              << " resize_events=" << result.resizeEventsReceived
              << " pixel_size_events=" << result.pixelSizeEventsReceived
              << " unexpected_surface_state=" << (result.sawUnexpectedSurfaceState ? "true" : "false") << '\n';
    for (const ResizeStepResult& resizeStep : result.resizeSteps)
    {
        std::cout << "  resize_step=" << resizeStep.label
                  << " requested=" << resizeStep.requestedWidth << 'x' << resizeStep.requestedHeight
                  << " configured=" << resizeStep.configuredWidth << 'x' << resizeStep.configuredHeight
                  << " frames_presented=" << resizeStep.framesPresented
                  << " resize_event=" << (resizeStep.resizeEventReceived ? "true" : "false")
                  << " pixel_size_event=" << (resizeStep.pixelSizeEventReceived ? "true" : "false")
                  << " reached_requested_size=" << (resizeStep.reachedRequestedSize ? "true" : "false") << '\n';
    }
    std::cout << "  startup_frame_visible_ms=" << result.timings.startupFrameVisibleMilliseconds
              << " startup_surface_released_ms=" << result.timings.startupSurfaceReleasedMilliseconds
              << " webgpu_surface_created_ms=" << result.timings.webGpuSurfaceCreatedMilliseconds
              << " first_presented_ms=" << result.timings.firstPresentedMilliseconds
              << " queue_done_ms=" << result.timings.queueCompletedMilliseconds << '\n';
    if (!result.success)
    {
        std::cout << "  failure: stage=" << result.failureStage << " message=\"" << result.failureMessage << "\"\n";
    }
    for (const std::string& message : result.deviceMessages)
    {
        std::cout << "  device_message: " << message << '\n';
    }

    const std::string availableDrivers = JoinVideoDrivers(result.availableVideoDrivers);
    std::cout << "PROBE_SDL_VIDEO"
              << " mode=" << ToString(mode)
              << " current=" << result.currentVideoDriver
              << " available=" << QuoteForMachineOutput(availableDrivers) << '\n';
    std::cout << "PROBE_NATIVE_SURFACE"
              << " mode=" << ToString(mode)
              << " kind=" << ToString(result.nativeSurfaceKind)
              << " width=" << result.surfacePixelWidth
              << " height=" << result.surfacePixelHeight
              << " sdl_surface_before_release=" << (result.hadSdlWindowSurfaceBeforeRelease ? "true" : "false")
              << " sdl_surface_after_release=" << (result.hasSdlWindowSurfaceAfterRelease ? "true" : "false")
              << '\n';
    std::cout << "PROBE_PRESENT_RESULT"
              << " mode=" << ToString(mode)
              << " candidate=" << ToString(result.candidateKind)
              << " success=" << (result.success ? "true" : "false")
              << " failure_stage=" << (result.failureStage.empty() ? "none" : result.failureStage)
              << " adapter_index=";
    if (result.adapterIndex == std::numeric_limits<std::size_t>::max())
    {
        std::cout << "none";
    }
    else
    {
        std::cout << result.adapterIndex;
    }
    std::cout << " selection_reason=" << (result.selectionReason.empty() ? "none" : result.selectionReason)
              << " backend=" << result.adapterIdentity.backendType
              << " adapter_type=" << result.adapterIdentity.adapterType
              << " vendor_id=" << result.adapterIdentity.vendorId
              << " device_id=" << result.adapterIdentity.deviceId
              << " surface_format=" << ToString(result.surfaceFormat)
              << " alpha_mode=" << ToString(result.alphaMode)
              << " present_mode=" << ToString(result.presentMode)
              << " current_texture_status=" << ToString(result.currentTextureStatus)
              << " present_status=" << ToString(result.presentStatus)
              << " successful_frames=" << result.successfulFrameCount
              << " resize_events=" << result.resizeEventsReceived
              << " pixel_size_events=" << result.pixelSizeEventsReceived
              << " unexpected_surface_state=" << (result.sawUnexpectedSurfaceState ? "true" : "false")
              << " window_ready_ms=" << result.timings.windowReadyMilliseconds
              << " startup_frame_visible_ms=" << result.timings.startupFrameVisibleMilliseconds
              << " startup_surface_released_ms=" << result.timings.startupSurfaceReleasedMilliseconds
              << " webgpu_surface_created_ms=" << result.timings.webGpuSurfaceCreatedMilliseconds
              << " adapter_selected_ms=" << result.timings.adapterSelectedMilliseconds
              << " device_request_ms=" << result.timings.deviceRequestedMilliseconds
              << " queue_acquired_ms=" << result.timings.queueAcquiredMilliseconds
              << " surface_capabilities_ms=" << result.timings.surfaceCapabilitiesMilliseconds
              << " surface_configured_ms=" << result.timings.surfaceConfiguredMilliseconds
              << " shader_ready_ms=" << result.timings.shaderModuleCreatedMilliseconds
              << " pipeline_ready_ms=" << result.timings.pipelineCreatedMilliseconds
              << " first_frame_acquired_ms=" << result.timings.firstFrameAcquiredMilliseconds
              << " command_encoded_ms=" << result.timings.commandEncodedMilliseconds
              << " preliminary_ready_ms=" << result.timings.firstQueueSubmittedMilliseconds
              << " first_presented_ms=" << result.timings.firstPresentedMilliseconds
              << " queue_complete_ms=" << result.timings.queueCompletedMilliseconds
              << " post_present_pump_ms=" << result.timings.postPresentPumpCompletedMilliseconds
              << '\n';
    for (const ResizeStepResult& resizeStep : result.resizeSteps)
    {
        std::cout << "PROBE_RESIZE_STEP"
                  << " mode=" << ToString(mode)
                  << " candidate=" << ToString(result.candidateKind)
                  << " label=" << resizeStep.label
                  << " requested_width=" << resizeStep.requestedWidth
                  << " requested_height=" << resizeStep.requestedHeight
                  << " configured_width=" << resizeStep.configuredWidth
                  << " configured_height=" << resizeStep.configuredHeight
                  << " frames_presented=" << resizeStep.framesPresented
                  << " resize_event=" << (resizeStep.resizeEventReceived ? "true" : "false")
                  << " pixel_size_event=" << (resizeStep.pixelSizeEventReceived ? "true" : "false")
                  << " set_window_size=" << (resizeStep.setWindowSizeSucceeded ? "true" : "false")
                  << " sync_window=" << (resizeStep.syncWindowSucceeded ? "true" : "false")
                  << " reached_requested_size=" << (resizeStep.reachedRequestedSize ? "true" : "false")
                  << " surface_statuses=" << QuoteForMachineOutput(JoinVideoDrivers(resizeStep.surfaceStatuses))
                  << '\n';
    }
}

void PrintAdapterInventoryRecord(const AdapterInventoryRecord& record)
{
    const AdapterIdentity& identity = record.identity;
    std::cout << "PROBE_ADAPTER"
              << " index=" << record.index
              << " backend=" << identity.backendType
              << " type=" << identity.adapterType
              << " vendor_id=" << identity.vendorId
              << " device_id=" << identity.deviceId
              << " vendor=" << QuoteForMachineOutput(identity.vendor)
              << " architecture=" << QuoteForMachineOutput(identity.architecture)
              << " name=" << QuoteForMachineOutput(identity.device)
              << " description=" << QuoteForMachineOutput(identity.description)
              << " feature_count=" << identity.featureCount
              << " max_texture_2d=" << identity.maxTextureDimension2D
              << " subgroup_min=" << identity.subgroupMinSize
              << " subgroup_max=" << identity.subgroupMaxSize
              << " swiftshader=" << (record.isSwiftShader ? "true" : "false")
              << " hardware=" << (record.isHardware ? "true" : "false")
              << " hardware_rank=" << record.hardwareRank
              << " classification_reason=" << record.classificationReason << '\n';
}

int RunInventoryMode()
{
    const wgpu::InstanceDescriptor instanceDescriptor = MakeProbeInstanceDescriptor();
    dawn::native::Instance nativeInstance{&instanceDescriptor};
    const std::vector<AdapterInventoryRecord> adapterRecords = EnumerateVulkanAdapters(nativeInstance);

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "Greenfield renderer startup probe\n";
    std::cout << "mode=inventory adapters=" << adapterRecords.size() << '\n';
    for (const AdapterInventoryRecord& record : adapterRecords)
    {
        PrintAdapterInventoryRecord(record);
    }

    return adapterRecords.empty() ? 2 : 0;
}

void PrintStartupResult(ProbeMode mode, const StartupFrameResult& result)
{
    std::cout << "Startup frame: " << (result.success ? "visible/synced" : "failed") << '\n';
    if (!result.success)
    {
        std::cout << "  failure: " << result.failureMessage << '\n';
    }
    std::cout << "  window_ready_ms=" << result.windowReadyMilliseconds
              << " startup_frame_visible_ms=" << result.startupFrameVisibleMilliseconds << '\n';
    std::cout << "PROBE_STARTUP_FRAME"
              << " mode=" << ToString(mode)
              << " success=" << (result.success ? "true" : "false")
              << " window_ready_ms=" << result.windowReadyMilliseconds
              << " visible_ms=" << result.startupFrameVisibleMilliseconds << '\n';
}

void PrintWinner(ProbeMode mode, const std::vector<CandidateResult>& results)
{
    const auto winner = std::min_element(results.begin(), results.end(), [](const CandidateResult& left, const CandidateResult& right) {
        if (left.success != right.success)
        {
            return left.success;
        }
        return left.timings.firstQueueSubmittedMilliseconds < right.timings.firstQueueSubmittedMilliseconds;
    });

    if (winner == results.end() || !winner->success)
    {
        std::cout << "PROBE_WINNER mode=" << ToString(mode) << " candidate=none ready_ms=0 margin_ms=0\n";
        return;
    }

    double marginMilliseconds = 0.0;
    for (const CandidateResult& result : results)
    {
        if (&result != &(*winner) && result.success)
        {
            marginMilliseconds = result.timings.firstQueueSubmittedMilliseconds -
                                 winner->timings.firstQueueSubmittedMilliseconds;
        }
    }

    std::cout << "PROBE_WINNER"
              << " mode=" << ToString(mode)
              << " candidate=" << ToString(winner->candidateKind)
              << " ready_ms=" << winner->timings.firstQueueSubmittedMilliseconds
              << " margin_ms=" << marginMilliseconds << '\n';
}

[[nodiscard]] bool SameAdapterIdentity(const CandidateResult& left, const CandidateResult& right)
{
    return left.adapterIdentity.backendType == right.adapterIdentity.backendType &&
           left.adapterIdentity.adapterType == right.adapterIdentity.adapterType &&
           left.adapterIdentity.vendorId == right.adapterIdentity.vendorId &&
           left.adapterIdentity.deviceId == right.adapterIdentity.deviceId &&
           left.adapterIdentity.device == right.adapterIdentity.device &&
           left.adapterIdentity.description == right.adapterIdentity.description;
}

[[nodiscard]] bool RaceUsedDistinctSuccessfulAdapters(const std::vector<CandidateResult>& results)
{
    if (results.size() != 2U || !results[0].success || !results[1].success)
    {
        return false;
    }

    return !SameAdapterIdentity(results[0], results[1]);
}

void PrintRaceAdapterCheck(ProbeMode mode, const std::vector<CandidateResult>& results)
{
    if (mode != ProbeMode::Race)
    {
        return;
    }

    std::cout << "PROBE_RACE_ADAPTERS"
              << " mode=" << ToString(mode)
              << " distinct_successful_adapters=" << (RaceUsedDistinctSuccessfulAdapters(results) ? "true" : "false")
              << '\n';
}

} // namespace

int main(int argc, char** argv)
{
    try
    {
        const ProbeOptions options = ParseOptions(argc, argv);
        if (options.mode == ProbeMode::Inventory)
        {
            return RunInventoryMode();
        }

        if (IsPresentationMode(options.mode))
        {
            const Clock::time_point epoch = Clock::now();
            const CandidateKind candidateKind = GetPresentationCandidateKind(options.mode);
            const PresentationResult result = RunPresentationCandidate(candidateKind, options, epoch);
            PrintPresentationResult(options.mode, result);
            return result.success ? 0 : 2;
        }

        const std::vector<CandidateKind> candidateKinds = GetCandidatesForMode(options.mode);
        const Clock::time_point epoch = Clock::now();

        std::latch startLatch(static_cast<std::ptrdiff_t>(candidateKinds.size() + 1U));
        std::vector<std::promise<CandidateResult>> promises(candidateKinds.size());
        std::vector<std::future<CandidateResult>> futures;
        futures.reserve(candidateKinds.size());
        for (std::promise<CandidateResult>& promise : promises)
        {
            futures.push_back(promise.get_future());
        }

        std::vector<std::jthread> candidateThreads;
        candidateThreads.reserve(candidateKinds.size());
        for (std::size_t index = 0; index < candidateKinds.size(); ++index)
        {
            candidateThreads.emplace_back([&, index] {
                promises[index].set_value(RunCandidate(candidateKinds[index], options, epoch, startLatch));
            });
        }

        std::vector<std::future<CandidateResult>*> futurePointers;
        futurePointers.reserve(futures.size());
        for (std::future<CandidateResult>& future : futures)
        {
            futurePointers.push_back(&future);
        }

        StartupFrameResult startupFrameResult = RunStartupLane(options, epoch, startLatch, futurePointers);

        std::vector<CandidateResult> results;
        results.reserve(futures.size());
        for (std::future<CandidateResult>& future : futures)
        {
            results.push_back(future.get());
        }

        std::cout << std::fixed << std::setprecision(3);
        std::cout << "Greenfield renderer startup probe\n";
        std::cout << "mode=" << ToString(options.mode) << " candidates=" << results.size()
                  << " timeout_ms=" << options.timeoutMilliseconds << '\n';
        PrintStartupResult(options.mode, startupFrameResult);
        for (const CandidateResult& result : results)
        {
            PrintHumanResult(result);
            PrintMachineResult(options.mode, result);
        }
        PrintWinner(options.mode, results);
        PrintRaceAdapterCheck(options.mode, results);

        const bool anyCandidateSucceeded =
            std::any_of(results.begin(), results.end(), [](const CandidateResult& result) { return result.success; });
        const bool raceWasDistinct =
            options.mode != ProbeMode::Race || RaceUsedDistinctSuccessfulAdapters(results);
        return startupFrameResult.success && anyCandidateSucceeded && raceWasDistinct ? 0 : 2;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "greenfield_renderer_startup_probe: " << exception.what() << '\n';
        return 1;
    }
}

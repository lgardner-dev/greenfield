#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <future>
#include <iomanip>
#include <iostream>
#include <latch>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <dawn/webgpu_cpp.h>

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

struct CandidateResult
{
    CandidateKind candidateKind{CandidateKind::Hardware};
    bool success{false};
    std::string failureStage;
    std::string failureMessage;
    AdapterIdentity adapterIdentity;
    CandidateTimings timings;
    bool renderVerified{false};
    std::string renderVerificationMessage;
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
    }

    return "race";
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

[[nodiscard]] bool IsHardwareAdapter(const AdapterIdentity& identity)
{
    return identity.adapterType == "discrete" || identity.adapterType == "integrated";
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

[[nodiscard]] bool WaitForFuture(const wgpu::Instance& instance, const wgpu::Future& future, std::uint64_t timeoutNanoseconds)
{
    return instance.WaitAny(future, timeoutNanoseconds) == wgpu::WaitStatus::Success;
}

[[nodiscard]] wgpu::Instance CreateProbeInstance()
{
    static constexpr auto TimedWaitAnyFeature = wgpu::InstanceFeatureName::TimedWaitAny;
    const wgpu::InstanceDescriptor descriptor{
        .requiredFeatureCount = 1,
        .requiredFeatures = &TimedWaitAnyFeature,
    };

    return wgpu::CreateInstance(&descriptor);
}

[[nodiscard]] wgpu::RequestAdapterOptions MakeAdapterOptions(CandidateKind candidateKind)
{
    wgpu::RequestAdapterOptions options{};
    if (candidateKind == CandidateKind::Hardware)
    {
        options.powerPreference = wgpu::PowerPreference::HighPerformance;
        options.forceFallbackAdapter = false;
        return options;
    }

    options.forceFallbackAdapter = true;
    options.backendType = wgpu::BackendType::Vulkan;
    return options;
}

[[nodiscard]] std::string BuildAdapterRejectionMessage(CandidateKind candidateKind, const AdapterIdentity& identity)
{
    std::ostringstream stream;
    if (candidateKind == CandidateKind::Hardware)
    {
        stream << "Requested hardware, but Dawn returned adapter_type=" << identity.adapterType
               << " backend=" << identity.backendType << " device=\"" << identity.device << "\".";
        return stream.str();
    }

    stream << "Requested SwiftShader fallback, but identity did not prove SwiftShader: vendor=\"" << identity.vendor
           << "\" device=\"" << identity.device << "\" description=\"" << identity.description << "\" vendor_id=0x"
           << std::hex << identity.vendorId << " device_id=0x" << identity.deviceId << std::dec << ".";
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
        wgpu::Instance instance = CreateProbeInstance();
        if (instance == nullptr)
        {
            MarkFailure(result, "instance", "Failed to create WebGPU instance.");
            return result;
        }
        result.timings.instanceCreatedMilliseconds = MillisecondsSince(epoch, Clock::now());

        const auto adapterRequestStart = Clock::now();
        RequestAdapterResult adapterRequestResult{};
        const wgpu::RequestAdapterOptions adapterOptions = MakeAdapterOptions(candidateKind);
        auto adapterCallback =
            [&adapterRequestResult](wgpu::RequestAdapterStatus status, wgpu::Adapter adapter, wgpu::StringView message) {
                adapterRequestResult.status = status;
                adapterRequestResult.adapter = adapter;
                adapterRequestResult.message = ToString(message);
            };

        const wgpu::Future adapterFuture =
            instance.RequestAdapter(&adapterOptions, wgpu::CallbackMode::WaitAnyOnly, adapterCallback);
        if (!WaitForFuture(instance, adapterFuture, TimeoutNanoseconds(options)))
        {
            MarkFailure(result, "adapter", "Timed out while requesting adapter.");
            return result;
        }
        result.timings.adapterRequestedMilliseconds =
            std::chrono::duration<double, std::milli>(Clock::now() - adapterRequestStart).count();

        if (adapterRequestResult.status != wgpu::RequestAdapterStatus::Success ||
            adapterRequestResult.adapter == nullptr)
        {
            MarkFailure(result, "adapter", "Adapter request failed: " + adapterRequestResult.message);
            return result;
        }

        result.adapterIdentity = QueryAdapterIdentity(adapterRequestResult.adapter);
        result.timings.adapterInfoQueriedMilliseconds = MillisecondsSince(epoch, Clock::now());

        if (candidateKind == CandidateKind::Hardware && !IsHardwareAdapter(result.adapterIdentity))
        {
            MarkFailure(result, "adapter_identity", BuildAdapterRejectionMessage(candidateKind, result.adapterIdentity));
            return result;
        }

        if (candidateKind == CandidateKind::SwiftShader && !ContainsSwiftShaderIdentity(result.adapterIdentity))
        {
            MarkFailure(result, "adapter_identity", BuildAdapterRejectionMessage(candidateKind, result.adapterIdentity));
            return result;
        }

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
            adapterRequestResult.adapter.RequestDevice(&deviceDescriptor, wgpu::CallbackMode::WaitAnyOnly, deviceCallback);
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
                throw std::invalid_argument("Expected --mode=hardware, --mode=swiftshader, or --mode=race.");
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
    std::cout << "  adapter: backend=" << result.adapterIdentity.backendType
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
              << " backend=" << result.adapterIdentity.backendType
              << " adapter_type=" << result.adapterIdentity.adapterType
              << " vendor_id=" << result.adapterIdentity.vendorId
              << " device_id=" << result.adapterIdentity.deviceId
              << " feature_count=" << result.adapterIdentity.featureCount
              << " max_texture_2d=" << result.adapterIdentity.maxTextureDimension2D
              << " adapter_request_ms=" << result.timings.adapterRequestedMilliseconds
              << " device_request_ms=" << result.timings.deviceRequestedMilliseconds
              << " pipeline_ready_ms=" << result.timings.pipelineCreatedMilliseconds
              << " preliminary_ready_ms=" << result.timings.firstQueueSubmittedMilliseconds
              << " queue_complete_ms=" << result.timings.queueCompletedMilliseconds
              << " readback_complete_ms=" << result.timings.readbackCompletedMilliseconds
              << " render_verified=" << (result.renderVerified ? "true" : "false") << '\n';
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

} // namespace

int main(int argc, char** argv)
{
    try
    {
        const ProbeOptions options = ParseOptions(argc, argv);
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

        const bool anyCandidateSucceeded =
            std::any_of(results.begin(), results.end(), [](const CandidateResult& result) { return result.success; });
        return startupFrameResult.success && anyCandidateSucceeded ? 0 : 2;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "greenfield_renderer_startup_probe: " << exception.what() << '\n';
        return 1;
    }
}

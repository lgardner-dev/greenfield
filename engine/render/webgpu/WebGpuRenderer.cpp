#include "engine/render/webgpu/WebGpuRenderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "engine/core/Vec2.h"
#include "engine/render/webgpu/WebGpuContext.h"

namespace greenfield
{
namespace
{

bool IsUsableSurfaceTextureStatus(wgpu::SurfaceGetCurrentTextureStatus status)
{
    return status == wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal ||
           status == wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal;
}

constexpr int FirstPrintableAscii = 32;
constexpr int LastPrintableAscii = 126;
constexpr int AtlasTextureSize = 1024;
constexpr int GlyphPadding = 1;

class FreeTypeLibrary
{
public:
    FreeTypeLibrary()
    {
        if (FT_Init_FreeType(&_library) != 0)
        {
            throw std::runtime_error("Failed to initialize FreeType.");
        }
    }

    ~FreeTypeLibrary()
    {
        if (_library != nullptr)
        {
            FT_Done_FreeType(_library);
        }
    }

    FreeTypeLibrary(const FreeTypeLibrary&) = delete;
    FreeTypeLibrary& operator=(const FreeTypeLibrary&) = delete;

    [[nodiscard]] FT_Library Get() const noexcept
    {
        return _library;
    }

private:
    FT_Library _library{nullptr};
};

class FreeTypeFace
{
public:
    FreeTypeFace(FT_Library library, const std::string& fontPath)
    {
        if (FT_New_Face(library, fontPath.c_str(), 0, &_face) != 0)
        {
            throw std::runtime_error("Failed to load font face: " + fontPath);
        }
    }

    ~FreeTypeFace()
    {
        if (_face != nullptr)
        {
            FT_Done_Face(_face);
        }
    }

    FreeTypeFace(const FreeTypeFace&) = delete;
    FreeTypeFace& operator=(const FreeTypeFace&) = delete;

    [[nodiscard]] FT_Face Get() const noexcept
    {
        return _face;
    }

private:
    FT_Face _face{nullptr};
};

wgpu::Color GetClearColor()
{
    return wgpu::Color{
        .r = 0.08,
        .g = 0.09,
        .b = 0.10,
        .a = 1.0,
    };
}

const char* GetRectangleShaderSource()
{
    return R"(
struct VertexInput {
    @location(0) position: vec2f,
    @location(1) pixelPosition: vec2f,
    @location(2) rectanglePosition: vec2f,
    @location(3) rectangleSize: vec2f,
    @location(4) fillColor: vec4f,
    @location(5) borderColor: vec4f,
    @location(6) cornerRadius: f32,
    @location(7) borderThickness: f32,
};

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) pixelPosition: vec2f,
    @location(1) rectanglePosition: vec2f,
    @location(2) rectangleSize: vec2f,
    @location(3) fillColor: vec4f,
    @location(4) borderColor: vec4f,
    @location(5) cornerRadius: f32,
    @location(6) borderThickness: f32,
};

@vertex
fn VertexMain(input: VertexInput) -> VertexOutput {
    var output: VertexOutput;
    output.position = vec4f(input.position, 0.0, 1.0);
    output.pixelPosition = input.pixelPosition;
    output.rectanglePosition = input.rectanglePosition;
    output.rectangleSize = input.rectangleSize;
    output.fillColor = input.fillColor;
    output.borderColor = input.borderColor;
    output.cornerRadius = input.cornerRadius;
    output.borderThickness = input.borderThickness;
    return output;
}

fn SignedRoundedRectangleDistance(pixelPosition: vec2f, rectanglePosition: vec2f, rectangleSize: vec2f,
                                  cornerRadius: f32) -> f32 {
    let halfSize = rectangleSize * 0.5;
    let radius = clamp(cornerRadius, 0.0, min(halfSize.x, halfSize.y));
    let centeredPosition = pixelPosition - rectanglePosition - halfSize;
    let cornerDistance = abs(centeredPosition) - halfSize + vec2f(radius);
    return length(max(cornerDistance, vec2f(0.0))) + min(max(cornerDistance.x, cornerDistance.y), 0.0) - radius;
}

@fragment
fn FragmentMain(input: VertexOutput) -> @location(0) vec4f {
    let signedDistance = SignedRoundedRectangleDistance(input.pixelPosition, input.rectanglePosition,
                                                        input.rectangleSize, input.cornerRadius);
    let edgeAlpha = 1.0 - smoothstep(0.0, 1.0, signedDistance);
    if (edgeAlpha <= 0.0) {
        discard;
    }

    let hasBorder = input.borderThickness > 0.0 && input.borderColor.a > 0.0;
    var rectangleColor = input.fillColor;
    if (hasBorder) {
        let fillAmount = 1.0 - smoothstep(-input.borderThickness - 1.0, -input.borderThickness, signedDistance);
        rectangleColor = mix(input.borderColor, input.fillColor, fillAmount);
    }

    return vec4f(rectangleColor.rgb, rectangleColor.a * edgeAlpha);
}
)";
}

const char* GetTextShaderSource()
{
    return R"(
@group(0) @binding(0) var glyphSampler: sampler;
@group(0) @binding(1) var glyphAtlas: texture_2d<f32>;

struct VertexInput {
    @location(0) position: vec2f,
    @location(1) textureCoordinate: vec2f,
    @location(2) color: vec4f,
};

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) textureCoordinate: vec2f,
    @location(1) color: vec4f,
};

@vertex
fn VertexMain(input: VertexInput) -> VertexOutput {
    var output: VertexOutput;
    output.position = vec4f(input.position, 0.0, 1.0);
    output.textureCoordinate = input.textureCoordinate;
    output.color = input.color;
    return output;
}

@fragment
fn FragmentMain(input: VertexOutput) -> @location(0) vec4f {
    let glyphAlpha = textureSample(glyphAtlas, glyphSampler, input.textureCoordinate).r;
    return vec4f(input.color.rgb, input.color.a * glyphAlpha);
}
)";
}

const char* GetVisualizationShaderSource()
{
    return R"(
@group(0) @binding(0) var<storage, read> polylinePoints: array<vec2f>;

struct VertexInput {
    @location(0) position: vec2f,
    @location(1) color: vec4f,
    @location(2) firstPoint: vec2f,
    @location(3) secondPoint: vec2f,
    @location(4) thicknessOrRadius: f32,
    @location(5) primitiveKind: u32,
    @location(6) pointOffset: u32,
    @location(7) pointCount: u32,
};

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) color: vec4f,
    @location(1) firstPoint: vec2f,
    @location(2) secondPoint: vec2f,
    @location(3) thicknessOrRadius: f32,
    @location(4) @interpolate(flat) primitiveKind: u32,
    @location(5) @interpolate(flat) pointOffset: u32,
    @location(6) @interpolate(flat) pointCount: u32,
};

@vertex
fn VertexMain(input: VertexInput) -> VertexOutput {
    var output: VertexOutput;
    output.position = vec4f(input.position, 0.0, 1.0);
    output.color = input.color;
    output.firstPoint = input.firstPoint;
    output.secondPoint = input.secondPoint;
    output.thicknessOrRadius = input.thicknessOrRadius;
    output.primitiveKind = input.primitiveKind;
    output.pointOffset = input.pointOffset;
    output.pointCount = input.pointCount;
    return output;
}

fn SquaredDistance(firstPoint: vec2f, secondPoint: vec2f) -> f32 {
    let distance = firstPoint - secondPoint;
    return dot(distance, distance);
}

fn SquaredDistanceToSegment(point: vec2f, segmentStart: vec2f, segmentEnd: vec2f) -> f32 {
    let segment = segmentEnd - segmentStart;
    let segmentLengthSquared = dot(segment, segment);
    if (segmentLengthSquared <= 0.0) {
        return SquaredDistance(point, segmentStart);
    }

    let projection = clamp(dot(point - segmentStart, segment) / segmentLengthSquared, 0.0, 1.0);
    return SquaredDistance(point, segmentStart + segment * projection);
}

fn IsCoveredByPolyline(pixelCenter: vec2f, pointOffset: u32, pointCount: u32, strokeThickness: f32) -> bool {
    if (pointCount < 2u) {
        return false;
    }

    let halfThickness = strokeThickness * 0.5;
    let coverageDistanceSquared = halfThickness * halfThickness;
    var segmentIndex = 1u;
    loop {
        if (segmentIndex >= pointCount) {
            break;
        }

        let segmentStart = polylinePoints[pointOffset + segmentIndex - 1u];
        let segmentEnd = polylinePoints[pointOffset + segmentIndex];
        if (SquaredDistanceToSegment(pixelCenter, segmentStart, segmentEnd) <= coverageDistanceSquared) {
            return true;
        }

        segmentIndex = segmentIndex + 1u;
    }

    return false;
}

@fragment
fn FragmentMain(input: VertexOutput, @builtin(position) fragmentPosition: vec4f) -> @location(0) vec4f {
    // WebGPU fragment positions are framebuffer pixel-center coordinates.
    let pixelCenter = fragmentPosition.xy;
    var isCovered = false;

    if (input.primitiveKind == 0u) {
        let halfThickness = input.thicknessOrRadius * 0.5;
        isCovered = SquaredDistanceToSegment(pixelCenter, input.firstPoint, input.secondPoint) <=
            halfThickness * halfThickness;
    } else if (input.primitiveKind == 1u) {
        isCovered = IsCoveredByPolyline(pixelCenter, input.pointOffset, input.pointCount, input.thicknessOrRadius);
    } else if (input.primitiveKind == 2u) {
        isCovered = SquaredDistance(pixelCenter, input.firstPoint) <=
            input.thicknessOrRadius * input.thicknessOrRadius;
    }

    if (!isCovered || input.color.a <= 0.0) {
        discard;
    }

    return input.color;
}
)";
}

float ConvertPixelXToClipSpace(float pixelX, std::uint32_t surfaceWidth)
{
    return (pixelX / static_cast<float>(surfaceWidth)) * 2.0f - 1.0f;
}

float ConvertPixelYToClipSpace(float pixelY, std::uint32_t surfaceHeight)
{
    return 1.0f - (pixelY / static_cast<float>(surfaceHeight)) * 2.0f;
}

bool HasPositiveArea(const Rect& rectangle)
{
    return rectangle.size.x > 0.0f && rectangle.size.y > 0.0f;
}

int GetAtlasPixelSize(float fontSize)
{
    return std::max(1, static_cast<int>(std::lround(fontSize)));
}

} // namespace

WebGpuRenderer::WebGpuRenderer(WebGpuContext& context, std::string defaultFontPath)
    : _context(&context)
    , _defaultFontPath(std::move(defaultFontPath))
{
}

void WebGpuRenderer::BeginFrame()
{
    _submissionQueue.Clear();
}

void WebGpuRenderer::Submit(const RenderCommandList& renderCommands)
{
    _submissionQueue.QueueRenderCommands(renderCommands);
}

void WebGpuRenderer::SubmitVisualization(const VisualizationCommandList& visualizationCommands)
{
    _submissionQueue.QueueVisualizationCommands(visualizationCommands);
}

void WebGpuRenderer::EndFrame()
{
    if (_context == nullptr || !_context->IsInitialized())
    {
        return;
    }

    _context->ReconfigureIfNeeded();

    wgpu::SurfaceTexture surfaceTexture{};
    _context->GetSurface().GetCurrentTexture(&surfaceTexture);
    if (!IsUsableSurfaceTextureStatus(surfaceTexture.status))
    {
        _context->ReconfigureIfNeeded();
        return;
    }

    if (surfaceTexture.texture == nullptr)
    {
        throw std::runtime_error("WebGPU surface returned a null frame texture.");
    }

    wgpu::TextureView textureView = surfaceTexture.texture.CreateView();
    wgpu::RenderPassColorAttachment colorAttachment{};
    colorAttachment.view = textureView;
    colorAttachment.loadOp = wgpu::LoadOp::Clear;
    colorAttachment.storeOp = wgpu::StoreOp::Store;
    colorAttachment.clearValue = GetClearColor();

    const wgpu::RenderPassDescriptor renderPassDescriptor{
        .colorAttachmentCount = 1,
        .colorAttachments = &colorAttachment,
    };

    wgpu::CommandEncoder commandEncoder = _context->GetDevice().CreateCommandEncoder();
    wgpu::RenderPassEncoder renderPass = commandEncoder.BeginRenderPass(&renderPassDescriptor);
    DrawRenderCommands(renderPass);
    renderPass.End();

    wgpu::CommandBuffer commandBuffer = commandEncoder.Finish();
    _context->GetQueue().Submit(1, &commandBuffer);
    _context->GetSurface().Present();

    _completedFrameCommandCount = _submissionQueue.SubmittedRenderCommandCount();
    _completedFrameVisualizationCommandCount = _submissionQueue.SubmittedVisualizationCommandCount();
}

void WebGpuRenderer::SetDefaultFontPath(std::string fontPath)
{
    if (_defaultFontPath == fontPath)
    {
        return;
    }

    _defaultFontPath = std::move(fontPath);
    _fontAtlases.clear();
}

std::size_t WebGpuRenderer::SubmittedCommandCount() const noexcept
{
    return _submissionQueue.SubmittedRenderCommandCount();
}

std::size_t WebGpuRenderer::CompletedFrameCommandCount() const noexcept
{
    return _completedFrameCommandCount;
}

std::size_t WebGpuRenderer::SubmittedVisualizationCommandCount() const noexcept
{
    return _submissionQueue.SubmittedVisualizationCommandCount();
}

std::size_t WebGpuRenderer::CompletedFrameVisualizationCommandCount() const noexcept
{
    return _completedFrameVisualizationCommandCount;
}

void WebGpuRenderer::EnsureRectanglePipeline()
{
    const wgpu::TextureFormat surfaceFormat = _context->GetSurfaceFormat();
    if (_rectanglePipeline != nullptr && _rectanglePipelineFormat == surfaceFormat)
    {
        return;
    }

    wgpu::ShaderSourceWGSL shaderSource{};
    shaderSource.code = GetRectangleShaderSource();

    const wgpu::ShaderModuleDescriptor shaderModuleDescriptor{
        .nextInChain = &shaderSource,
        .label = "Rectangle shader",
    };
    wgpu::ShaderModule shaderModule = _context->GetDevice().CreateShaderModule(&shaderModuleDescriptor);

    const std::array<wgpu::VertexAttribute, 8> vertexAttributes{{
        wgpu::VertexAttribute{
            .format = wgpu::VertexFormat::Float32x2,
            .offset = offsetof(RectangleVertex, position),
            .shaderLocation = 0,
        },
        wgpu::VertexAttribute{
            .format = wgpu::VertexFormat::Float32x2,
            .offset = offsetof(RectangleVertex, pixelPosition),
            .shaderLocation = 1,
        },
        wgpu::VertexAttribute{
            .format = wgpu::VertexFormat::Float32x2,
            .offset = offsetof(RectangleVertex, rectanglePosition),
            .shaderLocation = 2,
        },
        wgpu::VertexAttribute{
            .format = wgpu::VertexFormat::Float32x2,
            .offset = offsetof(RectangleVertex, rectangleSize),
            .shaderLocation = 3,
        },
        wgpu::VertexAttribute{
            .format = wgpu::VertexFormat::Float32x4,
            .offset = offsetof(RectangleVertex, fillColor),
            .shaderLocation = 4,
        },
        wgpu::VertexAttribute{
            .format = wgpu::VertexFormat::Float32x4,
            .offset = offsetof(RectangleVertex, borderColor),
            .shaderLocation = 5,
        },
        wgpu::VertexAttribute{
            .format = wgpu::VertexFormat::Float32,
            .offset = offsetof(RectangleVertex, cornerRadius),
            .shaderLocation = 6,
        },
        wgpu::VertexAttribute{
            .format = wgpu::VertexFormat::Float32,
            .offset = offsetof(RectangleVertex, borderThickness),
            .shaderLocation = 7,
        },
    }};

    const wgpu::VertexBufferLayout vertexBufferLayout{
        .stepMode = wgpu::VertexStepMode::Vertex,
        .arrayStride = sizeof(RectangleVertex),
        .attributeCount = vertexAttributes.size(),
        .attributes = vertexAttributes.data(),
    };

    const wgpu::BlendState blendState{
        .color =
            wgpu::BlendComponent{
                .operation = wgpu::BlendOperation::Add,
                .srcFactor = wgpu::BlendFactor::SrcAlpha,
                .dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha,
            },
        .alpha =
            wgpu::BlendComponent{
                .operation = wgpu::BlendOperation::Add,
                .srcFactor = wgpu::BlendFactor::One,
                .dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha,
            },
    };

    const wgpu::ColorTargetState colorTargetState{
        .format = surfaceFormat,
        .blend = &blendState,
        .writeMask = wgpu::ColorWriteMask::All,
    };

    const wgpu::FragmentState fragmentState{
        .module = shaderModule,
        .entryPoint = "FragmentMain",
        .targetCount = 1,
        .targets = &colorTargetState,
    };

    wgpu::RenderPipelineDescriptor pipelineDescriptor{};
    pipelineDescriptor.label = "Rectangle render pipeline";
    pipelineDescriptor.vertex.module = shaderModule;
    pipelineDescriptor.vertex.entryPoint = "VertexMain";
    pipelineDescriptor.vertex.bufferCount = 1;
    pipelineDescriptor.vertex.buffers = &vertexBufferLayout;
    pipelineDescriptor.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    pipelineDescriptor.fragment = &fragmentState;

    _rectanglePipeline = _context->GetDevice().CreateRenderPipeline(&pipelineDescriptor);
    _rectanglePipelineFormat = surfaceFormat;
}

void WebGpuRenderer::EnsureTextPipeline()
{
    const wgpu::TextureFormat surfaceFormat = _context->GetSurfaceFormat();
    if (_textPipeline != nullptr && _textPipelineFormat == surfaceFormat)
    {
        return;
    }

    const std::array<wgpu::BindGroupLayoutEntry, 2> bindGroupLayoutEntries{{
        wgpu::BindGroupLayoutEntry{
            .binding = 0,
            .visibility = wgpu::ShaderStage::Fragment,
            .sampler = wgpu::SamplerBindingLayout{.type = wgpu::SamplerBindingType::Filtering},
        },
        wgpu::BindGroupLayoutEntry{
            .binding = 1,
            .visibility = wgpu::ShaderStage::Fragment,
            .texture =
                wgpu::TextureBindingLayout{
                    .sampleType = wgpu::TextureSampleType::Float,
                    .viewDimension = wgpu::TextureViewDimension::e2D,
                },
        },
    }};

    const wgpu::BindGroupLayoutDescriptor bindGroupLayoutDescriptor{
        .label = "Text bind group layout",
        .entryCount = bindGroupLayoutEntries.size(),
        .entries = bindGroupLayoutEntries.data(),
    };
    _textBindGroupLayout = _context->GetDevice().CreateBindGroupLayout(&bindGroupLayoutDescriptor);

    const wgpu::PipelineLayoutDescriptor pipelineLayoutDescriptor{
        .label = "Text pipeline layout",
        .bindGroupLayoutCount = 1,
        .bindGroupLayouts = &_textBindGroupLayout,
    };
    wgpu::PipelineLayout pipelineLayout = _context->GetDevice().CreatePipelineLayout(&pipelineLayoutDescriptor);

    wgpu::ShaderSourceWGSL shaderSource{};
    shaderSource.code = GetTextShaderSource();

    const wgpu::ShaderModuleDescriptor shaderModuleDescriptor{
        .nextInChain = &shaderSource,
        .label = "Text shader",
    };
    wgpu::ShaderModule shaderModule = _context->GetDevice().CreateShaderModule(&shaderModuleDescriptor);

    const std::array<wgpu::VertexAttribute, 3> vertexAttributes{{
        wgpu::VertexAttribute{
            .format = wgpu::VertexFormat::Float32x2,
            .offset = offsetof(TextVertex, position),
            .shaderLocation = 0,
        },
        wgpu::VertexAttribute{
            .format = wgpu::VertexFormat::Float32x2,
            .offset = offsetof(TextVertex, textureCoordinate),
            .shaderLocation = 1,
        },
        wgpu::VertexAttribute{
            .format = wgpu::VertexFormat::Float32x4,
            .offset = offsetof(TextVertex, color),
            .shaderLocation = 2,
        },
    }};

    const wgpu::VertexBufferLayout vertexBufferLayout{
        .stepMode = wgpu::VertexStepMode::Vertex,
        .arrayStride = sizeof(TextVertex),
        .attributeCount = vertexAttributes.size(),
        .attributes = vertexAttributes.data(),
    };

    const wgpu::BlendState blendState{
        .color =
            wgpu::BlendComponent{
                .operation = wgpu::BlendOperation::Add,
                .srcFactor = wgpu::BlendFactor::SrcAlpha,
                .dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha,
            },
        .alpha =
            wgpu::BlendComponent{
                .operation = wgpu::BlendOperation::Add,
                .srcFactor = wgpu::BlendFactor::One,
                .dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha,
            },
    };

    const wgpu::ColorTargetState colorTargetState{
        .format = surfaceFormat,
        .blend = &blendState,
        .writeMask = wgpu::ColorWriteMask::All,
    };

    const wgpu::FragmentState fragmentState{
        .module = shaderModule,
        .entryPoint = "FragmentMain",
        .targetCount = 1,
        .targets = &colorTargetState,
    };

    wgpu::RenderPipelineDescriptor pipelineDescriptor{};
    pipelineDescriptor.label = "Text render pipeline";
    pipelineDescriptor.layout = pipelineLayout;
    pipelineDescriptor.vertex.module = shaderModule;
    pipelineDescriptor.vertex.entryPoint = "VertexMain";
    pipelineDescriptor.vertex.bufferCount = 1;
    pipelineDescriptor.vertex.buffers = &vertexBufferLayout;
    pipelineDescriptor.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    pipelineDescriptor.fragment = &fragmentState;

    _textPipeline = _context->GetDevice().CreateRenderPipeline(&pipelineDescriptor);
    _textPipelineFormat = surfaceFormat;
}

void WebGpuRenderer::EnsureVisualizationPipeline()
{
    const wgpu::TextureFormat surfaceFormat = _context->GetSurfaceFormat();
    if (_visualizationPipeline != nullptr && _visualizationPipelineFormat == surfaceFormat)
    {
        return;
    }

    const wgpu::BindGroupLayoutEntry bindGroupLayoutEntry{
        .binding = 0,
        .visibility = wgpu::ShaderStage::Fragment,
        .buffer = wgpu::BufferBindingLayout{.type = wgpu::BufferBindingType::ReadOnlyStorage},
    };

    const wgpu::BindGroupLayoutDescriptor bindGroupLayoutDescriptor{
        .label = "Visualization bind group layout",
        .entryCount = 1,
        .entries = &bindGroupLayoutEntry,
    };
    _visualizationBindGroupLayout = _context->GetDevice().CreateBindGroupLayout(&bindGroupLayoutDescriptor);
    _visualizationBindGroup = nullptr;

    const wgpu::PipelineLayoutDescriptor pipelineLayoutDescriptor{
        .label = "Visualization pipeline layout",
        .bindGroupLayoutCount = 1,
        .bindGroupLayouts = &_visualizationBindGroupLayout,
    };
    wgpu::PipelineLayout pipelineLayout = _context->GetDevice().CreatePipelineLayout(&pipelineLayoutDescriptor);

    wgpu::ShaderSourceWGSL shaderSource{};
    shaderSource.code = GetVisualizationShaderSource();

    const wgpu::ShaderModuleDescriptor shaderModuleDescriptor{
        .nextInChain = &shaderSource,
        .label = "Visualization shader",
    };
    wgpu::ShaderModule shaderModule = _context->GetDevice().CreateShaderModule(&shaderModuleDescriptor);

    const std::array<wgpu::VertexAttribute, 8> vertexAttributes{{
        wgpu::VertexAttribute{
            .format = wgpu::VertexFormat::Float32x2,
            .offset = offsetof(VisualizationVertex, position),
            .shaderLocation = 0,
        },
        wgpu::VertexAttribute{
            .format = wgpu::VertexFormat::Float32x4,
            .offset = offsetof(VisualizationVertex, color),
            .shaderLocation = 1,
        },
        wgpu::VertexAttribute{
            .format = wgpu::VertexFormat::Float32x2,
            .offset = offsetof(VisualizationVertex, firstPoint),
            .shaderLocation = 2,
        },
        wgpu::VertexAttribute{
            .format = wgpu::VertexFormat::Float32x2,
            .offset = offsetof(VisualizationVertex, secondPoint),
            .shaderLocation = 3,
        },
        wgpu::VertexAttribute{
            .format = wgpu::VertexFormat::Float32,
            .offset = offsetof(VisualizationVertex, thicknessOrRadius),
            .shaderLocation = 4,
        },
        wgpu::VertexAttribute{
            .format = wgpu::VertexFormat::Uint32,
            .offset = offsetof(VisualizationVertex, primitiveKind),
            .shaderLocation = 5,
        },
        wgpu::VertexAttribute{
            .format = wgpu::VertexFormat::Uint32,
            .offset = offsetof(VisualizationVertex, pointOffset),
            .shaderLocation = 6,
        },
        wgpu::VertexAttribute{
            .format = wgpu::VertexFormat::Uint32,
            .offset = offsetof(VisualizationVertex, pointCount),
            .shaderLocation = 7,
        },
    }};

    const wgpu::VertexBufferLayout vertexBufferLayout{
        .stepMode = wgpu::VertexStepMode::Vertex,
        .arrayStride = sizeof(VisualizationVertex),
        .attributeCount = vertexAttributes.size(),
        .attributes = vertexAttributes.data(),
    };

    const wgpu::BlendState blendState{
        .color =
            wgpu::BlendComponent{
                .operation = wgpu::BlendOperation::Add,
                .srcFactor = wgpu::BlendFactor::SrcAlpha,
                .dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha,
            },
        .alpha =
            wgpu::BlendComponent{
                .operation = wgpu::BlendOperation::Add,
                .srcFactor = wgpu::BlendFactor::One,
                .dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha,
            },
    };

    const wgpu::ColorTargetState colorTargetState{
        .format = surfaceFormat,
        .blend = &blendState,
        .writeMask = wgpu::ColorWriteMask::All,
    };

    const wgpu::FragmentState fragmentState{
        .module = shaderModule,
        .entryPoint = "FragmentMain",
        .targetCount = 1,
        .targets = &colorTargetState,
    };

    wgpu::RenderPipelineDescriptor pipelineDescriptor{};
    pipelineDescriptor.label = "Visualization render pipeline";
    pipelineDescriptor.layout = pipelineLayout;
    pipelineDescriptor.vertex.module = shaderModule;
    pipelineDescriptor.vertex.entryPoint = "VertexMain";
    pipelineDescriptor.vertex.bufferCount = 1;
    pipelineDescriptor.vertex.buffers = &vertexBufferLayout;
    pipelineDescriptor.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    pipelineDescriptor.fragment = &fragmentState;

    _visualizationPipeline = _context->GetDevice().CreateRenderPipeline(&pipelineDescriptor);
    _visualizationPipelineFormat = surfaceFormat;
}

WebGpuRenderer::FontAtlas* WebGpuRenderer::GetOrCreateFontAtlas(float fontSize)
{
    if (_defaultFontPath.empty())
    {
        return nullptr;
    }

    EnsureTextPipeline();

    const int atlasPixelSize = GetAtlasPixelSize(fontSize);
    auto existingAtlas = _fontAtlases.find(atlasPixelSize);
    if (existingAtlas != _fontAtlases.end())
    {
        return &existingAtlas->second;
    }

    FreeTypeLibrary freeTypeLibrary;
    FreeTypeFace freeTypeFace(freeTypeLibrary.Get(), _defaultFontPath);
    if (FT_Set_Pixel_Sizes(freeTypeFace.Get(), 0, static_cast<FT_UInt>(atlasPixelSize)) != 0)
    {
        throw std::runtime_error("Failed to set font pixel size.");
    }

    std::vector<std::uint8_t> atlasPixels(AtlasTextureSize * AtlasTextureSize, 0);
    FontAtlas fontAtlas{};
    fontAtlas.lineHeight = static_cast<float>(freeTypeFace.Get()->size->metrics.height >> 6);

    int cursorX = GlyphPadding;
    int cursorY = GlyphPadding;
    int rowHeight = 0;

    for (int characterCode = FirstPrintableAscii; characterCode <= LastPrintableAscii; ++characterCode)
    {
        if (FT_Load_Char(freeTypeFace.Get(), static_cast<FT_ULong>(characterCode), FT_LOAD_RENDER) != 0)
        {
            continue;
        }

        const FT_GlyphSlot glyph = freeTypeFace.Get()->glyph;
        const int glyphWidth = static_cast<int>(glyph->bitmap.width);
        const int glyphHeight = static_cast<int>(glyph->bitmap.rows);

        if (cursorX + glyphWidth + GlyphPadding >= AtlasTextureSize)
        {
            cursorX = GlyphPadding;
            cursorY += rowHeight + GlyphPadding;
            rowHeight = 0;
        }

        if (cursorY + glyphHeight + GlyphPadding >= AtlasTextureSize)
        {
            throw std::runtime_error("Font atlas is too small for printable ASCII glyphs.");
        }

        for (int sourceY = 0; sourceY < glyphHeight; ++sourceY)
        {
            const auto* sourceRow = glyph->bitmap.buffer + sourceY * glyph->bitmap.pitch;
            auto* destinationRow = atlasPixels.data() + (cursorY + sourceY) * AtlasTextureSize + cursorX;
            std::copy(sourceRow, sourceRow + glyphWidth, destinationRow);
        }

        fontAtlas.glyphs.emplace(
            static_cast<char>(characterCode),
            GlyphInfo{
                .textureLeft = static_cast<float>(cursorX) / static_cast<float>(AtlasTextureSize),
                .textureTop = static_cast<float>(cursorY) / static_cast<float>(AtlasTextureSize),
                .textureRight = static_cast<float>(cursorX + glyphWidth) / static_cast<float>(AtlasTextureSize),
                .textureBottom = static_cast<float>(cursorY + glyphHeight) / static_cast<float>(AtlasTextureSize),
                .width = static_cast<float>(glyphWidth),
                .height = static_cast<float>(glyphHeight),
                .bearingX = static_cast<float>(glyph->bitmap_left),
                .bearingY = static_cast<float>(glyph->bitmap_top),
                .advance = static_cast<float>(glyph->advance.x >> 6),
            });

        cursorX += glyphWidth + GlyphPadding;
        rowHeight = std::max(rowHeight, glyphHeight);
    }

    const wgpu::TextureDescriptor textureDescriptor{
        .label = "Text glyph atlas",
        .usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::TextureBinding,
        .dimension = wgpu::TextureDimension::e2D,
        .size = wgpu::Extent3D{
            .width = AtlasTextureSize,
            .height = AtlasTextureSize,
            .depthOrArrayLayers = 1,
        },
        .format = wgpu::TextureFormat::R8Unorm,
    };
    fontAtlas.texture = _context->GetDevice().CreateTexture(&textureDescriptor);
    fontAtlas.textureView = fontAtlas.texture.CreateView();

    const wgpu::TexelCopyTextureInfo destination{
        .texture = fontAtlas.texture,
        .aspect = wgpu::TextureAspect::All,
    };
    const wgpu::TexelCopyBufferLayout sourceLayout{
        .bytesPerRow = AtlasTextureSize,
        .rowsPerImage = AtlasTextureSize,
    };
    const wgpu::Extent3D writeSize{
        .width = AtlasTextureSize,
        .height = AtlasTextureSize,
        .depthOrArrayLayers = 1,
    };
    _context->GetQueue().WriteTexture(&destination, atlasPixels.data(), atlasPixels.size(), &sourceLayout, &writeSize);

    const wgpu::SamplerDescriptor samplerDescriptor{
        .label = "Text glyph sampler",
        .addressModeU = wgpu::AddressMode::ClampToEdge,
        .addressModeV = wgpu::AddressMode::ClampToEdge,
        .addressModeW = wgpu::AddressMode::ClampToEdge,
        .magFilter = wgpu::FilterMode::Linear,
        .minFilter = wgpu::FilterMode::Linear,
        .mipmapFilter = wgpu::MipmapFilterMode::Nearest,
    };
    fontAtlas.sampler = _context->GetDevice().CreateSampler(&samplerDescriptor);

    const std::array<wgpu::BindGroupEntry, 2> bindGroupEntries{{
        wgpu::BindGroupEntry{
            .binding = 0,
            .sampler = fontAtlas.sampler,
        },
        wgpu::BindGroupEntry{
            .binding = 1,
            .textureView = fontAtlas.textureView,
        },
    }};
    const wgpu::BindGroupDescriptor bindGroupDescriptor{
        .label = "Text bind group",
        .layout = _textBindGroupLayout,
        .entryCount = bindGroupEntries.size(),
        .entries = bindGroupEntries.data(),
    };
    fontAtlas.bindGroup = _context->GetDevice().CreateBindGroup(&bindGroupDescriptor);

    auto [createdAtlas, wasCreated] = _fontAtlases.emplace(atlasPixelSize, std::move(fontAtlas));
    return wasCreated ? &createdAtlas->second : nullptr;
}

WebGpuRenderer::RectangleVertex WebGpuRenderer::MakeRectangleVertex(float clipPositionX, float clipPositionY,
                                                                    float pixelPositionX, float pixelPositionY,
                                                                    const RenderCommand& renderCommand) const noexcept
{
    const Rect& rectangle = renderCommand.rectangle;
    return RectangleVertex{
        .position = {clipPositionX, clipPositionY},
        .pixelPosition = {pixelPositionX, pixelPositionY},
        .rectanglePosition = {rectangle.position.x, rectangle.position.y},
        .rectangleSize = {rectangle.size.x, rectangle.size.y},
        .fillColor =
            {
                renderCommand.fillColor.red,
                renderCommand.fillColor.green,
                renderCommand.fillColor.blue,
                renderCommand.fillColor.alpha,
            },
        .borderColor =
            {
                renderCommand.borderColor.red,
                renderCommand.borderColor.green,
                renderCommand.borderColor.blue,
                renderCommand.borderColor.alpha,
            },
        .cornerRadius = renderCommand.cornerRadius,
        .borderThickness = renderCommand.borderThickness,
    };
}

WebGpuRenderer::TextVertex WebGpuRenderer::MakeTextVertex(float pixelPositionX, float pixelPositionY,
                                                          float textureCoordinateX, float textureCoordinateY,
                                                          const Color& color) const noexcept
{
    return TextVertex{
        .position =
            {
                ConvertPixelXToClipSpace(pixelPositionX, _context->GetSurfaceWidth()),
                ConvertPixelYToClipSpace(pixelPositionY, _context->GetSurfaceHeight()),
            },
        .textureCoordinate = {textureCoordinateX, textureCoordinateY},
        .color = {color.red, color.green, color.blue, color.alpha},
    };
}

WebGpuRenderer::VisualizationVertex WebGpuRenderer::MakeVisualizationVertex(
    float pixelPositionX,
    float pixelPositionY,
    const WebGpuPreparedVisualizationPrimitive& primitive,
    std::uint32_t pointOffset) const noexcept
{
    return VisualizationVertex{
        .position =
            {
                ConvertPixelXToClipSpace(pixelPositionX, _context->GetSurfaceWidth()),
                ConvertPixelYToClipSpace(pixelPositionY, _context->GetSurfaceHeight()),
            },
        .color = {primitive.color.red, primitive.color.green, primitive.color.blue, primitive.color.alpha},
        .firstPoint = {primitive.firstPoint.x, primitive.firstPoint.y},
        .secondPoint = {primitive.secondPoint.x, primitive.secondPoint.y},
        .thicknessOrRadius = primitive.thicknessOrRadius,
        .primitiveKind = static_cast<std::uint32_t>(primitive.kind),
        .pointOffset = pointOffset,
        .pointCount = primitive.pointCount,
    };
}

void WebGpuRenderer::AppendTextVertices(const RenderCommand& renderCommand, const FontAtlas& fontAtlas)
{
    if (_context->GetSurfaceWidth() == 0 || _context->GetSurfaceHeight() == 0 || renderCommand.text.empty())
    {
        return;
    }

    float cursorX = renderCommand.rectangle.position.x;
    const float baselineY = renderCommand.rectangle.position.y + renderCommand.fontSize;

    for (char character : renderCommand.text)
    {
        const auto glyph = fontAtlas.glyphs.find(character);
        if (glyph == fontAtlas.glyphs.end())
        {
            continue;
        }

        const GlyphInfo& glyphInfo = glyph->second;
        const float left = cursorX + glyphInfo.bearingX;
        const float top = baselineY - glyphInfo.bearingY;
        const float right = left + glyphInfo.width;
        const float bottom = top + glyphInfo.height;

        if (glyphInfo.width > 0.0f && glyphInfo.height > 0.0f)
        {
            const TextVertex topLeft = MakeTextVertex(
                left, top, glyphInfo.textureLeft, glyphInfo.textureTop, renderCommand.textColor);
            const TextVertex topRight = MakeTextVertex(
                right, top, glyphInfo.textureRight, glyphInfo.textureTop, renderCommand.textColor);
            const TextVertex bottomLeft = MakeTextVertex(
                left, bottom, glyphInfo.textureLeft, glyphInfo.textureBottom, renderCommand.textColor);
            const TextVertex bottomRight = MakeTextVertex(
                right, bottom, glyphInfo.textureRight, glyphInfo.textureBottom, renderCommand.textColor);

            _textVertices.push_back(topLeft);
            _textVertices.push_back(bottomLeft);
            _textVertices.push_back(topRight);
            _textVertices.push_back(topRight);
            _textVertices.push_back(bottomLeft);
            _textVertices.push_back(bottomRight);
        }

        cursorX += glyphInfo.advance;
    }
}

void WebGpuRenderer::EnsureVertexBuffer(std::size_t requiredSize)
{
    if (requiredSize == 0 || _rectangleVertexBufferSize >= requiredSize)
    {
        return;
    }

    const wgpu::BufferDescriptor bufferDescriptor{
        .label = "Rectangle vertex buffer",
        .usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex,
        .size = requiredSize,
    };

    _rectangleVertexBuffer = _context->GetDevice().CreateBuffer(&bufferDescriptor);
    _rectangleVertexBufferSize = requiredSize;
}

void WebGpuRenderer::EnsureTextVertexBuffer(std::size_t requiredSize)
{
    if (requiredSize == 0 || _textVertexBufferSize >= requiredSize)
    {
        return;
    }

    const wgpu::BufferDescriptor bufferDescriptor{
        .label = "Text vertex buffer",
        .usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex,
        .size = requiredSize,
    };

    _textVertexBuffer = _context->GetDevice().CreateBuffer(&bufferDescriptor);
    _textVertexBufferSize = requiredSize;
}

void WebGpuRenderer::EnsureVisualizationVertexBuffer(std::size_t requiredSize)
{
    if (requiredSize == 0 || _visualizationVertexBufferSize >= requiredSize)
    {
        return;
    }

    const wgpu::BufferDescriptor bufferDescriptor{
        .label = "Visualization vertex buffer",
        .usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex,
        .size = requiredSize,
    };

    _visualizationVertexBuffer = _context->GetDevice().CreateBuffer(&bufferDescriptor);
    _visualizationVertexBufferSize = requiredSize;
}

void WebGpuRenderer::EnsureVisualizationPointBuffer(std::size_t requiredSize)
{
    const std::size_t storageSize = std::max(requiredSize, sizeof(Vec2));
    if (_visualizationPointBufferSize >= storageSize)
    {
        return;
    }

    const wgpu::BufferDescriptor bufferDescriptor{
        .label = "Visualization polyline point buffer",
        .usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Storage,
        .size = storageSize,
    };

    _visualizationPointBuffer = _context->GetDevice().CreateBuffer(&bufferDescriptor);
    _visualizationPointBufferSize = storageSize;
    _visualizationBindGroup = nullptr;
}

void WebGpuRenderer::EnsureVisualizationBindGroup()
{
    if (_visualizationBindGroup != nullptr)
    {
        return;
    }

    EnsureVisualizationPipeline();
    EnsureVisualizationPointBuffer(0);

    const wgpu::BindGroupEntry bindGroupEntry{
        .binding = 0,
        .buffer = _visualizationPointBuffer,
        .offset = 0,
        .size = _visualizationPointBufferSize,
    };

    const wgpu::BindGroupDescriptor bindGroupDescriptor{
        .label = "Visualization bind group",
        .layout = _visualizationBindGroupLayout,
        .entryCount = 1,
        .entries = &bindGroupEntry,
    };
    _visualizationBindGroup = _context->GetDevice().CreateBindGroup(&bindGroupDescriptor);
}

void WebGpuRenderer::DrawRenderCommands(wgpu::RenderPassEncoder& renderPass)
{
    BuildRenderBatches();
    WritePreparedVertexBuffers();

    ApplyFullFrameScissor(renderPass);
    for (const QueuedDrawOperation& queuedDrawOperation : _queuedDrawOperations)
    {
        DrawQueuedOperation(renderPass, queuedDrawOperation);
    }
}

void WebGpuRenderer::BuildRenderBatches()
{
    std::vector<Rect> clipRectangles;
    _rectangleVertices.clear();
    _textVertices.clear();
    _visualizationVertices.clear();
    _visualizationPoints.clear();
    _drawBatches.clear();
    _visualizationDrawBatches.clear();
    _queuedDrawOperations.clear();

    for (const WebGpuQueuedSubmission& queuedSubmission : _submissionQueue.Submissions())
    {
        if (const WebGpuQueuedRenderSubmission* renderSubmission =
                std::get_if<WebGpuQueuedRenderSubmission>(&queuedSubmission))
        {
            BuildRenderCommandBatches(std::span<const RenderCommand>(renderSubmission->commands), clipRectangles);
            continue;
        }

        if (const WebGpuQueuedVisualizationSubmission* visualizationSubmission =
                std::get_if<WebGpuQueuedVisualizationSubmission>(&queuedSubmission))
        {
            BuildVisualizationBatches(*visualizationSubmission);
        }
    }
}

void WebGpuRenderer::BuildRenderCommandBatches(std::span<const RenderCommand> renderCommands,
                                               std::vector<Rect>& clipRectangles)
{
    for (const RenderCommand& renderCommand : renderCommands)
    {
        if (renderCommand.type == RenderCommandType::PushClip)
        {
            const Rect clipRectangle = clipRectangles.empty()
                                           ? renderCommand.rectangle
                                           : IntersectRectangles(clipRectangles.back(), renderCommand.rectangle);
            clipRectangles.push_back(clipRectangle);
            continue;
        }

        if (renderCommand.type == RenderCommandType::PopClip)
        {
            if (!clipRectangles.empty())
            {
                clipRectangles.pop_back();
            }
            continue;
        }

        if (!clipRectangles.empty() && !HasPositiveArea(clipRectangles.back()))
        {
            continue;
        }

        if (renderCommand.type == RenderCommandType::FillRectangle)
        {
            const auto firstVertex = static_cast<std::uint32_t>(_rectangleVertices.size());
            AppendRectangleVertices(renderCommand);
            const auto vertexCount = static_cast<std::uint32_t>(_rectangleVertices.size()) - firstVertex;
            if (vertexCount == 0)
            {
                continue;
            }

            _drawBatches.push_back(DrawBatch{
                .type = RenderCommandType::FillRectangle,
                .hasClip = !clipRectangles.empty(),
                .clipRectangle = clipRectangles.empty() ? Rect{} : clipRectangles.back(),
                .bindGroup = {},
                .firstVertex = firstVertex,
                .vertexCount = vertexCount,
            });
            _queuedDrawOperations.push_back(QueuedDrawBatch{.index = _drawBatches.size() - 1U});
        }
        else if (renderCommand.type == RenderCommandType::DrawText)
        {
            FontAtlas* fontAtlas = GetOrCreateFontAtlas(renderCommand.fontSize);
            if (fontAtlas == nullptr)
            {
                continue;
            }

            const auto firstVertex = static_cast<std::uint32_t>(_textVertices.size());
            AppendTextVertices(renderCommand, *fontAtlas);
            const auto vertexCount = static_cast<std::uint32_t>(_textVertices.size()) - firstVertex;
            if (vertexCount == 0)
            {
                continue;
            }

            _drawBatches.push_back(DrawBatch{
                .type = RenderCommandType::DrawText,
                .hasClip = !clipRectangles.empty(),
                .clipRectangle = clipRectangles.empty() ? Rect{} : clipRectangles.back(),
                .bindGroup = fontAtlas->bindGroup,
                .firstVertex = firstVertex,
                .vertexCount = vertexCount,
            });
            _queuedDrawOperations.push_back(QueuedDrawBatch{.index = _drawBatches.size() - 1U});
        }
    }
}

void WebGpuRenderer::BuildVisualizationBatches(const WebGpuQueuedVisualizationSubmission& visualizationSubmission)
{
    const WebGpuPreparedVisualizationSubmission preparedSubmission =
        PrepareWebGpuVisualizationSubmission(visualizationSubmission,
                                             _context->GetSurfaceWidth(),
                                             _context->GetSurfaceHeight());

    if (preparedSubmission.scissorRectangle.width == 0 || preparedSubmission.scissorRectangle.height == 0)
    {
        return;
    }

    if (_visualizationPoints.size() > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()))
    {
        throw std::runtime_error("WebGPU visualization frame point storage exceeded 32-bit shader indexing.");
    }

    const std::uint32_t basePointOffset = static_cast<std::uint32_t>(_visualizationPoints.size());
    _visualizationPoints.insert(_visualizationPoints.end(),
                                preparedSubmission.polylinePoints.begin(),
                                preparedSubmission.polylinePoints.end());
    if (_visualizationPoints.size() > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()))
    {
        throw std::runtime_error("WebGPU visualization frame point storage exceeded 32-bit shader indexing.");
    }

    for (const WebGpuPreparedVisualizationPrimitive& primitive : preparedSubmission.primitives)
    {
        AppendVisualizationVertices(primitive,
                                    preparedSubmission.scissorRectangle,
                                    basePointOffset + primitive.pointOffset);
    }
}

void WebGpuRenderer::AppendRectangleVertices(const RenderCommand& renderCommand)
{
    if (_context->GetSurfaceWidth() == 0 || _context->GetSurfaceHeight() == 0 || !HasPositiveArea(renderCommand.rectangle))
    {
        return;
    }

    const Rect& rectangle = renderCommand.rectangle;
    const float left = ConvertPixelXToClipSpace(rectangle.position.x, _context->GetSurfaceWidth());
    const float right =
        ConvertPixelXToClipSpace(rectangle.position.x + rectangle.size.x, _context->GetSurfaceWidth());
    const float top = ConvertPixelYToClipSpace(rectangle.position.y, _context->GetSurfaceHeight());
    const float bottom =
        ConvertPixelYToClipSpace(rectangle.position.y + rectangle.size.y, _context->GetSurfaceHeight());

    const RectangleVertex topLeft =
        MakeRectangleVertex(left, top, rectangle.position.x, rectangle.position.y, renderCommand);
    const RectangleVertex topRight =
        MakeRectangleVertex(right, top, rectangle.position.x + rectangle.size.x, rectangle.position.y, renderCommand);
    const RectangleVertex bottomLeft =
        MakeRectangleVertex(left, bottom, rectangle.position.x, rectangle.position.y + rectangle.size.y, renderCommand);
    const RectangleVertex bottomRight = MakeRectangleVertex(
        right, bottom, rectangle.position.x + rectangle.size.x, rectangle.position.y + rectangle.size.y, renderCommand);

    _rectangleVertices.push_back(topLeft);
    _rectangleVertices.push_back(bottomLeft);
    _rectangleVertices.push_back(topRight);
    _rectangleVertices.push_back(topRight);
    _rectangleVertices.push_back(bottomLeft);
    _rectangleVertices.push_back(bottomRight);
}

void WebGpuRenderer::AppendVisualizationVertices(const WebGpuPreparedVisualizationPrimitive& primitive,
                                                 const WebGpuScissorRectangle& scissorRectangle,
                                                 std::uint32_t pointOffset)
{
    if (_context->GetSurfaceWidth() == 0 || _context->GetSurfaceHeight() == 0 ||
        !HasPositiveArea(primitive.coverageBounds))
    {
        return;
    }

    // Visualization commands are already screen-space; these triangles only cover
    // the expanded primitive bounds while the fragment shader applies pixel-center coverage.
    const Rect& bounds = primitive.coverageBounds;
    const float left = bounds.position.x;
    const float top = bounds.position.y;
    const float right = bounds.position.x + bounds.size.x;
    const float bottom = bounds.position.y + bounds.size.y;

    const auto firstVertex = static_cast<std::uint32_t>(_visualizationVertices.size());
    const VisualizationVertex topLeft = MakeVisualizationVertex(left, top, primitive, pointOffset);
    const VisualizationVertex topRight = MakeVisualizationVertex(right, top, primitive, pointOffset);
    const VisualizationVertex bottomLeft = MakeVisualizationVertex(left, bottom, primitive, pointOffset);
    const VisualizationVertex bottomRight = MakeVisualizationVertex(right, bottom, primitive, pointOffset);

    _visualizationVertices.push_back(topLeft);
    _visualizationVertices.push_back(bottomLeft);
    _visualizationVertices.push_back(topRight);
    _visualizationVertices.push_back(topRight);
    _visualizationVertices.push_back(bottomLeft);
    _visualizationVertices.push_back(bottomRight);

    _visualizationDrawBatches.push_back(VisualizationDrawBatch{
        .scissorRectangle = scissorRectangle,
        .firstVertex = firstVertex,
        .vertexCount = 6,
    });
    _queuedDrawOperations.push_back(QueuedVisualizationDrawBatch{.index = _visualizationDrawBatches.size() - 1U});
}

void WebGpuRenderer::WritePreparedVertexBuffers()
{
    if (!_rectangleVertices.empty())
    {
        EnsureRectanglePipeline();

        const std::size_t vertexDataSize = _rectangleVertices.size() * sizeof(RectangleVertex);
        EnsureVertexBuffer(vertexDataSize);

        _context->GetQueue().WriteBuffer(_rectangleVertexBuffer, 0, _rectangleVertices.data(), vertexDataSize);
    }

    if (!_textVertices.empty())
    {
        EnsureTextPipeline();

        const std::size_t vertexDataSize = _textVertices.size() * sizeof(TextVertex);
        EnsureTextVertexBuffer(vertexDataSize);

        _context->GetQueue().WriteBuffer(_textVertexBuffer, 0, _textVertices.data(), vertexDataSize);
    }

    if (!_visualizationVertices.empty())
    {
        EnsureVisualizationPipeline();

        const std::size_t vertexDataSize = _visualizationVertices.size() * sizeof(VisualizationVertex);
        EnsureVisualizationVertexBuffer(vertexDataSize);

        _context->GetQueue().WriteBuffer(_visualizationVertexBuffer,
                                         0,
                                         _visualizationVertices.data(),
                                         vertexDataSize);
        WriteVisualizationPointBuffer();
        EnsureVisualizationBindGroup();
    }
}

void WebGpuRenderer::WriteVisualizationPointBuffer()
{
    const std::size_t pointDataSize = _visualizationPoints.size() * sizeof(Vec2);
    EnsureVisualizationPointBuffer(pointDataSize);

    if (!_visualizationPoints.empty())
    {
        _context->GetQueue().WriteBuffer(_visualizationPointBuffer,
                                         0,
                                         _visualizationPoints.data(),
                                         pointDataSize);
    }
}

void WebGpuRenderer::DrawQueuedOperation(wgpu::RenderPassEncoder& renderPass,
                                         const QueuedDrawOperation& queuedDrawOperation)
{
    if (const QueuedDrawBatch* drawBatch = std::get_if<QueuedDrawBatch>(&queuedDrawOperation))
    {
        DrawPreparedBatch(renderPass, _drawBatches[drawBatch->index]);
        return;
    }

    if (const QueuedVisualizationDrawBatch* visualizationDrawBatch =
            std::get_if<QueuedVisualizationDrawBatch>(&queuedDrawOperation))
    {
        DrawPreparedVisualizationBatch(renderPass, _visualizationDrawBatches[visualizationDrawBatch->index]);
    }
}

void WebGpuRenderer::DrawPreparedBatch(wgpu::RenderPassEncoder& renderPass, const DrawBatch& drawBatch)
{
    if (drawBatch.hasClip)
    {
        ApplyClipScissor(renderPass, drawBatch.clipRectangle);
    }
    else
    {
        ApplyFullFrameScissor(renderPass);
    }

    if (drawBatch.type == RenderCommandType::FillRectangle)
    {
        const std::uint64_t vertexBufferOffset = static_cast<std::uint64_t>(drawBatch.firstVertex) * sizeof(RectangleVertex);
        const std::uint64_t vertexBufferSize = static_cast<std::uint64_t>(drawBatch.vertexCount) * sizeof(RectangleVertex);

        renderPass.SetPipeline(_rectanglePipeline);
        renderPass.SetVertexBuffer(0, _rectangleVertexBuffer, vertexBufferOffset, vertexBufferSize);
        renderPass.Draw(drawBatch.vertexCount);
        return;
    }

    if (drawBatch.type == RenderCommandType::DrawText)
    {
        const std::uint64_t vertexBufferOffset = static_cast<std::uint64_t>(drawBatch.firstVertex) * sizeof(TextVertex);
        const std::uint64_t vertexBufferSize = static_cast<std::uint64_t>(drawBatch.vertexCount) * sizeof(TextVertex);

        renderPass.SetPipeline(_textPipeline);
        renderPass.SetVertexBuffer(0, _textVertexBuffer, vertexBufferOffset, vertexBufferSize);
        renderPass.SetBindGroup(0, drawBatch.bindGroup);
        renderPass.Draw(drawBatch.vertexCount);
    }
}

void WebGpuRenderer::DrawPreparedVisualizationBatch(wgpu::RenderPassEncoder& renderPass,
                                                    const VisualizationDrawBatch& visualizationDrawBatch)
{
    ApplyVisualizationScissor(renderPass, visualizationDrawBatch.scissorRectangle);

    const std::uint64_t vertexBufferOffset =
        static_cast<std::uint64_t>(visualizationDrawBatch.firstVertex) * sizeof(VisualizationVertex);
    const std::uint64_t vertexBufferSize =
        static_cast<std::uint64_t>(visualizationDrawBatch.vertexCount) * sizeof(VisualizationVertex);

    renderPass.SetPipeline(_visualizationPipeline);
    renderPass.SetVertexBuffer(0, _visualizationVertexBuffer, vertexBufferOffset, vertexBufferSize);
    renderPass.SetBindGroup(0, _visualizationBindGroup);
    renderPass.Draw(visualizationDrawBatch.vertexCount);
}

void WebGpuRenderer::ApplyFullFrameScissor(wgpu::RenderPassEncoder& renderPass) const
{
    renderPass.SetScissorRect(0, 0, _context->GetSurfaceWidth(), _context->GetSurfaceHeight());
}

void WebGpuRenderer::ApplyClipScissor(wgpu::RenderPassEncoder& renderPass, const Rect& clipRectangle) const
{
    const ScissorRectangle scissorRectangle = MakeScissorRectangle(clipRectangle);
    renderPass.SetScissorRect(scissorRectangle.x, scissorRectangle.y, scissorRectangle.width, scissorRectangle.height);
}

void WebGpuRenderer::ApplyVisualizationScissor(wgpu::RenderPassEncoder& renderPass,
                                               const WebGpuScissorRectangle& scissorRectangle) const
{
    renderPass.SetScissorRect(scissorRectangle.x,
                              scissorRectangle.y,
                              scissorRectangle.width,
                              scissorRectangle.height);
}

WebGpuRenderer::ScissorRectangle WebGpuRenderer::MakeScissorRectangle(const Rect& clipRectangle) const noexcept
{
    const float surfaceWidth = static_cast<float>(_context->GetSurfaceWidth());
    const float surfaceHeight = static_cast<float>(_context->GetSurfaceHeight());
    const float left = std::clamp(clipRectangle.position.x, 0.0f, surfaceWidth);
    const float top = std::clamp(clipRectangle.position.y, 0.0f, surfaceHeight);
    const float right = std::clamp(clipRectangle.position.x + clipRectangle.size.x, 0.0f, surfaceWidth);
    const float bottom = std::clamp(clipRectangle.position.y + clipRectangle.size.y, 0.0f, surfaceHeight);

    const auto x = static_cast<std::uint32_t>(std::floor(left));
    const auto y = static_cast<std::uint32_t>(std::floor(top));
    const auto rightEdge = static_cast<std::uint32_t>(std::ceil(std::max(left, right)));
    const auto bottomEdge = static_cast<std::uint32_t>(std::ceil(std::max(top, bottom)));

    return ScissorRectangle{
        .x = x,
        .y = y,
        .width = rightEdge - x,
        .height = bottomEdge - y,
    };
}

} // namespace greenfield

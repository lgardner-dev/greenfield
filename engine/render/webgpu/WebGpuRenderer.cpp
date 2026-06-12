#include "engine/render/webgpu/WebGpuRenderer.h"

#include <algorithm>
#include <array>
#include <cmath>
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
    _submittedCommands.Clear();
}

void WebGpuRenderer::Submit(const RenderCommandList& renderCommands)
{
    _submittedCommands.Append(renderCommands);
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
    DrawRectangles(renderPass);
    DrawText(renderPass);
    renderPass.End();

    wgpu::CommandBuffer commandBuffer = commandEncoder.Finish();
    _context->GetQueue().Submit(1, &commandBuffer);
    _context->GetSurface().Present();
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
    return _submittedCommands.Size();
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

void WebGpuRenderer::BuildRectangleVertices()
{
    _rectangleVertices.clear();
    if (_context->GetSurfaceWidth() == 0 || _context->GetSurfaceHeight() == 0)
    {
        return;
    }

    _rectangleVertices.reserve(_submittedCommands.Size() * 6);

    for (const RenderCommand& renderCommand : _submittedCommands.Commands())
    {
        if (renderCommand.type != RenderCommandType::FillRectangle || !HasPositiveArea(renderCommand.rectangle))
        {
            continue;
        }

        const Rect& rectangle = renderCommand.rectangle;
        const float left = ConvertPixelXToClipSpace(rectangle.position.x, _context->GetSurfaceWidth());
        const float right = ConvertPixelXToClipSpace(rectangle.position.x + rectangle.size.x, _context->GetSurfaceWidth());
        const float top = ConvertPixelYToClipSpace(rectangle.position.y, _context->GetSurfaceHeight());
        const float bottom = ConvertPixelYToClipSpace(rectangle.position.y + rectangle.size.y, _context->GetSurfaceHeight());

        const RectangleVertex topLeft =
            MakeRectangleVertex(left, top, rectangle.position.x, rectangle.position.y, renderCommand);
        const RectangleVertex topRight = MakeRectangleVertex(
            right, top, rectangle.position.x + rectangle.size.x, rectangle.position.y, renderCommand);
        const RectangleVertex bottomLeft = MakeRectangleVertex(
            left, bottom, rectangle.position.x, rectangle.position.y + rectangle.size.y, renderCommand);
        const RectangleVertex bottomRight =
            MakeRectangleVertex(right, bottom, rectangle.position.x + rectangle.size.x, rectangle.position.y + rectangle.size.y, renderCommand);

        _rectangleVertices.push_back(topLeft);
        _rectangleVertices.push_back(bottomLeft);
        _rectangleVertices.push_back(topRight);
        _rectangleVertices.push_back(topRight);
        _rectangleVertices.push_back(bottomLeft);
        _rectangleVertices.push_back(bottomRight);
    }
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

void WebGpuRenderer::DrawRectangles(wgpu::RenderPassEncoder& renderPass)
{
    BuildRectangleVertices();
    if (_rectangleVertices.empty())
    {
        return;
    }

    EnsureRectanglePipeline();

    const std::size_t vertexDataSize = _rectangleVertices.size() * sizeof(RectangleVertex);
    EnsureVertexBuffer(vertexDataSize);

    _context->GetQueue().WriteBuffer(_rectangleVertexBuffer, 0, _rectangleVertices.data(), vertexDataSize);

    renderPass.SetPipeline(_rectanglePipeline);
    renderPass.SetVertexBuffer(0, _rectangleVertexBuffer, 0, vertexDataSize);
    renderPass.Draw(static_cast<std::uint32_t>(_rectangleVertices.size()));
}

void WebGpuRenderer::DrawText(wgpu::RenderPassEncoder& renderPass)
{
    EnsureTextPipeline();
    _textVertices.clear();
    _textBatches.clear();

    for (const RenderCommand& renderCommand : _submittedCommands.Commands())
    {
        if (renderCommand.type != RenderCommandType::DrawText || !HasPositiveArea(renderCommand.rectangle))
        {
            continue;
        }

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

        _textBatches.push_back(TextBatch{
            .bindGroup = fontAtlas->bindGroup,
            .firstVertex = firstVertex,
            .vertexCount = vertexCount,
        });
    }

    if (_textVertices.empty())
    {
        return;
    }

    const std::size_t vertexDataSize = _textVertices.size() * sizeof(TextVertex);
    EnsureTextVertexBuffer(vertexDataSize);
    _context->GetQueue().WriteBuffer(_textVertexBuffer, 0, _textVertices.data(), vertexDataSize);

    renderPass.SetPipeline(_textPipeline);
    renderPass.SetVertexBuffer(0, _textVertexBuffer, 0, vertexDataSize);
    for (const TextBatch& textBatch : _textBatches)
    {
        renderPass.SetBindGroup(0, textBatch.bindGroup);
        renderPass.Draw(textBatch.vertexCount, 1, textBatch.firstVertex);
    }
}

} // namespace greenfield

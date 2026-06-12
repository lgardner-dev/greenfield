#include "engine/render/webgpu/WebGpuRenderer.h"

#include <array>
#include <stdexcept>

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

} // namespace

WebGpuRenderer::WebGpuRenderer(WebGpuContext& context)
    : _context(&context)
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
    renderPass.End();

    wgpu::CommandBuffer commandBuffer = commandEncoder.Finish();
    _context->GetQueue().Submit(1, &commandBuffer);
    _context->GetSurface().Present();
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

} // namespace greenfield

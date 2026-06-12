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
    @location(1) color: vec4f,
};

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) color: vec4f,
};

@vertex
fn VertexMain(input: VertexInput) -> VertexOutput {
    var output: VertexOutput;
    output.position = vec4f(input.position, 0.0, 1.0);
    output.color = input.color;
    return output;
}

@fragment
fn FragmentMain(input: VertexOutput) -> @location(0) vec4f {
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

    const std::array<wgpu::VertexAttribute, 2> vertexAttributes{{
        wgpu::VertexAttribute{
            .format = wgpu::VertexFormat::Float32x2,
            .offset = offsetof(RectangleVertex, position),
            .shaderLocation = 0,
        },
        wgpu::VertexAttribute{
            .format = wgpu::VertexFormat::Float32x4,
            .offset = offsetof(RectangleVertex, color),
            .shaderLocation = 1,
        },
    }};

    const wgpu::VertexBufferLayout vertexBufferLayout{
        .stepMode = wgpu::VertexStepMode::Vertex,
        .arrayStride = sizeof(RectangleVertex),
        .attributeCount = vertexAttributes.size(),
        .attributes = vertexAttributes.data(),
    };

    const wgpu::ColorTargetState colorTargetState{
        .format = surfaceFormat,
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

void WebGpuRenderer::BuildRectangleVertices()
{
    _rectangleVertices.clear();
    if (_context->GetSurfaceWidth() == 0 || _context->GetSurfaceHeight() == 0)
    {
        return;
    }

    _rectangleVertices.reserve(_submittedCommands.Size() * 6);

    const auto makeRectangleVertex = [](float positionX, float positionY, const Color& color) {
        return RectangleVertex{
            .position = {positionX, positionY},
            .color = {color.red, color.green, color.blue, color.alpha},
        };
    };

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

        const RectangleVertex topLeft = makeRectangleVertex(left, top, renderCommand.color);
        const RectangleVertex topRight = makeRectangleVertex(right, top, renderCommand.color);
        const RectangleVertex bottomLeft = makeRectangleVertex(left, bottom, renderCommand.color);
        const RectangleVertex bottomRight = makeRectangleVertex(right, bottom, renderCommand.color);

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

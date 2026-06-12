#pragma once

#include "engine/render/RenderCommandList.h"

namespace greenfield
{

class IRenderer
{
public:
    virtual ~IRenderer() = default;

    virtual void BeginFrame() = 0;
    virtual void Submit(const RenderCommandList& renderCommands) = 0;
    virtual void EndFrame() = 0;
};

} // namespace greenfield

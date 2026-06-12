#pragma once

#include "engine/core/Color.h"
#include "engine/core/Rect.h"

namespace greenfield
{

enum class RenderCommandType
{
    FillRectangle,
};

struct RenderCommand
{
    RenderCommandType type{RenderCommandType::FillRectangle};
    Rect rectangle{};
    Color color{};
    float cornerRadius{0.0f};
};

} // namespace greenfield

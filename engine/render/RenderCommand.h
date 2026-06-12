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
    Color fillColor{};
    float cornerRadius{0.0f};
    Color borderColor{};
    float borderThickness{0.0f};
};

} // namespace greenfield

#pragma once

#include <string>

#include "engine/core/Color.h"
#include "engine/core/Rect.h"

namespace greenfield
{

enum class RenderCommandType
{
    FillRectangle,
    DrawText,
};

struct RenderCommand
{
    RenderCommandType type{RenderCommandType::FillRectangle};
    Rect rectangle{};
    Color fillColor{};
    float cornerRadius{0.0f};
    Color borderColor{};
    float borderThickness{0.0f};
    std::string text{};
    float fontSize{16.0f};
    Color textColor{};
};

} // namespace greenfield

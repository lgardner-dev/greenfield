#pragma once

#include "engine/core/Color.h"

namespace greenfield
{

struct Style
{
    Color windowBackground{0.10f, 0.11f, 0.13f, 1.0f};
    Color panelBackground{0.18f, 0.20f, 0.24f, 0.94f};
    Color panelBorder{0.34f, 0.40f, 0.50f, 0.45f};
    Color accent{0.34f, 0.60f, 0.95f, 1.0f};
    float panelCornerRadius{18.0f};
    float panelBorderThickness{1.0f};
};

} // namespace greenfield

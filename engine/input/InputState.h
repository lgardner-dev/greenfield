#pragma once

#include "engine/core/Vec2.h"

namespace greenfield
{

struct InputState
{
    Vec2 mousePosition{};
    bool leftMouseButtonDown{false};
    bool leftMouseButtonPressed{false};
    bool leftMouseButtonReleased{false};
};

} // namespace greenfield

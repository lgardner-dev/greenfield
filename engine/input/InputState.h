#pragma once

#include <string>

#include "engine/core/Vec2.h"

namespace greenfield
{

struct InputState
{
    Vec2 mousePosition{};
    bool leftMouseButtonDown{false};
    bool leftMouseButtonPressed{false};
    bool leftMouseButtonReleased{false};
    float verticalScrollDelta{0.0f};
    bool tabPressed{false};
    bool shiftTabPressed{false};
    bool enterPressed{false};
    bool spacePressed{false};
    bool backspacePressed{false};
    bool leftArrowPressed{false};
    bool rightArrowPressed{false};
    std::string committedText{};
};

} // namespace greenfield

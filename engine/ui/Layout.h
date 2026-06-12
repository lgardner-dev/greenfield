#pragma once

#include "engine/core/Rect.h"
#include "engine/core/Vec2.h"

namespace greenfield
{

struct Layout
{
    Rect bounds{};
    float padding{16.0f};
    float gap{0.0f};
    Vec2 itemSize{};
};

enum class LayoutDirection
{
    Column,
    Row,
};

struct LayoutContainer
{
    Rect bounds{};
    float padding{0.0f};
    float gap{0.0f};
    Vec2 itemSize{};
};

} // namespace greenfield

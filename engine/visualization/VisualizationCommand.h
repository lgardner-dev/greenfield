#pragma once

#include <variant>
#include <vector>

#include "engine/core/Color.h"
#include "engine/core/Vec2.h"

namespace greenfield
{

// Visualization commands are renderer-neutral screen-space primitives.
// Screen-space X increases right and Y increases down. Viewport2D conversion
// belongs to calling visualization/application code before commands are added.
struct VisualizationLineCommand
{
    Vec2 start{};
    Vec2 end{};
    Color strokeColor{};
    float strokeThickness{1.0f};
};

// Open polyline. The point sequence is owned by the command and is not
// implicitly closed.
struct VisualizationPolylineCommand
{
    std::vector<Vec2> points;
    Color strokeColor{};
    float strokeThickness{1.0f};
};

struct VisualizationPointMarkerCommand
{
    Vec2 center{};
    float radius{1.0f};
    Color fillColor{};
};

using VisualizationCommand =
    std::variant<VisualizationLineCommand, VisualizationPolylineCommand, VisualizationPointMarkerCommand>;

} // namespace greenfield

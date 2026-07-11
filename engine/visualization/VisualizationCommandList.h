#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "engine/core/Color.h"
#include "engine/core/Rect.h"
#include "engine/core/Vec2.h"
#include "engine/visualization/VisualizationCommand.h"

namespace greenfield
{

class VisualizationCommandList
{
public:
    using Container = std::vector<VisualizationCommand>;

    // Clip bounds are screen-space metadata for later backend consumption.
    // Finite reversed bounds are normalized; non-finite bounds are rejected.
    [[nodiscard]] bool SetClipBounds(const Rect& clipBounds);
    [[nodiscard]] Rect ClipBounds() const noexcept;

    // Invalid commands are rejected and leave the command list unchanged.
    [[nodiscard]] bool AddLine(Vec2 start, Vec2 end, Color strokeColor, float strokeThickness);
    [[nodiscard]] bool AddPolyline(std::span<const Vec2> points, Color strokeColor, float strokeThickness);
    [[nodiscard]] bool AddPointMarker(Vec2 center, float radius, Color fillColor);

    // Clear removes commands while preserving the current clip bounds.
    void Clear() noexcept;

    [[nodiscard]] std::size_t Size() const noexcept;
    [[nodiscard]] bool IsEmpty() const noexcept;
    [[nodiscard]] std::span<const VisualizationCommand> Commands() const noexcept;

private:
    Rect _clipBounds{};
    Container _commands;
};

} // namespace greenfield

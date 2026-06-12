#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "engine/render/RenderCommand.h"

namespace greenfield
{

class RenderCommandList
{
public:
    using Container = std::vector<RenderCommand>;

    void Clear() noexcept
    {
        _commands.clear();
    }

    void AddFillRectangle(const Rect& rectangle, const Color& color)
    {
        _commands.push_back(RenderCommand{
            .type = RenderCommandType::FillRectangle,
            .rectangle = rectangle,
            .color = color,
        });
    }

    void Append(const RenderCommandList& other)
    {
        _commands.insert(_commands.end(), other._commands.begin(), other._commands.end());
    }

    [[nodiscard]] bool IsEmpty() const noexcept
    {
        return _commands.empty();
    }

    [[nodiscard]] std::size_t Size() const noexcept
    {
        return _commands.size();
    }

    [[nodiscard]] std::span<const RenderCommand> Commands() const noexcept
    {
        return std::span<const RenderCommand>(_commands);
    }

private:
    Container _commands;
};

} // namespace greenfield

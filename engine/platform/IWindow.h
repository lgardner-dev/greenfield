#pragma once

namespace greenfield
{

class IWindow
{
public:
    virtual ~IWindow() = default;

    virtual void PollEvents() = 0;
    virtual bool ShouldClose() const noexcept = 0;
    virtual int GetWidth() const noexcept = 0;
    virtual int GetHeight() const noexcept = 0;
};

} // namespace greenfield

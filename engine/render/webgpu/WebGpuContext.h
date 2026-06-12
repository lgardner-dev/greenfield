#pragma once

namespace greenfield
{

class IWindow;

class WebGpuContext
{
public:
    explicit WebGpuContext(IWindow& window);

    [[nodiscard]] bool IsInitialized() const noexcept;

private:
    IWindow* _window{nullptr};
    bool _initialized{false};
};

} // namespace greenfield

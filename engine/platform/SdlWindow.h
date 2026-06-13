#pragma once

#include <memory>
#include <string>

#include "engine/platform/IWindow.h"

struct SDL_Window;

namespace greenfield
{

struct SdlWindowDeleter
{
    void operator()(SDL_Window* window) const noexcept;
};

class SdlWindow final : public IWindow
{
public:
    SdlWindow(std::string title, int width, int height);
    ~SdlWindow() override;

    SdlWindow(const SdlWindow&) = delete;
    SdlWindow& operator=(const SdlWindow&) = delete;
    SdlWindow(SdlWindow&&) = delete;
    SdlWindow& operator=(SdlWindow&&) = delete;

    void PollEvents() override;
    [[nodiscard]] bool ShouldClose() const noexcept override;
    [[nodiscard]] int GetWidth() const noexcept override;
    [[nodiscard]] int GetHeight() const noexcept override;
    [[nodiscard]] const InputState& GetInputState() const noexcept override;
    [[nodiscard]] SDL_Window* GetNativeWindow() const noexcept;

private:
    void BeginInputFrame();
    void HandleMouseMotion(float x, float y);
    void HandleMouseButtonDown(unsigned char button, float x, float y);
    void HandleMouseButtonUp(unsigned char button, float x, float y);
    void HandleMouseWheel(float x, float y, float verticalScrollDelta);
    void UpdateWindowSize();

    std::string _title;
    std::unique_ptr<SDL_Window, SdlWindowDeleter> _window;
    int _width{0};
    int _height{0};
    bool _shouldClose{false};
    InputState _inputState{};
};

} // namespace greenfield

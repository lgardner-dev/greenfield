#pragma once

#include <string>

#include "engine/platform/IWindow.h"

struct SDL_Window;

namespace greenfield
{

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
    [[nodiscard]] SDL_Window* GetNativeWindow() const noexcept;

private:
    void UpdateWindowSize();

    std::string _title;
    SDL_Window* _window{nullptr};
    int _width{0};
    int _height{0};
    bool _shouldClose{false};
};

} // namespace greenfield

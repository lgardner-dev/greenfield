#include "engine/platform/SdlWindow.h"

#include <cstddef>
#include <stdexcept>
#include <utility>

#include <SDL3/SDL.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_video.h>

namespace greenfield
{
namespace
{

std::size_t SdlVideoUserCount = 0;

void InitializeSdlVideo()
{
    if (SdlVideoUserCount == 0 && !SDL_Init(SDL_INIT_VIDEO))
    {
        throw std::runtime_error(SDL_GetError());
    }

    ++SdlVideoUserCount;
}

void QuitSdlVideo()
{
    if (SdlVideoUserCount == 0)
    {
        return;
    }

    --SdlVideoUserCount;
    if (SdlVideoUserCount == 0)
    {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }
}

} // namespace

void SdlWindowDeleter::operator()(SDL_Window* window) const noexcept
{
    if (window != nullptr)
    {
        SDL_DestroyWindow(window);
    }
}

SdlWindow::SdlWindow(std::string title, int width, int height)
    : _title(std::move(title))
    , _width(width)
    , _height(height)
{
    InitializeSdlVideo();

    _window.reset(SDL_CreateWindow(_title.c_str(), _width, _height, SDL_WINDOW_RESIZABLE));
    if (_window == nullptr)
    {
        const std::string errorMessage = SDL_GetError();
        QuitSdlVideo();
        throw std::runtime_error(errorMessage);
    }

    UpdateWindowSize();
}

SdlWindow::~SdlWindow()
{
    _window.reset();
    QuitSdlVideo();
}

void SdlWindow::PollEvents()
{
    BeginInputFrame();

    SDL_Event event{};
    while (SDL_PollEvent(&event))
    {
        if (event.type == SDL_EVENT_QUIT)
        {
            _shouldClose = true;
        }
        else if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(_window.get()))
        {
            _shouldClose = true;
        }
        else if (event.type == SDL_EVENT_WINDOW_RESIZED && event.window.windowID == SDL_GetWindowID(_window.get()))
        {
            UpdateWindowSize();
        }
        else if (event.type == SDL_EVENT_MOUSE_MOTION && event.motion.windowID == SDL_GetWindowID(_window.get()))
        {
            HandleMouseMotion(event.motion.x, event.motion.y);
        }
        else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.windowID == SDL_GetWindowID(_window.get()))
        {
            HandleMouseButtonDown(event.button.button, event.button.x, event.button.y);
        }
        else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.windowID == SDL_GetWindowID(_window.get()))
        {
            HandleMouseButtonUp(event.button.button, event.button.x, event.button.y);
        }
        else if (event.type == SDL_EVENT_MOUSE_WHEEL && event.wheel.windowID == SDL_GetWindowID(_window.get()))
        {
            HandleMouseWheel(event.wheel.mouse_x, event.wheel.mouse_y, event.wheel.y);
        }
    }

    UpdateWindowSize();
}

bool SdlWindow::ShouldClose() const noexcept
{
    return _shouldClose;
}

int SdlWindow::GetWidth() const noexcept
{
    return _width;
}

int SdlWindow::GetHeight() const noexcept
{
    return _height;
}

const InputState& SdlWindow::GetInputState() const noexcept
{
    return _inputState;
}

int SdlWindow::GetSurfacePixelWidth() const noexcept
{
    return _width;
}

int SdlWindow::GetSurfacePixelHeight() const noexcept
{
    return _height;
}

NativeSurfaceDescriptor SdlWindow::GetNativeSurfaceDescriptor() const
{
    SDL_PropertiesID properties = SDL_GetWindowProperties(_window.get());
    if (properties == 0)
    {
        throw std::runtime_error("Failed to read SDL window properties: " + std::string(SDL_GetError()));
    }

    void* waylandDisplay = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr);
    void* waylandSurface = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);
    if (waylandDisplay != nullptr && waylandSurface != nullptr)
    {
        return NativeSurfaceDescriptor{
            .kind = NativeSurfaceKind::Wayland,
            .display = waylandDisplay,
            .surface = waylandSurface,
        };
    }

    void* x11Display = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
    const Sint64 x11Window = SDL_GetNumberProperty(properties, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
    if (x11Display != nullptr && x11Window != 0)
    {
        return NativeSurfaceDescriptor{
            .kind = NativeSurfaceKind::X11,
            .display = x11Display,
            .window = static_cast<std::uint64_t>(x11Window),
        };
    }

    return NativeSurfaceDescriptor{};
}

SDL_Window* SdlWindow::GetNativeWindow() const noexcept
{
    return _window.get();
}

void SdlWindow::BeginInputFrame()
{
    _inputState.leftMouseButtonPressed = false;
    _inputState.leftMouseButtonReleased = false;
    _inputState.verticalScrollDelta = 0.0f;
}

void SdlWindow::HandleMouseMotion(float x, float y)
{
    _inputState.mousePosition = Vec2{x, y};
}

void SdlWindow::HandleMouseButtonDown(unsigned char button, float x, float y)
{
    if (button != SDL_BUTTON_LEFT)
    {
        return;
    }

    _inputState.mousePosition = Vec2{x, y};
    if (!_inputState.leftMouseButtonDown)
    {
        _inputState.leftMouseButtonPressed = true;
    }

    _inputState.leftMouseButtonDown = true;
}

void SdlWindow::HandleMouseButtonUp(unsigned char button, float x, float y)
{
    if (button != SDL_BUTTON_LEFT)
    {
        return;
    }

    _inputState.mousePosition = Vec2{x, y};
    if (_inputState.leftMouseButtonDown)
    {
        _inputState.leftMouseButtonReleased = true;
    }

    _inputState.leftMouseButtonDown = false;
}

void SdlWindow::HandleMouseWheel(float x, float y, float verticalScrollDelta)
{
    _inputState.mousePosition = Vec2{x, y};
    _inputState.verticalScrollDelta += verticalScrollDelta;
}

void SdlWindow::UpdateWindowSize()
{
    SDL_GetWindowSizeInPixels(_window.get(), &_width, &_height);
}

} // namespace greenfield

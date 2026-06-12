#include "engine/platform/SdlStartupPresenter.h"

#include <algorithm>
#include <stdexcept>
#include <string>

#include <SDL3/SDL.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_video.h>

#include "engine/platform/SdlWindow.h"

namespace greenfield
{
namespace
{

void ThrowSdlError(const std::string& message)
{
    throw std::runtime_error(message + ": " + SDL_GetError());
}

void FillRectangle(SDL_Surface* surface, const SDL_Rect& rectangle, Uint32 color)
{
    if (!SDL_FillSurfaceRect(surface, &rectangle, color))
    {
        ThrowSdlError("Failed to draw startup rectangle");
    }
}

SDL_Rect MakeCenteredIndicatorRectangle(int surfaceWidth, int surfaceHeight)
{
    const int width = std::max(96, surfaceWidth / 8);
    const int height = 8;

    return SDL_Rect{
        .x = (surfaceWidth - width) / 2,
        .y = (surfaceHeight - height) / 2,
        .w = width,
        .h = height,
    };
}

SDL_Rect MakeMovingIndicatorRectangle(const SDL_Rect& trackRectangle, int frameIndex)
{
    const int segmentWidth = std::max(18, trackRectangle.w / 4);
    const int travelDistance = std::max(1, trackRectangle.w - segmentWidth);
    const int position = (frameIndex * 13) % travelDistance;

    return SDL_Rect{
        .x = trackRectangle.x + position,
        .y = trackRectangle.y,
        .w = segmentWidth,
        .h = trackRectangle.h,
    };
}

} // namespace

SdlStartupPresenter::SdlStartupPresenter(SdlWindow& window) noexcept
    : _window(&window)
{
}

SdlStartupPresenter::~SdlStartupPresenter()
{
    ReleaseWindowSurface();
}

void SdlStartupPresenter::DrawFrame()
{
    if (_window == nullptr || _window->GetNativeWindow() == nullptr)
    {
        return;
    }

    SDL_Surface* surface = SDL_GetWindowSurface(_window->GetNativeWindow());
    if (surface == nullptr)
    {
        ThrowSdlError("Failed to get SDL startup window surface");
    }

    const Uint32 backgroundColor = SDL_MapSurfaceRGB(surface, 18, 20, 23);
    const Uint32 trackColor = SDL_MapSurfaceRGB(surface, 49, 55, 63);
    const Uint32 indicatorColor = SDL_MapSurfaceRGB(surface, 96, 176, 142);

    if (!SDL_FillSurfaceRect(surface, nullptr, backgroundColor))
    {
        ThrowSdlError("Failed to clear startup window surface");
    }

    const SDL_Rect trackRectangle = MakeCenteredIndicatorRectangle(surface->w, surface->h);
    const SDL_Rect indicatorRectangle = MakeMovingIndicatorRectangle(trackRectangle, _frameIndex);

    FillRectangle(surface, trackRectangle, trackColor);
    FillRectangle(surface, indicatorRectangle, indicatorColor);

    if (!SDL_UpdateWindowSurface(_window->GetNativeWindow()))
    {
        ThrowSdlError("Failed to present startup window surface");
    }

    SDL_SyncWindow(_window->GetNativeWindow());
    ++_frameIndex;
}

void SdlStartupPresenter::ReleaseWindowSurface() noexcept
{
    if (_window == nullptr || _window->GetNativeWindow() == nullptr)
    {
        return;
    }

    // SDL window surfaces cannot remain attached when a 3D API surface takes over this window.
    if (SDL_WindowHasSurface(_window->GetNativeWindow()))
    {
        SDL_DestroyWindowSurface(_window->GetNativeWindow());
    }

    _window = nullptr;
}

} // namespace greenfield

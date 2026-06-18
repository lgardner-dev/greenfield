#include "engine/platform/SdlRasterPresenter.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

#include <SDL3/SDL.h>
#include <SDL3/SDL_pixels.h>
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

[[nodiscard]] bool CanMakeRasterByteBuffer(std::size_t width, std::size_t height) noexcept
{
    if (width == 0U || height == 0U)
    {
        return false;
    }

    return width <= std::numeric_limits<std::size_t>::max() / height / SdlRasterPixelByteCount;
}

[[nodiscard]] int ToSdlDimension(std::size_t value)
{
    if (value > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    {
        throw std::invalid_argument("SDL raster presenter dimensions exceed SDL surface limits.");
    }

    return static_cast<int>(value);
}

} // namespace

std::uint8_t ConvertColorChannelToByte(float channel) noexcept
{
    if (!std::isfinite(channel))
    {
        return 0;
    }

    const float clampedChannel = std::clamp(channel, 0.0f, 1.0f);
    return static_cast<std::uint8_t>(std::lround(clampedChannel * 255.0f));
}

SdlRasterPixel ConvertColorToSdlRasterPixel(Color color) noexcept
{
    return SdlRasterPixel{
        .red = ConvertColorChannelToByte(color.red),
        .green = ConvertColorChannelToByte(color.green),
        .blue = ConvertColorChannelToByte(color.blue),
        .alpha = ConvertColorChannelToByte(color.alpha),
    };
}

bool IsValidSdlRasterBuffer(std::size_t width, std::size_t height, std::span<const Color> pixels) noexcept
{
    if (!CanMakeRasterByteBuffer(width, height))
    {
        return false;
    }

    return pixels.size() == width * height;
}

std::vector<std::uint8_t> ConvertRasterToSdlRgbaPixels(std::size_t width, std::size_t height,
                                                       std::span<const Color> pixels)
{
    if (!IsValidSdlRasterBuffer(width, height, pixels))
    {
        return {};
    }

    std::vector<std::uint8_t> rgbaPixels(width * height * SdlRasterPixelByteCount);
    for (std::size_t pixelIndex = 0; pixelIndex < pixels.size(); ++pixelIndex)
    {
        const SdlRasterPixel rasterPixel = ConvertColorToSdlRasterPixel(pixels[pixelIndex]);
        const std::size_t byteIndex = pixelIndex * SdlRasterPixelByteCount;
        rgbaPixels[byteIndex] = rasterPixel.red;
        rgbaPixels[byteIndex + 1U] = rasterPixel.green;
        rgbaPixels[byteIndex + 2U] = rasterPixel.blue;
        rgbaPixels[byteIndex + 3U] = rasterPixel.alpha;
    }

    return rgbaPixels;
}

SdlRasterPresenter::SdlRasterPresenter(SdlWindow& window) noexcept
    : _window(&window)
{
}

bool SdlRasterPresenter::PresentRaster(std::size_t width, std::size_t height, std::span<const Color> pixels)
{
    if (_window == nullptr || _window->GetNativeWindow() == nullptr || !IsValidSdlRasterBuffer(width, height, pixels))
    {
        return false;
    }

    _rgbaPixels = ConvertRasterToSdlRgbaPixels(width, height, pixels);
    if (_rgbaPixels.empty())
    {
        return false;
    }

    const int surfaceWidth = ToSdlDimension(width);
    const int surfaceHeight = ToSdlDimension(height);
    const int pitch = ToSdlDimension(width * SdlRasterPixelByteCount);

    SDL_Surface* sourceSurface =
        SDL_CreateSurfaceFrom(surfaceWidth, surfaceHeight, SDL_PIXELFORMAT_RGBA32, _rgbaPixels.data(), pitch);
    if (sourceSurface == nullptr)
    {
        ThrowSdlError("Failed to create SDL raster source surface");
    }

    SDL_Surface* windowSurface = SDL_GetWindowSurface(_window->GetNativeWindow());
    if (windowSurface == nullptr)
    {
        SDL_DestroySurface(sourceSurface);
        ThrowSdlError("Failed to get SDL raster window surface");
    }

    const bool blitSucceeded = SDL_BlitSurfaceScaled(sourceSurface, nullptr, windowSurface, nullptr, SDL_SCALEMODE_NEAREST);
    SDL_DestroySurface(sourceSurface);
    if (!blitSucceeded)
    {
        ThrowSdlError("Failed to upload SDL raster surface");
    }

    if (!SDL_UpdateWindowSurface(_window->GetNativeWindow()))
    {
        ThrowSdlError("Failed to present SDL raster window surface");
    }

    SDL_SyncWindow(_window->GetNativeWindow());
    return true;
}

} // namespace greenfield

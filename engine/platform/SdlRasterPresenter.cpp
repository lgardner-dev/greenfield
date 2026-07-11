#include "engine/platform/SdlRasterPresenter.h"

#include <chrono>
#include <cmath>
#include <limits>
#include <memory>
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

struct SdlSurfaceDeleter
{
    void operator()(SDL_Surface* surface) const noexcept
    {
        if (surface != nullptr)
        {
            SDL_DestroySurface(surface);
        }
    }
};

using SdlSurfacePtr = std::unique_ptr<SDL_Surface, SdlSurfaceDeleter>;

void ThrowSdlError(const std::string& message)
{
    throw std::runtime_error(message + ": " + SDL_GetError());
}

[[nodiscard]] double MillisecondsBetween(std::chrono::steady_clock::time_point start,
                                         std::chrono::steady_clock::time_point end)
{
    return std::chrono::duration<double, std::milli>(end - start).count();
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
    if (channel >= 0.0f && channel <= 1.0f)
    {
        return static_cast<std::uint8_t>(channel * 255.0f + 0.5f);
    }

    if (channel > 1.0f && std::isfinite(channel))
    {
        return 255;
    }

    return 0;
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

std::size_t GetSdlRgbaByteCount(std::size_t width, std::size_t height) noexcept
{
    if (!CanMakeRasterByteBuffer(width, height))
    {
        return 0U;
    }

    return width * height * SdlRasterPixelByteCount;
}

bool FillSdlRgbaPixels(std::size_t width,
                       std::size_t height,
                       std::span<const Color> pixels,
                       std::span<std::uint8_t> rgbaPixels) noexcept
{
    const std::size_t requiredByteCount = GetSdlRgbaByteCount(width, height);
    if (requiredByteCount == 0U || pixels.size() != width * height || rgbaPixels.size() != requiredByteCount)
    {
        return false;
    }

    for (std::size_t pixelIndex = 0; pixelIndex < pixels.size(); ++pixelIndex)
    {
        const SdlRasterPixel rasterPixel = ConvertColorToSdlRasterPixel(pixels[pixelIndex]);
        const std::size_t byteIndex = pixelIndex * SdlRasterPixelByteCount;
        rgbaPixels[byteIndex] = rasterPixel.red;
        rgbaPixels[byteIndex + 1U] = rasterPixel.green;
        rgbaPixels[byteIndex + 2U] = rasterPixel.blue;
        rgbaPixels[byteIndex + 3U] = rasterPixel.alpha;
    }

    return true;
}

std::vector<std::uint8_t> ConvertRasterToSdlRgbaPixels(std::size_t width, std::size_t height,
                                                       std::span<const Color> pixels)
{
    const std::size_t requiredByteCount = GetSdlRgbaByteCount(width, height);
    if (requiredByteCount == 0U || pixels.size() != width * height)
    {
        return {};
    }

    std::vector<std::uint8_t> rgbaPixels(requiredByteCount);
    if (!FillSdlRgbaPixels(width, height, pixels, rgbaPixels))
    {
        return {};
    }

    return rgbaPixels;
}

SdlRasterPresenter::SdlRasterPresenter(SdlWindow& window) noexcept
    : _window(&window)
{
}

bool SdlRasterPresenter::PresentRaster(std::size_t width,
                                       std::size_t height,
                                       std::span<const Color> pixels,
                                       SdlRasterPresenterFrameTiming* frameTiming)
{
    const auto totalStart = frameTiming == nullptr ? std::chrono::steady_clock::time_point{}
                                                   : std::chrono::steady_clock::now();

    if (_window == nullptr || _window->GetNativeWindow() == nullptr || !IsValidSdlRasterBuffer(width, height, pixels))
    {
        return false;
    }

    auto stageStart = frameTiming == nullptr ? std::chrono::steady_clock::time_point{} : std::chrono::steady_clock::now();
    const std::size_t requiredByteCount = GetSdlRgbaByteCount(width, height);
    if (_rgbaPixels.size() != requiredByteCount)
    {
        _rgbaPixels.resize(requiredByteCount);
    }

    const bool converted = FillSdlRgbaPixels(width, height, pixels, _rgbaPixels);
    if (frameTiming != nullptr)
    {
        frameTiming->colorConversionMilliseconds = MillisecondsBetween(stageStart, std::chrono::steady_clock::now());
    }

    if (!converted || _rgbaPixels.empty())
    {
        return false;
    }

    const int surfaceWidth = ToSdlDimension(width);
    const int surfaceHeight = ToSdlDimension(height);
    const int pitch = ToSdlDimension(width * SdlRasterPixelByteCount);

    stageStart = frameTiming == nullptr ? std::chrono::steady_clock::time_point{} : std::chrono::steady_clock::now();
    SdlSurfacePtr sourceSurface{
        SDL_CreateSurfaceFrom(surfaceWidth, surfaceHeight, SDL_PIXELFORMAT_RGBA32, _rgbaPixels.data(), pitch)};
    if (frameTiming != nullptr)
    {
        frameTiming->sourceSurfaceCreationMilliseconds = MillisecondsBetween(stageStart, std::chrono::steady_clock::now());
    }

    if (sourceSurface == nullptr)
    {
        ThrowSdlError("Failed to create SDL raster source surface");
    }

    stageStart = frameTiming == nullptr ? std::chrono::steady_clock::time_point{} : std::chrono::steady_clock::now();
    SDL_Surface* windowSurface = SDL_GetWindowSurface(_window->GetNativeWindow());
    if (frameTiming != nullptr)
    {
        frameTiming->windowSurfaceAcquisitionMilliseconds = MillisecondsBetween(stageStart, std::chrono::steady_clock::now());
    }

    if (windowSurface == nullptr)
    {
        ThrowSdlError("Failed to get SDL raster window surface");
    }

    stageStart = frameTiming == nullptr ? std::chrono::steady_clock::time_point{} : std::chrono::steady_clock::now();
    const bool blitSucceeded =
        SDL_BlitSurfaceScaled(sourceSurface.get(), nullptr, windowSurface, nullptr, SDL_SCALEMODE_NEAREST);
    if (frameTiming != nullptr)
    {
        frameTiming->blitMilliseconds = MillisecondsBetween(stageStart, std::chrono::steady_clock::now());
    }

    if (!blitSucceeded)
    {
        ThrowSdlError("Failed to upload SDL raster surface");
    }

    stageStart = frameTiming == nullptr ? std::chrono::steady_clock::time_point{} : std::chrono::steady_clock::now();
    if (!SDL_UpdateWindowSurface(_window->GetNativeWindow()))
    {
        ThrowSdlError("Failed to present SDL raster window surface");
    }
    if (frameTiming != nullptr)
    {
        frameTiming->windowSurfaceUpdateMilliseconds = MillisecondsBetween(stageStart, std::chrono::steady_clock::now());
    }

    if (!_hasPresentedRaster)
    {
        stageStart = frameTiming == nullptr ? std::chrono::steady_clock::time_point{} : std::chrono::steady_clock::now();
        _window->Show();
        if (frameTiming != nullptr)
        {
            frameTiming->firstWindowShowMilliseconds = MillisecondsBetween(stageStart, std::chrono::steady_clock::now());
        }
        _hasPresentedRaster = true;

        // SDL documents SDL_SyncWindow as a barrier for pending window state.
        // The Fast2D path requests window state only when revealing the hidden
        // window after the first valid raster frame.
        stageStart = frameTiming == nullptr ? std::chrono::steady_clock::time_point{} : std::chrono::steady_clock::now();
        SDL_SyncWindow(_window->GetNativeWindow());
        if (frameTiming != nullptr)
        {
            frameTiming->windowSyncMilliseconds = MillisecondsBetween(stageStart, std::chrono::steady_clock::now());
        }
    }
    if (frameTiming != nullptr)
    {
        frameTiming->totalMilliseconds = MillisecondsBetween(totalStart, std::chrono::steady_clock::now());
    }

    return true;
}

} // namespace greenfield

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "engine/core/Color.h"

namespace greenfield
{

class SdlWindow;

struct SdlRasterPixel
{
    std::uint8_t red{0};
    std::uint8_t green{0};
    std::uint8_t blue{0};
    std::uint8_t alpha{255};
};

inline constexpr std::size_t SdlRasterPixelByteCount{4U};

[[nodiscard]] std::uint8_t ConvertColorChannelToByte(float channel) noexcept;
[[nodiscard]] SdlRasterPixel ConvertColorToSdlRasterPixel(Color color) noexcept;
[[nodiscard]] bool IsValidSdlRasterBuffer(std::size_t width, std::size_t height,
                                          std::span<const Color> pixels) noexcept;
[[nodiscard]] std::vector<std::uint8_t> ConvertRasterToSdlRgbaPixels(std::size_t width, std::size_t height,
                                                                     std::span<const Color> pixels);

class SdlRasterPresenter
{
public:
    explicit SdlRasterPresenter(SdlWindow& window) noexcept;

    [[nodiscard]] bool PresentRaster(std::size_t width, std::size_t height, std::span<const Color> pixels);

private:
    SdlWindow* _window{nullptr};
    std::vector<std::uint8_t> _rgbaPixels;
    bool _hasPresentedRaster{false};
};

} // namespace greenfield

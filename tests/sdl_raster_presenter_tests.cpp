#include <cstdlib>
#include <limits>
#include <vector>

#include "engine/platform/SdlRasterPresenter.h"

namespace
{

[[nodiscard]] bool TestColorChannelClampAndPacking()
{
    using namespace greenfield;

    const SdlRasterPixel packedColor = ConvertColorToSdlRasterPixel(Color{1.0f, 0.25f, -0.5f, 2.0f});

    return ConvertColorChannelToByte(-1.0f) == 0U && ConvertColorChannelToByte(0.0f) == 0U &&
           ConvertColorChannelToByte(0.5f) == 128U && ConvertColorChannelToByte(1.0f) == 255U &&
           ConvertColorChannelToByte(2.0f) == 255U &&
           ConvertColorChannelToByte(std::numeric_limits<float>::quiet_NaN()) == 0U &&
           packedColor.red == 255U && packedColor.green == 64U && packedColor.blue == 0U &&
           packedColor.alpha == 255U;
}

[[nodiscard]] bool TestRasterConversionOrderAndSize()
{
    using namespace greenfield;

    const std::vector<Color> pixels{
        Color{1.0f, 0.0f, 0.0f, 1.0f},
        Color{0.0f, 1.0f, 0.0f, 0.5f},
    };

    const std::vector<std::uint8_t> rgbaPixels = ConvertRasterToSdlRgbaPixels(2U, 1U, pixels);
    return rgbaPixels.size() == 8U && rgbaPixels[0] == 255U && rgbaPixels[1] == 0U && rgbaPixels[2] == 0U &&
           rgbaPixels[3] == 255U && rgbaPixels[4] == 0U && rgbaPixels[5] == 255U && rgbaPixels[6] == 0U &&
           rgbaPixels[7] == 128U;
}

[[nodiscard]] bool TestRasterFillUpdatesExistingStorage()
{
    using namespace greenfield;

    const std::vector<Color> firstPixels{
        Color{1.0f, 0.0f, 0.0f, 1.0f},
        Color{0.0f, 1.0f, 0.0f, 0.5f},
        Color{0.0f, 0.0f, 1.0f, 0.25f},
        Color{1.0f, 1.0f, 1.0f, 0.0f},
    };
    const std::vector<Color> secondPixels{
        Color{0.0f, 0.0f, 0.0f, 1.0f},
        Color{0.25f, 0.50f, 0.75f, 1.0f},
        Color{1.0f, 0.50f, 0.25f, 0.75f},
        Color{0.0f, 1.0f, 1.0f, 0.5f},
    };

    std::vector<std::uint8_t> rgbaPixels(GetSdlRgbaByteCount(2U, 2U), 99U);
    if (!FillSdlRgbaPixels(2U, 2U, firstPixels, rgbaPixels))
    {
        return false;
    }

    if (!FillSdlRgbaPixels(2U, 2U, secondPixels, rgbaPixels))
    {
        return false;
    }

    return rgbaPixels.size() == 16U &&
           rgbaPixels[0] == 0U && rgbaPixels[1] == 0U && rgbaPixels[2] == 0U && rgbaPixels[3] == 255U &&
           rgbaPixels[4] == 64U && rgbaPixels[5] == 128U && rgbaPixels[6] == 191U && rgbaPixels[7] == 255U &&
           rgbaPixels[8] == 255U && rgbaPixels[9] == 128U && rgbaPixels[10] == 64U && rgbaPixels[11] == 191U &&
           rgbaPixels[12] == 0U && rgbaPixels[13] == 255U && rgbaPixels[14] == 255U && rgbaPixels[15] == 128U;
}

[[nodiscard]] bool TestRasterFillRejectsIncorrectDestinationSize()
{
    using namespace greenfield;

    const std::vector<Color> pixels{
        Color{1.0f, 1.0f, 1.0f, 1.0f},
    };
    std::vector<std::uint8_t> tooSmall(SdlRasterPixelByteCount - 1U, 77U);

    return !FillSdlRgbaPixels(1U, 1U, pixels, tooSmall) && tooSmall[0] == 77U &&
           GetSdlRgbaByteCount(0U, 1U) == 0U;
}

[[nodiscard]] bool TestInvalidRasterInputIsRejected()
{
    using namespace greenfield;

    const std::vector<Color> pixels{
        Color{1.0f, 1.0f, 1.0f, 1.0f},
    };

    return !IsValidSdlRasterBuffer(0U, 1U, pixels) && !IsValidSdlRasterBuffer(1U, 0U, pixels) &&
           !IsValidSdlRasterBuffer(2U, 1U, pixels) && ConvertRasterToSdlRgbaPixels(2U, 1U, pixels).empty();
}

} // namespace

int main()
{
    if (!TestColorChannelClampAndPacking() || !TestRasterConversionOrderAndSize() ||
        !TestRasterFillUpdatesExistingStorage() || !TestRasterFillRejectsIncorrectDestinationSize() ||
        !TestInvalidRasterInputIsRejected())
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

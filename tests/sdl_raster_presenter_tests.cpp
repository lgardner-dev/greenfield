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
        !TestInvalidRasterInputIsRejected())
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

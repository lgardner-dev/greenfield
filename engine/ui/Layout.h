#pragma once

#include <algorithm>
#include <cstddef>

#include "engine/core/Rect.h"
#include "engine/core/Vec2.h"

namespace greenfield
{

struct Layout
{
    Rect bounds{};
    float padding{16.0f};
    float gap{0.0f};
    Vec2 itemSize{};
};

enum class LayoutDirection
{
    Column,
    Row,
};

struct LayoutContainer
{
    Rect bounds{};
    float padding{0.0f};
    float gap{0.0f};
    Vec2 itemSize{};
};

struct EdgeInsets
{
    float left{0.0f};
    float top{0.0f};
    float right{0.0f};
    float bottom{0.0f};
};

struct LayoutSplit
{
    Rect first{};
    Rect second{};
};

enum class FixedRegion
{
    First,
    Second,
};

[[nodiscard]] inline float ClampLayoutValue(float value, float minimum, float maximum) noexcept
{
    return std::clamp(value, minimum, maximum);
}

[[nodiscard]] inline Vec2 ClampSize(Vec2 size, Vec2 minimumSize, Vec2 maximumSize) noexcept
{
    return Vec2{
        .x = ClampLayoutValue(size.x, minimumSize.x, maximumSize.x),
        .y = ClampLayoutValue(size.y, minimumSize.y, maximumSize.y),
    };
}

[[nodiscard]] inline Rect InsetRectangle(const Rect& rectangle, const EdgeInsets& insets) noexcept
{
    const float width = std::max(0.0f, rectangle.size.x - insets.left - insets.right);
    const float height = std::max(0.0f, rectangle.size.y - insets.top - insets.bottom);

    return Rect{
        .position =
            Vec2{
                .x = rectangle.position.x + insets.left,
                .y = rectangle.position.y + insets.top,
            },
        .size = Vec2{.x = width, .y = height},
    };
}

[[nodiscard]] inline Rect InsetRectangle(const Rect& rectangle, float inset) noexcept
{
    return InsetRectangle(rectangle, EdgeInsets{.left = inset, .top = inset, .right = inset, .bottom = inset});
}

[[nodiscard]] inline float PercentageWidth(const Rect& rectangle, float percentage) noexcept
{
    return rectangle.size.x * ClampLayoutValue(percentage, 0.0f, 1.0f);
}

[[nodiscard]] inline float PercentageHeight(const Rect& rectangle, float percentage) noexcept
{
    return rectangle.size.y * ClampLayoutValue(percentage, 0.0f, 1.0f);
}

[[nodiscard]] inline LayoutSplit SplitRectangleHorizontallyByPercentage(const Rect& rectangle, float percentage,
                                                                        float gap = 0.0f) noexcept
{
    const float safeGap = std::max(0.0f, gap);
    const float availableWidth = std::max(0.0f, rectangle.size.x - safeGap);
    const float firstWidth = availableWidth * ClampLayoutValue(percentage, 0.0f, 1.0f);
    const float secondWidth = availableWidth - firstWidth;

    return LayoutSplit{
        .first =
            Rect{
                .position = rectangle.position,
                .size = Vec2{.x = firstWidth, .y = rectangle.size.y},
            },
        .second =
            Rect{
                .position = Vec2{.x = rectangle.position.x + firstWidth + safeGap, .y = rectangle.position.y},
                .size = Vec2{.x = secondWidth, .y = rectangle.size.y},
            },
    };
}

[[nodiscard]] inline LayoutSplit SplitRectangleVerticallyByPercentage(const Rect& rectangle, float percentage,
                                                                      float gap = 0.0f) noexcept
{
    const float safeGap = std::max(0.0f, gap);
    const float availableHeight = std::max(0.0f, rectangle.size.y - safeGap);
    const float firstHeight = availableHeight * ClampLayoutValue(percentage, 0.0f, 1.0f);
    const float secondHeight = availableHeight - firstHeight;

    return LayoutSplit{
        .first =
            Rect{
                .position = rectangle.position,
                .size = Vec2{.x = rectangle.size.x, .y = firstHeight},
            },
        .second =
            Rect{
                .position = Vec2{.x = rectangle.position.x, .y = rectangle.position.y + firstHeight + safeGap},
                .size = Vec2{.x = rectangle.size.x, .y = secondHeight},
            },
    };
}

[[nodiscard]] inline LayoutSplit SplitRectangleHorizontallyFixedFlexible(const Rect& rectangle, float fixedWidth,
                                                                         float gap = 0.0f,
                                                                         FixedRegion fixedRegion =
                                                                             FixedRegion::First) noexcept
{
    const float safeGap = std::max(0.0f, gap);
    const float availableWidth = std::max(0.0f, rectangle.size.x - safeGap);
    const float safeFixedWidth = ClampLayoutValue(fixedWidth, 0.0f, availableWidth);
    const float flexibleWidth = availableWidth - safeFixedWidth;

    if (fixedRegion == FixedRegion::Second)
    {
        return LayoutSplit{
            .first =
                Rect{
                    .position = rectangle.position,
                    .size = Vec2{.x = flexibleWidth, .y = rectangle.size.y},
                },
            .second =
                Rect{
                    .position = Vec2{.x = rectangle.position.x + flexibleWidth + safeGap, .y = rectangle.position.y},
                    .size = Vec2{.x = safeFixedWidth, .y = rectangle.size.y},
                },
        };
    }

    return LayoutSplit{
        .first =
            Rect{
                .position = rectangle.position,
                .size = Vec2{.x = safeFixedWidth, .y = rectangle.size.y},
            },
        .second =
            Rect{
                .position = Vec2{.x = rectangle.position.x + safeFixedWidth + safeGap, .y = rectangle.position.y},
                .size = Vec2{.x = flexibleWidth, .y = rectangle.size.y},
            },
    };
}

[[nodiscard]] inline LayoutSplit SplitRectangleVerticallyFixedFlexible(const Rect& rectangle, float fixedHeight,
                                                                       float gap = 0.0f,
                                                                       FixedRegion fixedRegion =
                                                                           FixedRegion::First) noexcept
{
    const float safeGap = std::max(0.0f, gap);
    const float availableHeight = std::max(0.0f, rectangle.size.y - safeGap);
    const float safeFixedHeight = ClampLayoutValue(fixedHeight, 0.0f, availableHeight);
    const float flexibleHeight = availableHeight - safeFixedHeight;

    if (fixedRegion == FixedRegion::Second)
    {
        return LayoutSplit{
            .first =
                Rect{
                    .position = rectangle.position,
                    .size = Vec2{.x = rectangle.size.x, .y = flexibleHeight},
                },
            .second =
                Rect{
                    .position = Vec2{.x = rectangle.position.x, .y = rectangle.position.y + flexibleHeight + safeGap},
                    .size = Vec2{.x = rectangle.size.x, .y = safeFixedHeight},
                },
        };
    }

    return LayoutSplit{
        .first =
            Rect{
                .position = rectangle.position,
                .size = Vec2{.x = rectangle.size.x, .y = safeFixedHeight},
            },
        .second =
            Rect{
                .position = Vec2{.x = rectangle.position.x, .y = rectangle.position.y + safeFixedHeight + safeGap},
                .size = Vec2{.x = rectangle.size.x, .y = flexibleHeight},
            },
    };
}

[[nodiscard]] inline Rect CalculateColumnRegion(const Rect& rectangle, std::size_t columnIndex,
                                                std::size_t columnCount, float gap = 0.0f) noexcept
{
    if (columnCount == 0U || columnIndex >= columnCount)
    {
        return Rect{};
    }

    const float safeGap = std::max(0.0f, gap);
    const float totalGap = safeGap * static_cast<float>(columnCount - 1U);
    const float columnWidth = std::max(0.0f, rectangle.size.x - totalGap) / static_cast<float>(columnCount);
    const float columnLeft = rectangle.position.x + (columnWidth + safeGap) * static_cast<float>(columnIndex);

    return Rect{
        .position = Vec2{.x = columnLeft, .y = rectangle.position.y},
        .size = Vec2{.x = columnWidth, .y = rectangle.size.y},
    };
}

[[nodiscard]] inline Rect CalculateRowRegion(const Rect& rectangle, std::size_t rowIndex, std::size_t rowCount,
                                             float gap = 0.0f) noexcept
{
    if (rowCount == 0U || rowIndex >= rowCount)
    {
        return Rect{};
    }

    const float safeGap = std::max(0.0f, gap);
    const float totalGap = safeGap * static_cast<float>(rowCount - 1U);
    const float rowHeight = std::max(0.0f, rectangle.size.y - totalGap) / static_cast<float>(rowCount);
    const float rowTop = rectangle.position.y + (rowHeight + safeGap) * static_cast<float>(rowIndex);

    return Rect{
        .position = Vec2{.x = rectangle.position.x, .y = rowTop},
        .size = Vec2{.x = rectangle.size.x, .y = rowHeight},
    };
}

} // namespace greenfield

#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <utility>

namespace greenfield
{

struct UiId
{
    std::string value{};
};

[[nodiscard]] inline UiId MakeUiId(std::string value)
{
    return UiId{.value = std::move(value)};
}

[[nodiscard]] inline UiId MakeUiId(const char* value)
{
    return MakeUiId(std::string{value});
}

[[nodiscard]] inline bool operator==(const UiId& left, const UiId& right) noexcept
{
    return left.value == right.value;
}

struct UiIdHash
{
    [[nodiscard]] std::size_t operator()(const UiId& uiId) const noexcept
    {
        return std::hash<std::string>{}(uiId.value);
    }
};

} // namespace greenfield

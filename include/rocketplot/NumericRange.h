#pragma once

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <ranges>
#include <type_traits>
#include <vector>

namespace rocketplot
{

/// A number type a series accepts: any integer or floating-point type except bool and the character
/// types (so that a string literal is never taken for data).
template <class T>
concept Numeric = std::is_arithmetic_v<T> && !std::same_as<T, bool> && !std::same_as<T, char> &&
                  !std::same_as<T, wchar_t> && !std::same_as<T, char8_t> &&
                  !std::same_as<T, char16_t> && !std::same_as<T, char32_t>;

/// Data a series copies from: any sized range of numbers (std::vector, std::array, std::span,
/// QList, ...). The values are converted to double.
template <class R>
concept NumericRange = std::ranges::forward_range<R> && std::ranges::sized_range<R> &&
                       Numeric<std::remove_cv_t<std::ranges::range_value_t<R>>>;

namespace detail
{

/// Contiguous doubles, which can be read in place through a std::span<const double>.
template <class R>
concept ContiguousDoubles = std::ranges::contiguous_range<R> && std::ranges::sized_range<R> &&
                            std::same_as<std::remove_cv_t<std::ranges::range_value_t<R>>, double>;

/// A copy of @p values as doubles.
template <NumericRange R>
[[nodiscard]] std::vector<double> toDoubleVector(const R& values)
{
    using Value      = std::remove_cv_t<std::ranges::range_value_t<R>>;
    const auto count = std::ranges::size(values);

    std::vector<double> result;
    if constexpr (std::is_signed_v<decltype(count)>)
    {
        result.reserve(static_cast<std::size_t>(count));
    }
    else
    {
        result.reserve(count);
    }
    if constexpr (std::same_as<Value, double>)
    {
        std::ranges::copy(values, std::back_inserter(result));
    }
    else
    {
        for (const auto& value : values)
        {
            result.push_back(static_cast<double>(value));
        }
    }
    return result;
}

}  // namespace detail

}  // namespace rocketplot

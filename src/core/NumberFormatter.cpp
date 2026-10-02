#include "core/NumberFormatter.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <string>
#include <string_view>

namespace rocketplot::core
{

namespace
{

constexpr int              kMaxDecimals       = 15;
constexpr double           kScientificStep    = 1e6;
constexpr double           kScientificSmall   = 1e-4;
constexpr std::string_view kMinusSign         = "−";  // U+2212, the width of a digit in most fonts
constexpr double           kDecimalsTolerance = 1e-6;

// "1.50e+06" -> "1.50e6", "2e-05" -> "2e-5": drop the exponent's plus sign and leading zeros.
std::string compactExponent(std::string text)
{
    const auto e = text.find('e');
    if (e == std::string::npos)
    {
        return text;
    }
    const std::string mantissa = text.substr(0, e);
    std::string       exponent = text.substr(e + 1);
    std::string       sign;
    if (!exponent.empty() && (exponent.front() == '+' || exponent.front() == '-'))
    {
        if (exponent.front() == '-')
        {
            sign = "-";
        }
        exponent.erase(0, 1);
    }
    const auto firstNonZero = exponent.find_first_not_of('0');
    exponent = firstNonZero == std::string::npos ? "0" : exponent.substr(firstNonZero);
    return mantissa + 'e' + sign + exponent;
}

// Whether every digit in the mantissa is zero ("-0.00", "-0e0"): such a value prints without its
// minus sign.
bool isAllZeros(std::string_view text)
{
    const auto end      = text.find('e');
    const auto mantissa = text.substr(0, end);
    return std::ranges::none_of(mantissa, [](char c) { return c >= '1' && c <= '9'; });
}

// ASCII '-' -> U+2212 for every minus sign (the number's and the exponent's).
std::string withMinusSigns(std::string_view text)
{
    std::string result;
    result.reserve(text.size() + 4);
    for (const char c : text)
    {
        if (c == '-')
        {
            result += kMinusSign;
        }
        else
        {
            result += c;
        }
    }
    return result;
}

}  // namespace

int decimalsForStep(double step)
{
    step = std::abs(step);
    if (!(step > 0.0) || !std::isfinite(step))
    {
        return 0;
    }
    double scaled = step;
    for (int decimals = 0; decimals < kMaxDecimals; ++decimals)
    {
        if (std::abs(scaled - std::round(scaled)) <= kDecimalsTolerance * scaled)
        {
            return decimals;
        }
        scaled *= 10.0;
    }
    return kMaxDecimals;
}

TickFormat chooseTickFormat(double step, double magnitude)
{
    step      = std::abs(step);
    magnitude = std::abs(magnitude);
    if (!(step > 0.0) || !std::isfinite(step))
    {
        return {};
    }
    if (step >= kScientificStep || (magnitude > 0.0 && magnitude < kScientificSmall))
    {
        // Mantissa digits: the step measured in units of the largest value's power of ten.
        const double exponent = std::floor(std::log10(std::max(magnitude, step)));
        return {.scientific = true, .decimals = decimalsForStep(step / std::pow(10.0, exponent))};
    }
    return {.scientific = false, .decimals = decimalsForStep(step)};
}

std::string formatTick(double value, const TickFormat& format)
{
    if (std::isnan(value))
    {
        return "NaN";
    }
    if (std::isinf(value))
    {
        return value > 0 ? "\u221E" : withMinusSigns("-\u221E");
    }
    const int         decimals = std::clamp(format.decimals, 0, kMaxDecimals);
    const std::string text     = format.scientific
                                     ? compactExponent(std::format("{:.{}e}", value, decimals))
                                     : std::format("{:.{}f}", value, decimals);
    if (isAllZeros(text))
    {
        return "0";
    }
    return withMinusSigns(text);
}

}  // namespace rocketplot::core

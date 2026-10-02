#include "core/NumberFormatter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <format>
#include <string>
#include <string_view>

#include "rocketplot/Range.h"

namespace rocketplot::core
{

namespace
{

constexpr int              kMaxDecimals       = 15;
constexpr double           kDecimalsTolerance = 1e-6;
constexpr std::string_view kMinusSign         = "−";  // U+2212, the width of a digit
constexpr std::string_view kTimes             = "×";
// An offset is only worth it when it leaves out at least 4 leading digits (the quotient is >=
// 10^3).
constexpr double kOffsetMinimum = 1000.0;
// AUTO factors out 10^n when the largest label's order of magnitude is at least kMultiplierFrom or
// at most kMultiplierBelow (labels outside 1e-4 .. 1e6).
constexpr int kMultiplierFrom  = 6;
constexpr int kMultiplierBelow = -5;
// Log axes write powers of ten from 10^kPlainLogLow to 10^kPlainLogHigh as plain numbers.
constexpr int kPlainLogLow  = -3;
constexpr int kPlainLogHigh = 4;
// SI prefixes from 10^-24 to 10^24, a factor of 1000 apart.
constexpr int                              kSiLowest   = -24;
constexpr int                              kSiHighest  = 24;
constexpr std::array<std::string_view, 17> kSiPrefixes = {
    "y", "z", "a", "f", "p", "n", "µ", "m", "", "k", "M", "G", "T", "P", "E", "Z", "Y",
};
constexpr std::array<std::string_view, 10> kSuperscriptDigits = {
    "⁰", "¹", "²", "³", "⁴", "⁵", "⁶", "⁷", "⁸", "⁹",
};
constexpr std::string_view kSuperscriptMinus = "⁻";
// Significant digits when writing a number as short as possible.
constexpr int kSignificantDigits = 15;

double pow10(int exponent)
{
    return std::pow(10.0, exponent);
}

// The order of magnitude: 1234 → 3, 0.05 → -2. Nudged so exact powers of ten land on themselves.
int magnitudeOf(double value)
{
    return static_cast<int>(std::floor(std::log10(std::abs(value)) + 1e-12));
}

// The exponent of the SI prefix for an order of magnitude: the multiple of 3 at or below it.
int thousandsOf(int magnitude)
{
    return magnitude >= 0 ? (magnitude / 3) * 3 : -(((-magnitude + 2) / 3) * 3);
}

std::string_view siPrefix(int exponent)
{
    return kSiPrefixes.at(static_cast<std::size_t>((exponent - kSiLowest) / 3));
}

// Whether every digit is zero ("-0.00"): such a value prints without its minus sign.
bool isAllZeros(std::string_view text)
{
    return std::ranges::none_of(text, [](char c) { return c >= '1' && c <= '9'; });
}

// ASCII '-' -> U+2212.
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

// Fixed notation; zero (however rounded) is "0".
std::string fixed(double value, int decimals)
{
    const std::string text = std::format("{:.{}f}", value, std::clamp(decimals, 0, kMaxDecimals));
    return isAllZeros(text) ? "0" : withMinusSigns(text);
}

// As few digits as needed: 1.7, 1.700000123, 250.
std::string shortest(double value)
{
    return withMinusSigns(std::format("{:.{}g}", value, kSignificantDigits));
}

// "1.7×10⁹" (or "10⁹").
std::string powerOfTen(double value, int magnitude)
{
    const std::string mantissa = shortest(value / pow10(magnitude));
    return (mantissa == "1" ? std::string() : mantissa + std::string(kTimes)) + "10" +
           superscript(magnitude);
}

// A positive number for an annotation: plain from 10^-4 to 10^6, else powerOfTen().
std::string scientific(double value)
{
    const int magnitude = magnitudeOf(value);
    if (magnitude > kMultiplierBelow && magnitude < kMultiplierFrom)
    {
        return shortest(value);
    }
    return powerOfTen(value, magnitude);
}

// A positive number with its own SI prefix: 1.5k, 250µ. Outside the prefixes: scientific().
std::string withPrefix(double value)
{
    const int exponent = thousandsOf(magnitudeOf(value));
    if (exponent < kSiLowest || exponent > kSiHighest)
    {
        return scientific(value);
    }
    return shortest(value / pow10(exponent)) + std::string(siPrefix(exponent));
}

// The offset that leaves out the leading digits min and max share (matplotlib's ScalarFormatter
// rule), or 0 when that saves fewer than 4 digits or the range includes zero.
double commonOffset(double min, double max)
{
    if (!(min < max) || (min <= 0.0 && max >= 0.0))
    {
        return 0.0;
    }
    const double low      = std::min(std::abs(min), std::abs(max));
    const double high     = std::max(std::abs(min), std::abs(max));
    const auto   quotient = [](double value, int magnitude) {
        return std::floor(value / pow10(magnitude));
    };
    const int top    = static_cast<int>(std::ceil(std::log10(high)));
    const int bottom = top - (2 * kSignificantDigits);
    // One power of ten below the smallest at which low and high still agree.
    int magnitude = top;
    while (magnitude > bottom && quotient(low, magnitude) == quotient(high, magnitude))
    {
        --magnitude;
    }
    ++magnitude;
    if ((high - low) / pow10(magnitude) <= 1e-2)
    {
        // The range straddles a multiple of a large power of ten: use the smallest power of ten
        // at which they are no more than one apart.
        magnitude = top;
        while (magnitude > bottom && quotient(high, magnitude) - quotient(low, magnitude) <= 1.0)
        {
            --magnitude;
        }
        ++magnitude;
    }
    const double leading = quotient(high, magnitude);
    if (leading < kOffsetMinimum)
    {
        return 0.0;
    }
    return std::copysign(leading * pow10(magnitude), min);
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

Labeling chooseLabeling(Range range, double step, NumberStyle style)
{
    Labeling labeling;
    if (!(step > 0.0) || !std::isfinite(step) || !range.isValid())
    {
        return labeling;
    }
    labeling.step = step;
    if (style == NumberStyle::PLAIN)
    {
        labeling.decimals = decimalsForStep(step);
        return labeling;
    }
    labeling.offset = commonOffset(range.min, range.max);
    if (style == NumberStyle::SI)
    {
        labeling.si       = true;  // every label gets its own prefix
        labeling.decimals = decimalsForStep(step);
        return labeling;
    }
    const double largest =
        std::max(std::abs(range.min - labeling.offset), std::abs(range.max - labeling.offset));
    if (largest > 0.0 && std::isfinite(largest))
    {
        const int magnitude = magnitudeOf(largest);
        if (magnitude >= kMultiplierFrom || magnitude <= kMultiplierBelow)
        {
            labeling.exponent = magnitude;
        }
    }
    labeling.decimals = decimalsForStep(step / pow10(labeling.exponent));
    return labeling;
}

std::string formatLabel(double value, const Labeling& labeling)
{
    if (std::isnan(value))
    {
        return "NaN";
    }
    if (std::isinf(value))
    {
        return value > 0 ? "∞" : withMinusSigns("-∞");
    }
    const double shifted = value - labeling.offset;
    if (!labeling.si)
    {
        return fixed(shifted / pow10(labeling.exponent), labeling.decimals);
    }
    // Rounded to the step's precision first, so 999.99999 becomes 1k and not 1000.
    const double rounded =
        std::round(shifted * pow10(labeling.decimals)) / pow10(labeling.decimals);
    if (rounded == 0.0)
    {
        return "0";
    }
    const int exponent = std::clamp(thousandsOf(magnitudeOf(rounded)), kSiLowest, kSiHighest);
    return fixed(rounded / pow10(exponent), decimalsForStep(labeling.step / pow10(exponent))) +
           std::string(siPrefix(exponent));
}

std::string labelingAnnotation(const Labeling& labeling)
{
    std::string text;
    if (!labeling.si && labeling.exponent != 0)
    {
        text = std::string(kTimes) + "10" + superscript(labeling.exponent);
    }
    if (labeling.offset != 0.0)
    {
        if (!text.empty())
        {
            text += ' ';
        }
        text += labeling.offset > 0.0 ? std::string("+") : std::string(kMinusSign);
        text += labeling.si ? withPrefix(std::abs(labeling.offset))
                            : scientific(std::abs(labeling.offset));
    }
    return text;
}

std::string formatLogLabel(double value, NumberStyle style)
{
    if (!(value > 0.0) || !std::isfinite(value))
    {
        return {};
    }
    if (style == NumberStyle::SI)
    {
        return withPrefix(value);
    }
    const int magnitude = magnitudeOf(value);
    if (style == NumberStyle::PLAIN || (magnitude >= kPlainLogLow && magnitude <= kPlainLogHigh))
    {
        return fixed(value, std::max(0, -magnitude));
    }
    return powerOfTen(value, magnitude);
}

std::string superscript(int exponent)
{
    std::string text = exponent < 0 ? std::string(kSuperscriptMinus) : std::string();
    for (const char digit : std::to_string(std::abs(exponent)))
    {
        text += kSuperscriptDigits.at(static_cast<std::size_t>(digit - '0'));
    }
    return text;
}

}  // namespace rocketplot::core

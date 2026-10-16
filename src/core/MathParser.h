#pragma once

#include <cstddef>
#include <expected>
#include <string>
#include <string_view>

#include "core/MathAtom.h"

namespace rocketplot::core
{

/// Why a formula can't be typeset: what is wrong, and where (a byte offset into the TeX).
struct MathError
{
    std::size_t position = 0;
    std::string message;
};

/// The math list of @p tex (UTF-8), the TeX between a label's $ signs: a subset of LaTeX's math
/// (characters, Greek and symbols, scripts and primes, \frac, \sqrt, \left \right, accents,
/// alphabets, \text, spaces, styles, matrix and cases environments). Nested at most 32 deep.
[[nodiscard]] std::expected<MathList, MathError> parseMath(std::string_view tex);

}  // namespace rocketplot::core

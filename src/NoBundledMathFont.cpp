// Built in place of the math font's bytes when ROCKETPLOT_BUNDLE_MATH_FONT is OFF.
#include <cstddef>
#include <span>

#include "BundledMathFont.h"

namespace rocketplot
{

std::span<const std::byte> bundledMathFont()
{
    return {};
}

}  // namespace rocketplot

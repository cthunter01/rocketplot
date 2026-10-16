#pragma once

#include <string>

#include "core/MathAtom.h"

namespace rocketplot::core
{

/// A formula as plain text, for where only text goes (a CSV header, the title of an SVG file):
/// symbols as their Unicode characters, scripts after _ and ^, a fraction with a slash: "v_z",
/// "α^2", "(a + b)/c", "√x". In UTF-8.
[[nodiscard]] std::string mathToPlainText(const MathList& list);

}  // namespace rocketplot::core

#pragma once

#include <QString>

#include "rocketplot/export.h"

/// Text a plot shows: titles, axis labels, legend names and annotation texts.
namespace rocketplot
{

/// Text that a plot shows exactly as @p text is written: none of its characters are taken as
/// markup or math. For names that users type or that come from files, which may hold a `<`, an
/// `&` or `$` signs:
///
/// @code
/// series->setName(rocketplot::literalText(part.name()));  // "<b>$5" shows as <b>$5
/// @endcode
///
/// Text that would be shown as written anyway is returned as it is. Any other text comes back as
/// rich text that shows those characters, spaces and line breaks included; such text is not
/// elided where plain text would be. CSV headers and the titles of exported files have the
/// characters of @p text.
[[nodiscard]] ROCKETPLOT_EXPORT QString literalText(const QString& text);

}  // namespace rocketplot

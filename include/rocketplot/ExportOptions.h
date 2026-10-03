#pragma once

#include <QSize>
#include <optional>

#include "rocketplot/Theme.h"

namespace rocketplot
{

/// How a plot is drawn when it is exported (PlotWidget::renderToImage(), exportImage(),
/// exportSvg(), exportPdf(), exportTo(), copyToClipboard()). By default an export looks like the
/// widget does.
///
/// @code
/// plot->exportPdf("ascent.pdf");
/// plot->exportImage("ascent.png", {.size = {600, 400}, .dpi = 300.0});   // 1875 × 1250 pixels
/// plot->exportSvg("ascent.svg", {.theme = rocketplot::Theme::print()});  // from a dark window
/// @endcode
struct ExportOptions
{
    /// The size the plot is laid out in, in device-independent pixels, as a widget's size is (96 to
    /// the inch). Empty, the default: the plot widget's own size.
    // NOLINTNEXTLINE(readability-redundant-member-init): lets designated initializers leave it out
    QSize size = {};

    /// For images: pixels per inch. At 96 an image has one pixel per device-independent pixel; at
    /// 300, text and lines are the size they are at 96 but 3.125 times as sharp, for print. 0, the
    /// default: as sharp as the screen the widget is on.
    double dpi = 0.0;

    /// The theme to draw with instead of the plot's own, which stays as it is.
    // NOLINTNEXTLINE(readability-redundant-member-init): lets designated initializers leave it out
    std::optional<Theme> theme = {};
};

}  // namespace rocketplot

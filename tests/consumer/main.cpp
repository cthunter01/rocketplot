// A program that uses rocketplot as a project that fetched it would: every public header, data
// moved in and appended to (what a sanitizer checks), math in a title, user-typed text, and an
// export. Runs without a display:
//   QT_QPA_PLATFORM=offscreen consumer <folder to write to>

#include <QApplication>
#include <QDir>
#include <QImage>
#include <QString>
#include <cstdio>
#include <utility>
#include <vector>

#include "rocketplot/Annotation.h"
#include "rocketplot/Axis.h"
#include "rocketplot/EventMarker.h"
#include "rocketplot/ExportOptions.h"
#include "rocketplot/InputBindings.h"
#include "rocketplot/Legend.h"
#include "rocketplot/LineSeries.h"
#include "rocketplot/NumericRange.h"
#include "rocketplot/PlotGrid.h"
#include "rocketplot/PlotLink.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/Range.h"
#include "rocketplot/ReferenceLine.h"
#include "rocketplot/ScatterSeries.h"
#include "rocketplot/Series.h"
#include "rocketplot/ShadedSpan.h"
#include "rocketplot/TextAnnotation.h"
#include "rocketplot/Theme.h"
#include "rocketplot/UniformX.h"
#include "rocketplot/enums.h"
#include "rocketplot/plottime.h"
#include "rocketplot/text.h"

int main(int argc, char* argv[])
{
    const QApplication app(argc, argv);
    const QDir         folder(argc > 1 ? QString::fromLocal8Bit(argv[1]) : QDir::currentPath());

    rocketplot::PlotWidget plot;
    plot.resize(640, 400);
    plot.setTitle(R"(Ascent, $\frac{dh}{dt} > 0$)");
    plot.xAxis()->setLabel("Time (s)");

    // Vectors with room to spare, moved into the series, then grown in place.
    std::vector<double> time;
    std::vector<double> altitude;
    time.reserve(1000);
    altitude.reserve(1000);
    for (int i = 0; i < 100; ++i)
    {
        time.push_back(i * 0.1);
        altitude.push_back(i * i * 0.01);
    }
    rocketplot::LineSeries* line =
        plot.addLine(std::move(time), std::move(altitude), rocketplot::literalText("<Stage 1> $5"));
    for (int i = 100; i < 500; ++i)
    {
        line->append(i * 0.1, i * i * 0.01);
    }
    plot.addEvent(5.0, "Burnout");

    const QImage image   = plot.renderToImage({.size = {640, 400}, .dpi = 96.0});
    const bool   written = plot.exportPdf(folder.filePath("consumer.pdf"),
                                          {.creator = QStringLiteral("rocketplot consumer")});
    if (image.isNull() || !written || line->size() != 500)
    {
        std::fputs("consumer: the plot could not be drawn or written\n", stderr);
        return 1;
    }
    std::puts("consumer: ok");
    return 0;
}

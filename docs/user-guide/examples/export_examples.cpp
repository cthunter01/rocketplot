// Examples of export.md.

#include <QDir>
#include <QImage>
#include <QMarginsF>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QRectF>
#include <QString>
#include <QtLogging>
#include <vector>

#include "Examples.h"
#include "SampleData.h"
#include "rocketplot/Axis.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/Theme.h"

namespace rocketplot::guide
{

namespace
{

void files(const QString& scratch)
{
    const Ascent ascent = sampleAscent();
    PlotWidget   window;
    PlotWidget   second;
    PlotWidget*  plot         = &window;
    PlotWidget*  velocityPlot = &second;
    plot->addLine(ascent.time, ascent.altitude, "Altitude");
    velocityPlot->addLine(ascent.time, ascent.velocity, "Velocity");
    const QDir folder(scratch);
    // [export-files]
    // The suffix picks the format.
    for (const QString name : {"ascent.png", "ascent.svg", "ascent.pdf", "ascent.csv"})
    {
        if (!plot->exportTo(folder.filePath(name)))
        {
            qWarning() << "could not write" << name;
        }
    }
    // [/export-files]

    // [export-options]
    // Laid out as a 600 × 400 widget would be, at 300 pixels per inch: 1875 × 1250 pixels.
    const bool imageWritten =
        plot->exportImage(folder.filePath("figure.png"), {.size = {600, 400}, .dpi = 300.0});

    // From a dark window onto white paper.
    const bool pageWritten = plot->exportPdf(
        folder.filePath("figure.pdf"), {.size = {600, 400}, .theme = rocketplot::Theme::print()});
    // [/export-options]
    static_cast<void>(imageWritten);
    static_cast<void>(pageWritten);

    // [export-image]
    const QImage image =
        plot->renderToImage({.size = {480, 300}, .dpi = 96.0});  // 480 × 300 pixels
    plot->copyToClipboard();  // what "Copy image" in the context menu does
    // [/export-image]

    // [export-paint]
    // A PDF page with two plots, one above the other.
    QPdfWriter writer(folder.filePath("report.pdf"));
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setPageMargins(QMarginsF(15.0, 15.0, 15.0, 15.0), QPageLayout::Millimeter);
    writer.setResolution(96);  // one unit of the painter is one device-independent pixel
    QPainter     painter(&writer);
    const double width  = writer.width();
    const double height = writer.height() / 2.0;
    plot->paint(painter, QRectF(0.0, 0.0, width, height));
    velocityPlot->paint(painter, QRectF(0.0, height, width, height));
    painter.end();
    // [/export-paint]
    static_cast<void>(image);
}

// A plot whose CSV is short enough to show.
void fillSmallPlot(PlotWidget& plot)
{
    const std::vector<double> time{0.0, 1.0, 2.0, 3.0, 4.0, 5.0};
    const std::vector<double> altitude{0.0, 0.4, 1.7, 3.9, 6.8, 10.6};
    const std::vector<double> velocity{0.0, 8.1, 17.9, 26.2, 33.0, 43.5};
    plot.xAxis()->setLabel("Time (s)");
    plot.addLine(time, altitude, "Altitude (m)");
    plot.addLine(time, velocity, "Velocity (m/s)");
}

QString csvOfTheView()
{
    PlotWidget window;
    fillSmallPlot(window);
    const PlotWidget* plot = &window;
    // [export-csv]
    plot->xAxis()->setRange(1.0, 4.0);  // only what is in view is written
    const QString text = plot->toCsv();
    // [/export-csv]
    return text.trimmed();
}

void csvFiles(const QString& scratch)
{
    PlotWidget window;
    fillSmallPlot(window);
    const PlotWidget* plot = &window;
    const QDir        folder(scratch);
    // [export-csv-file]
    const bool csvWritten = plot->exportCsv(folder.filePath("ascent.csv"));        // to a file
    const bool tsvWritten = plot->exportCsv(folder.filePath("ascent.tsv"), '\t');  // with tabs
    // [/export-csv-file]
    static_cast<void>(csvWritten);
    static_cast<void>(tsvWritten);
}

}  // namespace

void addExportExamples(Examples& examples)
{
    examples.demonstrations.push_back({.name = QStringLiteral("exporting"), .run = files});
    examples.demonstrations.push_back({.name = QStringLiteral("csv files"), .run = csvFiles});
    examples.outputs.push_back({.name = QStringLiteral("export-csv"), .text = csvOfTheView});
}

}  // namespace rocketplot::guide

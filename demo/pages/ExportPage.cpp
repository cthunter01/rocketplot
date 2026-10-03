#include <QCheckBox>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QObject>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>
#include <functional>
#include <initializer_list>
#include <vector>

#include "DemoPage.h"
#include "Generators.h"
#include "rocketplot/Axis.h"
#include "rocketplot/EventMarker.h"
#include "rocketplot/ExportOptions.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/Theme.h"

namespace rocketplot::demo
{

namespace
{

// Asks where to save a file of the kind @p filter names, writes it with @p write and says so if
// that fails.
void saveAs(QWidget* parent, const QString& filter, const QString& suffix,
            const std::function<bool(const QString&)>& write)
{
    QString fileName =
        QFileDialog::getSaveFileName(parent, QStringLiteral("Export plot"), QString(), filter);
    if (fileName.isEmpty())
    {
        return;
    }
    if (!fileName.endsWith(suffix, Qt::CaseInsensitive))
    {
        fileName += suffix;
    }
    if (!write(fileName))
    {
        QMessageBox::warning(
            parent, QStringLiteral("Export plot"),
            QStringLiteral("Could not write %1.").arg(QDir::toNativeSeparators(fileName)));
    }
}

QWidget* create(QWidget* parent)
{
    auto* page     = new QWidget(parent);
    auto* layout   = new QVBoxLayout(page);
    auto* controls = new QHBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    auto* copy     = new QPushButton(QStringLiteral("Copy image"), page);
    auto* image    = new QPushButton(QStringLiteral("PNG at 300 dpi…"), page);
    auto* svg      = new QPushButton(QStringLiteral("SVG…"), page);
    auto* pdf      = new QPushButton(QStringLiteral("PDF…"), page);
    auto* csv      = new QPushButton(QStringLiteral("CSV…"), page);
    auto* forPaper = new QCheckBox(QStringLiteral("In the print theme"), page);
    for (QWidget* control : std::initializer_list<QWidget*>{copy, image, svg, pdf, csv, forPaper})
    {
        controls->addWidget(control);
    }
    controls->addStretch(1);
    layout->addLayout(controls);

    // [snippet]
    auto* plot = new rocketplot::PlotWidget(page);
    plot->setTitle("Engine chill-down");
    plot->xAxis()->setLabel("Time (s)");
    plot->yAxis()->setLabel("Temperature (°C)");
    const std::vector<double> time = linspace(0.0, 600.0, 6001);
    plot->addLine(time, warmUp(time, 18.0, -160.0, 120.0, 1.5, 1), "LOX feed line");
    plot->addLine(time, warmUp(time, 21.0, -95.0, 200.0, 1.0, 2), "Turbopump housing");
    plot->addEvent(360.0, "Chill-down complete");

    // An export draws the plot afresh, as the widget shows it but without the crosshair: at the
    // widget's size or any other, and in the plot's theme or another (here: for paper, whatever
    // the window looks like).
    const auto options = [=] {
        rocketplot::ExportOptions chosen;
        if (forPaper->isChecked())
        {
            chosen.theme = rocketplot::Theme::print();
        }
        return chosen;
    };
    QObject::connect(copy, &QPushButton::clicked, plot, [=] { plot->copyToClipboard(options()); });
    QObject::connect(image, &QPushButton::clicked, plot, [=] {
        // 600 × 400 at 300 dpi: 1875 × 1250 pixels, with text and lines as large as on screen.
        rocketplot::ExportOptions sharp = options();
        sharp.size                      = {600, 400};
        sharp.dpi                       = 300.0;
        saveAs(page, "PNG image (*.png)", ".png",
               [=](const QString& file) { return plot->exportImage(file, sharp); });
    });
    QObject::connect(svg, &QPushButton::clicked, plot, [=] {
        saveAs(page, "SVG drawing (*.svg)", ".svg",
               [=](const QString& file) { return plot->exportSvg(file, options()); });
    });
    QObject::connect(pdf, &QPushButton::clicked, plot, [=] {
        saveAs(page, "PDF document (*.pdf)", ".pdf",
               [=](const QString& file) { return plot->exportPdf(file, options()); });
    });
    // The data in view: zoom in first to export a part of it.
    QObject::connect(csv, &QPushButton::clicked, plot, [=] {
        saveAs(page, "CSV data (*.csv)", ".csv",
               [=](const QString& file) { return plot->exportCsv(file); });
    });
    // [/snippet]

    layout->addWidget(plot, 1);
    return page;
}

}  // namespace

DemoPage exportPage()
{
    return {
        .title       = QStringLiteral("Export"),
        .description = QStringLiteral(
            "A plot exports as an image at any size and sharpness, as an SVG or PDF drawing, onto "
            "the clipboard, and its data as CSV: only the part in view, so zoom in to export a "
            "stretch of it. Any plot's context menu (right-click) has Copy image and Export… too."),
        .sourceFile = QStringLiteral("ExportPage.cpp"),
        .create     = create,
    };
}

}  // namespace rocketplot::demo

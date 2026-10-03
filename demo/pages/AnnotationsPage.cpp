#include <QColor>
#include <QPointF>
#include <QString>
#include <QWidget>
#include <Qt>

#include "DemoPage.h"
#include "Generators.h"
#include "rocketplot/Axis.h"
#include "rocketplot/EventMarker.h"
#include "rocketplot/LineSeries.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/ReferenceLine.h"
#include "rocketplot/ShadedSpan.h"
#include "rocketplot/TextAnnotation.h"

namespace rocketplot::demo
{

namespace
{

QWidget* create(QWidget* parent)
{
    // [snippet]
    auto* plot = new rocketplot::PlotWidget(parent);
    plot->setTitle("Ascent acceleration");
    plot->xAxis()->setLabel("Time (s)");
    plot->yAxis()->setLabel("Acceleration (m/s²)");
    const Ascent ascent = simulateAscent(300.0, 10.0, 3);
    plot->addLine(ascent.time, ascent.acceleration);

    // Events: a line with a flag. Flags too close to share a row are staggered.
    plot->addEvent(0.0, "Liftoff");
    plot->addEvent(150.0, "MECO");
    plot->addEvent(153.0, "Stage separation");
    plot->addEvent(160.0, "Second-stage ignition");

    // A shaded span between two x values, under the data.
    plot->addVerticalSpan(55.0, 85.0, "Max Q");

    // Reference lines. Autoscale ignores annotations unless told to make room for them.
    auto* limit = plot->addHorizontalLine(26.0, "Structural limit");
    limit->setColor(QColor(0xd0, 0x3b, 0x3b));
    limit->setIncludedInAutoscale(true);
    plot->addHorizontalLine(-9.81, "Free fall")->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);

    // Text at a data point; an offset (in pixels) moves it away and draws an arrow back.
    auto* note = plot->addText(95.0, 14.6, "Thrust is steady: the vehicle<br>gets lighter");
    note->setOffset(QPointF(60.0, 80.0));
    // [/snippet]
    return plot;
}

}  // namespace

DemoPage annotationsPage()
{
    return {
        .title       = QStringLiteral("Annotations"),
        .description = QStringLiteral(
            "Reference lines, shaded spans, text with an arrow and named events mark up the data "
            "and move with it. Zoom in around 155 s: the event flags move onto one row as they "
            "come apart. Labels keep to the plot's text colors; the line or flag beside them "
            "carries the color."),
        .sourceFile = QStringLiteral("AnnotationsPage.cpp"),
        .create     = create,
    };
}

}  // namespace rocketplot::demo

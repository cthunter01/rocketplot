// Examples of annotations.md.

#include <QColor>
#include <QIcon>
#include <QObject>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStyle>
#include <Qt>
#include <limits>
#include <optional>
#include <vector>

#include "Examples.h"
#include "SampleData.h"
#include "rocketplot/Annotation.h"
#include "rocketplot/Axis.h"
#include "rocketplot/EventMarker.h"
#include "rocketplot/IconAnnotation.h"
#include "rocketplot/LineSeries.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/RectAnnotation.h"
#include "rocketplot/ReferenceLine.h"
#include "rocketplot/ShadedSpan.h"
#include "rocketplot/TextAnnotation.h"
#include "rocketplot/enums.h"

namespace rocketplot::guide
{

namespace
{

void annotated(PlotWidget* plot)
{
    const Ascent ascent = sampleAscent();
    plot->addLine(ascent.time, ascent.acceleration);
    plot->setTitle("Ascent acceleration");
    plot->xAxis()->setLabel("Time (s)");
    plot->yAxis()->setLabel("Acceleration (m/s²)");
    // [annotations]
    // A value that matters: a line across the plot, with a label.
    rocketplot::ReferenceLine* limit = plot->addHorizontalLine(26.0, "Structural limit");
    limit->setColor(QColor("#c0392b"));
    limit->setIncludedInAutoscale(true);  // keep it in view though the data stays below it

    // A stretch of time: shaded from top to bottom, under the data.
    plot->addVerticalSpan(55.0, 85.0, "Max Q");

    // Things that happened: a line each, named on a flag.
    plot->addEvent(0.0, "Liftoff");
    plot->addEvent(150.0, "MECO");
    plot->addEvent(153.0, "Stage separation");
    plot->addEvent(160.0, "Second-stage ignition");

    // A remark about a point of the data, set off from it with an arrow.
    rocketplot::TextAnnotation* note =
        plot->addText(110.0, 16.3, "Steady thrust,<br>lighter vehicle");
    note->setOffset({10.0, 100.0});                       // pixels: right and down
    note->setAlignment(Qt::AlignHCenter | Qt::AlignTop);  // the middle of the text's top goes there
    // [/annotations]
}

void icons(PlotWidget* plot)
{
    const Ascent ascent = sampleAscent();
    plot->setTitle("Ascent acceleration");
    plot->xAxis()->setLabel("Time (s)");
    plot->yAxis()->setLabel("Acceleration (m/s²)");
    rocketplot::LineSeries* acceleration = plot->addLine(ascent.time, ascent.acceleration);
    // [annotations-icons]
    const QIcon warning = plot->style()->standardIcon(QStyle::SP_MessageBoxWarning);
    const QIcon start   = plot->style()->standardIcon(QStyle::SP_MediaPlay);  // or QIcon(":/...")

    // On the curve: valueAt() is the y its line passes through at an x.
    if (const std::optional<double> y = acceleration->valueAt(140.0))
    {
        rocketplot::IconAnnotation* peak = plot->addIcon(140.0, *y, warning);
        peak->setAlignment(Qt::AlignHCenter | Qt::AlignBottom);  // standing on the point
        peak->setToolTip("<b>3.2 g</b> just before cutoff");     // while the pointer is on it
    }

    // On an event's flag, before its name; or alone, without one.
    plot->addEvent(0.0, "Liftoff")->setIcon(start);
    plot->addEvent(150.0, {})->setIcon(warning);
    // [/annotations-icons]
}

void flagsAbove(PlotWidget* plot)
{
    const Ascent ascent = sampleAscent();
    plot->setTitle("Ascent acceleration");
    plot->xAxis()->setLabel("Time (s)");
    plot->yAxis()->setLabel("Acceleration (m/s²)");
    plot->addLine(ascent.time, ascent.acceleration);
    plot->addEvent(0.0, "Liftoff");
    plot->addEvent(150.0, "MECO");
    plot->addEvent(153.0, "Stage separation");
    plot->addEvent(160.0, "Second-stage ignition");
    // [annotations-flags]
    // Flags in a band of their own between the title and the plot area.
    plot->setEventFlagPlacement(rocketplot::FlagPlacement::ABOVE_PLOT_AREA);
    // [/annotations-flags]
}

void options(const QString& /*scratch*/)
{
    PlotWidget  window;
    PlotWidget* plot = &window;
    // [annotations-lines]
    rocketplot::ReferenceLine* target = plot->addHorizontalLine(200.0, "Target orbit");
    target->setLineStyle(Qt::SolidLine);  // dashed by default
    target->setLineWidth(2.0);
    target->setLabelAlignment(Qt::AlignLeft | Qt::AlignBottom);  // left end, below the line
    target->setValue(210.0);                                     // move it

    plot->addVerticalLine(42.0);  // at an x value, without a label
    // [/annotations-lines]

    // [annotations-spans]
    rocketplot::ShadedSpan* coast = plot->addVerticalSpan(150.0, 160.0, "Coast");
    coast->setColor(QColor("#2980b9"));
    coast->setOpacity(0.2);  // of the fill; 0.12 by default

    // Between two y values: an allowed band. An infinite end runs to the plot's edge.
    plot->addHorizontalSpan(-std::numeric_limits<double>::infinity(), 0.0, "Below zero");
    // [/annotations-spans]

    // [annotations-rect]
    // From (x, y) = (0.2, 10) to (0.8, 40), in data coordinates.
    rocketplot::RectAnnotation* domain =
        plot->addRect(QRectF(QPointF(0.2, 10.0), QPointF(0.8, 40.0)), "Search domain");
    domain->setLineStyle(Qt::DashLine);  // solid by default; Qt::NoPen for no outline
    domain->setLineWidth(1.5);           // the theme's annotation line width by default
    domain->setFillOpacity(0.1);         // a fill in its color; 0, none, by default
    // [/annotations-rect]

    // [annotations-text]
    rocketplot::TextAnnotation* peak = plot->addText(78.0, 31.4, "Max Q");
    peak->setOffset({40.0, -30.0});     // 40 px right of the point, 30 px above it
    peak->setArrowVisible(false);       // no arrow back to the point
    peak->setBackgroundVisible(false);  // no box behind the text
    peak->setText("q<sub>max</sub>");   // rich text
    peak->setPosition(79.5, 31.9);      // the point it is about, in data coordinates
    // [/annotations-text]

    // [annotations-pinned]
    rocketplot::TextAnnotation* summary =
        plot->addText(0.02, 0.98, "Apogee 212 km<br>Max Q 34 kPa");
    summary->setCoordinates(rocketplot::AnnotationCoordinates::PLOT_AREA);
    summary->setAlignment(Qt::AlignLeft | Qt::AlignTop);  // its top-left corner near the plot's
    summary->setFrameVisible(true);                       // a hairline box, as around the legend
    summary->setBackgroundColor(QColor("#fff8e1"));       // a box of another color

    // A line at three quarters of the plot's height, whatever the y axis shows.
    plot->addHorizontalLine(0.75)->setCoordinates(rocketplot::AnnotationCoordinates::PLOT_AREA);
    // [/annotations-pinned]

    // [annotations-hover]
    // What the pointer is on: show its details elsewhere (a status bar, a side panel).
    QObject::connect(plot, &rocketplot::PlotWidget::annotationHovered, plot,
                     [](rocketplot::Annotation* annotation) {
                         if (annotation == nullptr)
                         {
                             return;  // the pointer moved off it
                         }
                         // ...
                     });
    // [/annotations-hover]

    // [annotations-flags-options]
    // At the top of the plot area, with autoscaling y axes leaving room for them above the data.
    plot->setEventFlagPlacement(rocketplot::FlagPlacement::HEADROOM);
    // Turned to read upward, in one row: narrow, for events close together.
    plot->setEventFlagOrientation(Qt::Vertical);
    // [/annotations-flags-options]

    // [annotations-common]
    rocketplot::Annotation* any = peak;
    any->setVisible(false);
    any->setColor(Qt::darkGreen);  // of a line, a span's fill, a flag; resetColor() undoes it
    any->setLayer(rocketplot::AnnotationLayer::BELOW_SERIES);  // under the data
    any->setYAxis(plot->yAxis2());                             // its y values are on the right axis
    any->setIncludedInAutoscale(true);                         // autoscale makes room for it

    plot->removeAnnotation(any);  // deletes it
    plot->clearAnnotations();     // all of them
    // [/annotations-common]
}

}  // namespace

void addAnnotationExamples(Examples& examples)
{
    examples.addPlot(QStringLiteral("annotations"), annotated, {720, 440});
    examples.addPlot(QStringLiteral("annotations-icons"), icons);
    examples.addPlot(QStringLiteral("annotations-flags"), flagsAbove);
    examples.demonstrations.push_back(
        {.name = QStringLiteral("annotation options"), .run = options});
}

}  // namespace rocketplot::guide

#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QObject>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStyle>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>
#include <Qt>
#include <optional>

#include "DemoPage.h"
#include "Generators.h"
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

namespace rocketplot::demo
{

namespace
{

QWidget* create(QWidget* parent)
{
    auto* page   = new QWidget(parent);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    // [snippet]
    auto* plot = new rocketplot::PlotWidget(page);
    plot->setTitle("Ascent acceleration");
    plot->xAxis()->setLabel("Time (s)");
    plot->yAxis()->setLabel("Acceleration (m/s²)");
    const Ascent                  ascent       = simulateAscent(300.0, 10.0, 3);
    const rocketplot::LineSeries* acceleration = plot->addLine(ascent.time, ascent.acceleration);

    // Events: a line with a flag. Flags too close to share a row are staggered.
    plot->addEvent(0.0, "Liftoff")->setIcon(plot->style()->standardIcon(QStyle::SP_MediaPlay));
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

    // An icon on the curve, with a tool tip: point at it. valueAt() is where the line is at an x.
    if (const std::optional<double> peak = acceleration->valueAt(148.0))
    {
        auto* warning =
            plot->addIcon(148.0, *peak, plot->style()->standardIcon(QStyle::SP_MessageBoxWarning));
        warning->setAlignment(Qt::AlignHCenter | Qt::AlignBottom);
        warning->setToolTip("<b>Peak acceleration</b><br>just before main engine cutoff");
    }

    // A rectangle at data coordinates: here around the coast between the stages.
    auto* staging = plot->addRect(QRectF(QPointF(145.0, -13.0), QPointF(167.0, -3.0)), "Staging");
    staging->setLineStyle(Qt::DashLine);
    staging->setLabelAlignment(Qt::AlignRight | Qt::AlignTop);

    // Pinned to the plot area: fractions of it, (0, 0) at the bottom left. It stays put as the
    // view is panned and zoomed.
    auto* summary = plot->addText(0.98, 0.86, "Simulated flight, 3 g limit");
    summary->setCoordinates(rocketplot::AnnotationCoordinates::PLOT_AREA);
    summary->setAlignment(Qt::AlignRight | Qt::AlignTop);
    summary->setFrameVisible(true);

    // Where the flags go: over the data (the default), with room left above it, or in a band of
    // their own above the plot area; and turned to read upward.
    auto* placement = new QComboBox(page);
    placement->addItem(QStringLiteral("Over the data"),
                       QVariant::fromValue(rocketplot::FlagPlacement::OVER_DATA));
    placement->addItem(QStringLiteral("Room above the data"),
                       QVariant::fromValue(rocketplot::FlagPlacement::HEADROOM));
    placement->addItem(QStringLiteral("Above the plot area"),
                       QVariant::fromValue(rocketplot::FlagPlacement::ABOVE_PLOT_AREA));
    QObject::connect(placement, &QComboBox::currentIndexChanged, plot, [plot, placement] {
        plot->setEventFlagPlacement(placement->currentData().value<rocketplot::FlagPlacement>());
    });
    auto* upright = new QCheckBox(QStringLiteral("Upright flags"), page);
    QObject::connect(upright, &QCheckBox::toggled, plot, [plot](bool on) {
        plot->setEventFlagOrientation(on ? Qt::Vertical : Qt::Horizontal);
    });
    // [/snippet]

    auto* controls = new QHBoxLayout;
    controls->addWidget(new QLabel(QStringLiteral("Event flags:"), page));
    controls->addWidget(placement);
    controls->addWidget(upright);
    controls->addStretch(1);
    layout->addWidget(plot, 1);
    layout->addLayout(controls);
    return page;
}

}  // namespace

DemoPage annotationsPage()
{
    return {
        .title       = QStringLiteral("Annotations"),
        .description = QStringLiteral(
            "Reference lines, shaded spans, rectangles, text with an arrow, icons and named events "
            "mark up the data and move with it. Zoom in around 155 s: the event flags move onto "
            "one row as they come apart; below, put them above the data or the plot area. Point "
            "at the warning icon for its tool tip. Labels keep to the plot's text colors; the "
            "line or flag beside them carries the color."),
        .sourceFile = QStringLiteral("AnnotationsPage.cpp"),
        .create     = create,
    };
}

}  // namespace rocketplot::demo

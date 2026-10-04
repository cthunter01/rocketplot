#include "Examples.h"

#include <QPointF>
#include <QSize>
#include <QString>
#include <QWidget>
#include <functional>

#include "rocketplot/PlotWidget.h"
#include "rocketplot/enums.h"

namespace rocketplot::guide
{

void Examples::addPlot(const QString& name, const std::function<void(PlotWidget* plot)>& setUp,
                       QSize size, const std::function<QPointF(const PlotWidget& plot)>& pointer)
{
    figures.push_back({
        .name = name,
        .make = [setUp](QWidget* parent) -> QWidget* {
            auto* plot = new PlotWidget(parent);
            // The same in every figure, whatever the colors of the desktop they are drawn on.
            plot->setThemeMode(ThemeMode::LIGHT);
            setUp(plot);
            return plot;
        },
        .size    = size,
        .pointer = pointer,
    });
}

Examples allExamples()
{
    Examples examples;
    addGettingStartedExamples(examples);
    addDataExamples(examples);
    addSeriesExamples(examples);
    addAxesExamples(examples);
    addAnnotationExamples(examples);
    addInteractionExamples(examples);
    addLegendExamples(examples);
    addMultiplePlotExamples(examples);
    addAppearanceExamples(examples);
    addExportExamples(examples);
    addStateExamples(examples);
    addPerformanceExamples(examples);
    return examples;
}

}  // namespace rocketplot::guide

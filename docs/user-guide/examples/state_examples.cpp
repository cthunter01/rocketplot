// Examples of state.md.

#include <QByteArray>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QString>
#include <vector>

#include "Examples.h"
#include "SampleData.h"
#include "rocketplot/Axis.h"
#include "rocketplot/Legend.h"
#include "rocketplot/LineSeries.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/enums.h"

namespace rocketplot::guide
{

namespace
{

void fill(PlotWidget& plot)
{
    const std::vector<double> time = linspace(0.0, 600.0, 601);
    plot.addLine(time, settling(time, 18.0, -160.0, 120.0, 1.2, 1), "LOX feed line");
    plot.addLine(time, settling(time, 20.0, -40.0, 300.0, 0.7, 3), "Tank wall");
}

void keeping(const QString& scratch)
{
    PlotWidget  window;
    PlotWidget* plot = &window;
    fill(window);
    QSettings settings(QDir(scratch).filePath("settings.ini"), QSettings::IniFormat);
    // [state-save]
    // When the window closes: how the user left the plot.
    const QJsonObject state = plot->saveState();
    settings.setValue("plots/chillDown", QJsonDocument(state).toJson(QJsonDocument::Compact));
    // [/state-save]

    // [state-restore]
    // When it opens again, after the plot has its series.
    const QByteArray text = settings.value("plots/chillDown").toByteArray();
    if (!plot->restoreState(QJsonDocument::fromJson(text).object()))
    {
        // Nothing was saved yet, or by a version of the library this one can't read: the plot
        // is as the code set it up.
    }
    // [/state-restore]

    // [state-partial]
    QJsonObject view = plot->saveState();
    view.remove("themeMode");  // the theme stays whatever it is when the state is restored
    view.remove("series");     // and so does how each series is drawn
    plot->restoreState(view);  // the axes, the legend and the crosshair
    // [/state-partial]
}

// The state of a plot whose user zoomed in, hid a series and recolored the other. (Fixed ranges:
// an autoscaled one would hold the last digits of this machine's arithmetic.)
QString savedState()
{
    PlotWidget window;
    fill(window);
    window.xAxis()->setRange(100.0, 400.0);
    window.yAxis()->setRange(-170.0, -40.0);
    window.series().at(0)->setColor(QColor("#c2410c"));
    window.series().at(1)->setVisible(false);
    window.legend()->setAnchor(LegendAnchor::TOP_RIGHT);
    return QString::fromUtf8(QJsonDocument(window.saveState()).toJson(QJsonDocument::Indented))
        .trimmed();
}

}  // namespace

void addStateExamples(Examples& examples)
{
    examples.demonstrations.push_back({.name = QStringLiteral("keeping state"), .run = keeping});
    examples.outputs.push_back({.name = QStringLiteral("state-json"), .text = savedState});
}

}  // namespace rocketplot::guide

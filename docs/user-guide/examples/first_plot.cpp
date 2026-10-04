// The guide's first program, whole: getting-started.md shows this file.
// [first-plot-program]
#include <QApplication>
#include <vector>

#include "rocketplot/Axis.h"
#include "rocketplot/PlotWidget.h"

int main(int argc, char* argv[])
{
    const QApplication app(argc, argv);

    // Some data: any sized range of numbers will do (std::vector, std::array, QList, ...).
    std::vector<double> time;
    std::vector<double> altitude;
    for (int second = 0; second <= 300; ++second)
    {
        time.push_back(second);
        altitude.push_back(0.0006 * second * second);
    }

    rocketplot::PlotWidget plot;
    plot.setTitle("Ascent");
    plot.xAxis()->setLabel("Time (s)");
    plot.yAxis()->setLabel("Altitude (km)");
    plot.addLine(time, altitude);
    plot.resize(720, 400);
    plot.show();

    return QApplication::exec();
}
// [/first-plot-program]

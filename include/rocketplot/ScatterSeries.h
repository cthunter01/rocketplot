#pragma once

#include <QObject>

#include "rocketplot/Series.h"
#include "rocketplot/export.h"

namespace rocketplot
{

class PlotWidget;

/// A series drawn as unconnected markers (circles by default). Created by PlotWidget::addScatter().
class ROCKETPLOT_EXPORT ScatterSeries : public Series
{
    Q_OBJECT

public:
    ~ScatterSeries() override;
    Q_DISABLE_COPY_MOVE(ScatterSeries)

private:
    friend class PlotWidget;
    explicit ScatterSeries(PlotWidget* plot);
};

}  // namespace rocketplot

#pragma once

#include <QImage>
#include <QPoint>
#include <QPointF>
#include <QRect>

#include <gtest/gtest.h>

#include "rocketplot/Axis.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/enums.h"

namespace rocketplot::test
{

/// A plot of a fixed size and theme whose axes run from 0 to 10 whatever it holds, so that tests
/// can compare two renderings of it pixel for pixel: one with what they test, one without.
class RenderedPlotTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_plot.resize(800, 600);
        m_plot.setThemeMode(ThemeMode::LIGHT);
        m_plot.xAxis()->setRange(0.0, 10.0);
        m_plot.yAxis()->setRange(0.0, 10.0);
    }

    [[nodiscard]] QImage render() { return m_plot.grab().toImage(); }

    /// The pixel at a data position, moved by @p offset pixels.
    [[nodiscard]] QPoint pixelAt(double x, double y, QPoint offset = {}) const
    {
        return m_plot.mapFromData(QPointF(x, y)).toPoint() + offset;
    }

    /// Whether any pixel of @p area differs between two renderings.
    [[nodiscard]] static bool differ(const QImage& a, const QImage& b, QRect area)
    {
        area = area.intersected(a.rect());
        for (int y = area.top(); y <= area.bottom(); ++y)
        {
            for (int x = area.left(); x <= area.right(); ++x)
            {
                if (a.pixel(x, y) != b.pixel(x, y))
                {
                    return true;
                }
            }
        }
        return false;
    }

    PlotWidget m_plot;
};

}  // namespace rocketplot::test

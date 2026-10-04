#include "CustomWidget.h"

#include <QColor>
#include <QIcon>
#include <QObject>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QString>
#include <QWidget>
#include <Qt>
#include <utility>

#include "rocketplot/PlotGrid.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/Theme.h"

namespace rocketplot::designer
{

namespace
{

constexpr int    kIconSize  = 32;
constexpr double kLineWidth = 2.0;

// A little plot in @p area: axes on the left and below, and a line of data.
void drawPlot(QPainter& painter, const QRectF& area)
{
    const Theme theme = Theme::light();
    painter.fillRect(area, theme.background);
    painter.setPen(QPen(theme.axisLine, 1.0));
    painter.drawLine(area.bottomLeft(), area.topLeft());
    painter.drawLine(area.bottomLeft(), area.bottomRight());
    const auto point = [&area](double x, double y) {
        return QPointF(area.left() + (x * area.width()), area.bottom() - (y * area.height()));
    };
    painter.setPen(
        QPen(theme.seriesColor(0), kLineWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPolyline(
        QPolygonF{point(0.1, 0.2), point(0.35, 0.6), point(0.6, 0.4), point(0.9, 0.85)});
}

// The widget box's icon: @p rows little plots, one above the other.
QIcon plotIcon(int rows)
{
    QPixmap pixmap(kIconSize, kIconSize);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    const double margin = 2.0;
    const double gap    = 3.0;
    const double height = (kIconSize - (2.0 * margin) - (gap * (rows - 1))) / rows;
    for (int row = 0; row < rows; ++row)
    {
        drawPlot(painter, QRectF(margin + 0.5, margin + (row * (height + gap)) + 0.5,
                                 kIconSize - (2.0 * margin) - 1.0, height - 1.0));
    }
    return {pixmap};
}

QString sizeProperty(int width, int height)
{
    return QStringLiteral(
               "<property name=\"geometry\"><rect><x>0</x><y>0</y><width>%1</width>"
               "<height>%2</height></rect></property>")
        .arg(width)
        .arg(height);
}

}  // namespace

CustomWidget::CustomWidget(WidgetDescription description, QObject* parent)
  : QObject(parent), m_description(std::move(description))
{
}

QString CustomWidget::name() const
{
    return m_description.name;
}

QString CustomWidget::group() const
{
    return QStringLiteral("rocketplot");
}

QString CustomWidget::toolTip() const
{
    return m_description.toolTip;
}

QString CustomWidget::whatsThis() const
{
    return m_description.whatsThis;
}

QString CustomWidget::includeFile() const
{
    return m_description.includeFile;
}

QIcon CustomWidget::icon() const
{
    return m_description.icon();
}

bool CustomWidget::isContainer() const
{
    return false;
}

QWidget* CustomWidget::createWidget(QWidget* parent)
{
    return m_description.create(parent);
}

bool CustomWidget::isInitialized() const
{
    return m_initialized;
}

void CustomWidget::initialize(QDesignerFormEditorInterface* /*core*/)
{
    m_initialized = true;
}

QString CustomWidget::domXml() const
{
    return QStringLiteral(
               "<ui language=\"c++\">"
               "<widget class=\"%1\" name=\"%2\">%3</widget>"
               "<customwidgets><customwidget>"
               "<class>%1</class><extends>QWidget</extends>"
               "</customwidget></customwidgets>"
               "</ui>")
        .arg(m_description.name, m_description.objectName, m_description.properties);
}

WidgetDescription plotWidgetDescription()
{
    return {
        .name        = QStringLiteral("rocketplot::PlotWidget"),
        .includeFile = QStringLiteral("rocketplot/PlotWidget.h"),
        .objectName  = QStringLiteral("plot"),
        .toolTip     = QStringLiteral("A plot of data sets on shared axes, with a legend"),
        .whatsThis   = QStringLiteral(
            "Plots any number of data sets on shared x and y axes. The data, the axes' labels "
            "and scales and the annotations are given in code: plot->addLine(x, y, \"Name\")."),
        .properties = sizeProperty(480, 320),
        .create     = [](QWidget* parent) -> QWidget* { return new PlotWidget(parent); },
        .icon       = [] { return plotIcon(1); },
    };
}

WidgetDescription plotGridDescription()
{
    return {
        .name        = QStringLiteral("rocketplot::PlotGrid"),
        .includeFile = QStringLiteral("rocketplot/PlotGrid.h"),
        .objectName  = QStringLiteral("plotGrid"),
        .toolTip     = QStringLiteral("Plots in rows and columns, lined up and sharing axes"),
        .whatsThis   = QStringLiteral(
            "Arranges plots in rows and columns and lines their plot areas up. The plots of a "
            "column share their x axis. Each plot is filled in code: "
            "plotGrid->plot(row, column)->addLine(x, y)."),
        // Two rows: a grid of one plot looks like a plot.
        .properties = sizeProperty(480, 480) + QStringLiteral("<property name=\"rowCount\">"
                                                              "<number>2</number></property>"),
        .create     = [](QWidget* parent) -> QWidget* { return new PlotGrid(parent); },
        .icon       = [] { return plotIcon(2); },
    };
}

}  // namespace rocketplot::designer

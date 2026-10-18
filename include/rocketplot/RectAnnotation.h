#pragma once

#include <QObject>
#include <QRectF>
#include <QString>
#include <Qt>
#include <optional>

#include "rocketplot/Annotation.h"
#include "rocketplot/Range.h"
#include "rocketplot/export.h"

namespace rocketplot
{

class PlotWidget;

/// A rectangle at data coordinates: the bounds of a search, a region of interest, a tolerance box.
/// Created by PlotWidget::addRect(). Outlined in its color and not filled by default, under the
/// series.
///
/// @code
/// auto* domain = plot->addRect(QRectF(QPointF(0.2, 10.0), QPointF(0.8, 40.0)), "Search domain");
/// domain->setFillOpacity(0.1);   // a faint fill in its color
/// @endcode
class ROCKETPLOT_EXPORT RectAnnotation : public Annotation
{
    Q_OBJECT
    Q_PROPERTY(QRectF rect READ rect WRITE setRect NOTIFY changed)
    Q_PROPERTY(QString label READ label WRITE setLabel NOTIFY changed)
    Q_PROPERTY(
        double lineWidth READ lineWidth WRITE setLineWidth RESET resetLineWidth NOTIFY changed)
    Q_PROPERTY(Qt::PenStyle lineStyle READ lineStyle WRITE setLineStyle NOTIFY changed)
    Q_PROPERTY(double fillOpacity READ fillOpacity WRITE setFillOpacity RESET resetFillOpacity
                   NOTIFY changed)
    Q_PROPERTY(
        Qt::Alignment labelAlignment READ labelAlignment WRITE setLabelAlignment NOTIFY changed)

public:
    ~RectAnnotation() override;
    Q_DISABLE_COPY_MOVE(RectAnnotation)

    /// The rectangle in data coordinates (y on yAxis() unless setYAxis() says otherwise): x from
    /// left() to right(), y from top() to bottom(), whichever way up the y axis runs. Corners
    /// given the other way round are swapped (QRectF::normalized()). A rectangle with a NaN edge
    /// draws nothing.
    [[nodiscard]] QRectF rect() const noexcept { return m_rect; }
    void                 setRect(const QRectF& rect);
    /// Its x and y values.
    [[nodiscard]] Range xRange() const noexcept;
    [[nodiscard]] Range yRange() const noexcept;

    /// Text drawn inside the rectangle (the part of it in view). Math between $ signs and Qt rich
    /// text work too.
    [[nodiscard]] QString label() const { return m_label; }
    void                  setLabel(const QString& label);

    /// Width of the outline in device-independent pixels; by default the theme's annotation line
    /// width.
    [[nodiscard]] double lineWidth() const;
    void                 setLineWidth(double width);
    void                 resetLineWidth();

    /// Qt::SolidLine by default; Qt::NoPen draws no outline (only the fill).
    [[nodiscard]] Qt::PenStyle lineStyle() const noexcept { return m_lineStyle; }
    void                       setLineStyle(Qt::PenStyle style);

    /// How opaque the fill in its color is, from 0 (none, the default) to 1, on top of the
    /// color's own alpha.
    [[nodiscard]] double fillOpacity() const noexcept { return m_fillOpacity; }
    void                 setFillOpacity(double opacity);
    void                 resetFillOpacity() { setFillOpacity(0.0); }

    /// Where in the rectangle the label sits: Qt::AlignLeft | Qt::AlignTop by default.
    [[nodiscard]] Qt::Alignment labelAlignment() const noexcept { return m_labelAlignment; }
    void                        setLabelAlignment(Qt::Alignment alignment);

private:
    friend class PlotWidget;
    RectAnnotation(PlotWidget* plot, const QRectF& rect);

    [[nodiscard]] Range xExtent() const override;
    [[nodiscard]] Range yExtent() const override;

    QRectF                m_rect;
    QString               m_label;
    std::optional<double> m_lineWidth;
    double                m_fillOpacity    = 0.0;
    Qt::PenStyle          m_lineStyle      = Qt::SolidLine;
    Qt::Alignment         m_labelAlignment = Qt::AlignLeft | Qt::AlignTop;
};

}  // namespace rocketplot

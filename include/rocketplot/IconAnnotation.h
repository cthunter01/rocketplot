#pragma once

#include <QIcon>
#include <QObject>
#include <QPointF>
#include <QSizeF>
#include <Qt>
#include <optional>

#include "rocketplot/Annotation.h"
#include "rocketplot/Range.h"
#include "rocketplot/export.h"

namespace rocketplot
{

class PlotWidget;

/// An icon at a point of the data: an event on a curve, a warning, a waypoint. Created by
/// PlotWidget::addIcon(). Drawn over the series at its size whatever the zoom, and sharp at any
/// resolution the icon has images for (in SVG and PDF exports at least four times the size it is
/// drawn at). Give it a tool tip (Annotation::setToolTip()) to say what it marks.
///
/// @code
/// if (const std::optional<double> y = altitude->valueAt(apogee))
/// {
///     auto* icon = plot->addIcon(apogee, *y, QIcon(":/icons/apogee.png"));
///     icon->setToolTip("Apogee: 1 204 m");
/// }
/// @endcode
class ROCKETPLOT_EXPORT IconAnnotation : public Annotation
{
    Q_OBJECT
    Q_PROPERTY(QPointF position READ position WRITE setPosition NOTIFY changed)
    Q_PROPERTY(QIcon icon READ icon WRITE setIcon NOTIFY changed)
    Q_PROPERTY(QSizeF size READ size WRITE setSize RESET resetSize NOTIFY changed)
    Q_PROPERTY(Qt::Alignment alignment READ alignment WRITE setAlignment NOTIFY changed)
    Q_PROPERTY(QPointF offset READ offset WRITE setOffset NOTIFY changed)

public:
    ~IconAnnotation() override;
    Q_DISABLE_COPY_MOVE(IconAnnotation)

    /// The point it marks, in data coordinates (y on yAxis() unless setYAxis() says otherwise).
    [[nodiscard]] QPointF position() const noexcept { return m_position; }
    void                  setPosition(QPointF position);
    void                  setPosition(double x, double y) { setPosition(QPointF(x, y)); }

    /// The icon. A null icon draws nothing.
    [[nodiscard]] QIcon icon() const { return m_icon; }
    void                setIcon(const QIcon& icon);

    /// The size it is drawn at, in device-independent pixels: 16 × 16 by default.
    [[nodiscard]] QSizeF size() const noexcept;
    void                 setSize(QSizeF size);
    void                 resetSize();

    /// Which part of the icon is at the point (moved by offset()): Qt::AlignCenter, the default,
    /// centers it there; Qt::AlignHCenter | Qt::AlignBottom stands it on the point.
    [[nodiscard]] Qt::Alignment alignment() const noexcept { return m_alignment; }
    void                        setAlignment(Qt::Alignment alignment);

    /// How far from the point it is drawn, in device-independent pixels (x to the right, y
    /// downward). (0, 0) by default.
    [[nodiscard]] QPointF offset() const noexcept { return m_offset; }
    void                  setOffset(QPointF offset);

private:
    friend class PlotWidget;
    IconAnnotation(PlotWidget* plot, QPointF position, QIcon icon);

    [[nodiscard]] Range xExtent() const override;
    [[nodiscard]] Range yExtent() const override;

    QPointF               m_position;
    QIcon                 m_icon;
    std::optional<QSizeF> m_size;
    QPointF               m_offset;
    Qt::Alignment         m_alignment = Qt::AlignCenter;
};

}  // namespace rocketplot

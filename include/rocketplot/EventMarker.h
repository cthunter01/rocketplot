#pragma once

#include <QObject>
#include <QString>

#include "rocketplot/Annotation.h"
#include "rocketplot/Range.h"
#include "rocketplot/export.h"

namespace rocketplot
{

class PlotWidget;

/// Something that happened at one x: a vertical line across the plot with its name on a flag at
/// the top. Created by PlotWidget::addEvent(). The flags of events close together are staggered
/// onto rows so they don't cover each other; when there are too many for the rows that fit, the
/// flags without room are left out until the view is zoomed in (their lines stay).
///
/// @code
/// plot->addEvent(150.0, "MECO");
/// plot->addEvent(153.0, "Stage separation");
/// plot->addEvent(160.0, "SES-1");
/// @endcode
class ROCKETPLOT_EXPORT EventMarker : public Annotation
{
    Q_OBJECT
    Q_PROPERTY(double x READ x WRITE setX NOTIFY changed)
    Q_PROPERTY(QString label READ label WRITE setLabel NOTIFY changed)

public:
    ~EventMarker() override;
    Q_DISABLE_COPY_MOVE(EventMarker)

    /// When (or where) it happened: an x value. One the axis can't show draws nothing.
    [[nodiscard]] double x() const noexcept { return m_x; }
    void                 setX(double x);

    /// The name on the flag. Qt rich text works too. Without one there is only the line.
    [[nodiscard]] QString label() const { return m_label; }
    void                  setLabel(const QString& label);

private:
    friend class PlotWidget;
    EventMarker(PlotWidget* plot, double x, QString label);

    [[nodiscard]] Range xExtent() const override;

    double  m_x;
    QString m_label;
};

}  // namespace rocketplot

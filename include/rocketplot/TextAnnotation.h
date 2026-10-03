#pragma once

#include <QColor>
#include <QObject>
#include <QPointF>
#include <QString>
#include <Qt>

#include "rocketplot/Annotation.h"
#include "rocketplot/Range.h"
#include "rocketplot/export.h"

namespace rocketplot
{

class PlotWidget;
struct Theme;

/// Text at a point of the data, optionally set apart from it with an arrow pointing at the point.
/// Created by PlotWidget::addText(). Drawn over the series, in the theme's text color.
///
/// @code
/// auto* maxQ = plot->addText(78.0, 31.4, "Max Q");
/// maxQ->setOffset({40.0, -30.0});   // 40 px right of the point and 30 px above, with an arrow
/// @endcode
class ROCKETPLOT_EXPORT TextAnnotation : public Annotation
{
    Q_OBJECT
    Q_PROPERTY(QPointF position READ position WRITE setPosition NOTIFY changed)
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY changed)
    Q_PROPERTY(QPointF offset READ offset WRITE setOffset NOTIFY changed)
    Q_PROPERTY(Qt::Alignment alignment READ alignment WRITE setAlignment NOTIFY changed)
    Q_PROPERTY(bool arrowVisible READ isArrowVisible WRITE setArrowVisible NOTIFY changed)
    Q_PROPERTY(
        bool backgroundVisible READ isBackgroundVisible WRITE setBackgroundVisible NOTIFY changed)

public:
    ~TextAnnotation() override;
    Q_DISABLE_COPY_MOVE(TextAnnotation)

    /// The point it is about, in data coordinates (y on yAxis() unless setYAxis() says otherwise).
    [[nodiscard]] QPointF position() const noexcept { return m_position; }
    void                  setPosition(QPointF position);
    void                  setPosition(double x, double y) { setPosition(QPointF(x, y)); }

    /// The text. Qt rich text works too: "T<sub>max</sub>".
    [[nodiscard]] QString text() const { return m_text; }
    void                  setText(const QString& text);

    /// How far from the point the text is, in device-independent pixels (x to the right, y
    /// downward): it stays that far away as the view changes. (0, 0), the default, puts the text
    /// at the point.
    [[nodiscard]] QPointF offset() const noexcept { return m_offset; }
    void                  setOffset(QPointF offset);

    /// Which part of the text is at the point (moved by offset()): Qt::AlignCenter, the default,
    /// centers the text there; Qt::AlignLeft | Qt::AlignBottom puts its bottom-left corner there,
    /// so the text is above and right of it.
    [[nodiscard]] Qt::Alignment alignment() const noexcept { return m_alignment; }
    void                        setAlignment(Qt::Alignment alignment);

    /// Whether an arrow is drawn from the text to the point, where the offset leaves room for one.
    /// On by default.
    [[nodiscard]] bool isArrowVisible() const noexcept { return m_arrowVisible; }
    void               setArrowVisible(bool visible);

    /// Whether the text is on a box of the plot's background, which keeps it readable over the
    /// grid and the data. On by default.
    [[nodiscard]] bool isBackgroundVisible() const noexcept { return m_backgroundVisible; }
    void               setBackgroundVisible(bool visible);

private:
    friend class PlotWidget;
    TextAnnotation(PlotWidget* plot, QPointF position, QString text);

    [[nodiscard]] QColor themeColor(const Theme& theme) const override;
    [[nodiscard]] Range  xExtent() const override;
    [[nodiscard]] Range  yExtent() const override;

    QPointF       m_position;
    QString       m_text;
    QPointF       m_offset;
    Qt::Alignment m_alignment         = Qt::AlignCenter;
    bool          m_arrowVisible      = true;
    bool          m_backgroundVisible = true;
};

}  // namespace rocketplot

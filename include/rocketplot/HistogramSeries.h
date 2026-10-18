#pragma once

#include <QObject>
#include <span>
#include <vector>

#include "rocketplot/BarSeries.h"
#include "rocketplot/NumericRange.h"
#include "rocketplot/Range.h"
#include "rocketplot/enums.h"
#include "rocketplot/export.h"

class QJsonObject;

namespace rocketplot
{

class PlotWidget;

/// A histogram: bars counted from values, in equal bins. Created by PlotWidget::addHistogram().
///
/// @code
/// rocketplot::HistogramSeries* landings = plot->addHistogram(missDistances, "Landings");
/// landings->setBinCount(20);
/// landings->setNormalization(rocketplot::HistogramNormalization::PROBABILITY);
/// @endcode
///
/// The bins run from the smallest value to the largest unless setBinRange() says otherwise, and
/// there are as many as the square root of the count of values (5 to 50) unless setBinCount() says.
/// A value at an edge goes into the bin above it, the largest into the last bin; NaN and values
/// outside the range aren't counted. Each change counts the bars again: setBars() throws
/// std::logic_error on a histogram, and errors set on its bars go with them.
class ROCKETPLOT_EXPORT HistogramSeries : public BarSeries
{
    Q_OBJECT
    Q_PROPERTY(int binCount READ binCount WRITE setBinCount RESET resetBinCount NOTIFY changed)
    Q_PROPERTY(rocketplot::HistogramNormalization normalization READ normalization WRITE
                   setNormalization NOTIFY changed)

public:
    ~HistogramSeries() override;
    Q_DISABLE_COPY_MOVE(HistogramSeries)

    /// Copies @p values and counts them.
    template <NumericRange V>
    void setValues(const V& values)
    {
        setValues(detail::toDoubleVector(values));
    }
    void                                  setValues(std::vector<double>&& values);
    [[nodiscard]] std::span<const double> values() const noexcept { return m_values; }

    /// How many bins: 0 (the default) for as many as the square root of the count of values,
    /// within 5 to 50. A count below 0 is ignored with a warning.
    [[nodiscard]] int binCount() const noexcept { return m_binCount; }
    void              setBinCount(int count);
    void              resetBinCount() { setBinCount(0); }

    /// The range the bins cover: from the smallest value to the largest unless set. A range that
    /// can't hold bins (not finite, or empty) is ignored with a warning.
    [[nodiscard]] Range binRange() const noexcept;
    void                setBinRange(Range range);
    void                resetBinRange();
    /// Whether the range is set (rather than following the values).
    [[nodiscard]] bool hasBinRange() const noexcept { return m_binRange.isValid(); }

    [[nodiscard]] HistogramNormalization normalization() const noexcept { return m_normalization; }
    void                                 setNormalization(HistogramNormalization normalization);

protected:
    void               saveState(QJsonObject& state) const override;
    void               restoreState(const QJsonObject& state) override;
    [[nodiscard]] bool takesBars() const noexcept override { return false; }

private:
    friend class PlotWidget;
    explicit HistogramSeries(PlotWidget* plot);

    // Counts the bars again.
    void recount();

    std::vector<double>    m_values;
    int                    m_binCount      = 0;
    Range                  m_binRange      = Range::empty();
    HistogramNormalization m_normalization = HistogramNormalization::COUNT;
};

}  // namespace rocketplot

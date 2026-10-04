// PlotWidget::saveState() and restoreState(): how a plot is set up, as JSON.

#include <QByteArray>
#include <QColor>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLatin1String>
#include <QPointF>
#include <QString>
#include <QTimeZone>
#include <Qt>
#include <array>
#include <vector>

#include <gtest/gtest.h>

#include "rocketplot/Axis.h"
#include "rocketplot/Legend.h"
#include "rocketplot/LineSeries.h"
#include "rocketplot/PlotLink.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/Range.h"
#include "rocketplot/ScatterSeries.h"
#include "rocketplot/Theme.h"
#include "rocketplot/enums.h"

namespace
{

using rocketplot::AutoscaleMode;
using rocketplot::LegendAnchor;
using rocketplot::LineSeries;
using rocketplot::Marker;
using rocketplot::NumberFormat;
using rocketplot::PlotWidget;
using rocketplot::Range;
using rocketplot::ScaleType;
using rocketplot::Theme;
using rocketplot::ThemeMode;

constexpr std::array<double, 4> kX{1.0, 2.0, 3.0, 4.0};
constexpr std::array<double, 4> kY{10.0, 40.0, 20.0, 30.0};

// A state holding only @p settings, for the part of a plot named @p part ("" for the plot itself).
QJsonObject stateOf(const QString& part, const QJsonObject& settings)
{
    QJsonObject state = part.isEmpty() ? settings : QJsonObject{{part, settings}};
    state.insert(QLatin1String("format"), QLatin1String("rocketplot.state"));
    state.insert(QLatin1String("version"), 1);
    return state;
}

// A state holding only @p settings, for one series.
QJsonObject seriesStateOf(const QJsonObject& settings)
{
    QJsonObject state = stateOf(QString(), {});
    state.insert(QLatin1String("series"), QJsonArray{settings});
    return state;
}

class PlotStateTest : public testing::Test
{
protected:
    void SetUp() override
    {
        for (PlotWidget* plot : {&m_saved, &m_restored})
        {
            plot->resize(600, 400);
            plot->setThemeMode(ThemeMode::LIGHT);
        }
    }

    // Takes the state of m_saved to m_restored.
    void carryOver() { ASSERT_TRUE(m_restored.restoreState(m_saved.saveState())); }

    PlotWidget m_saved;
    PlotWidget m_restored;
};

TEST_F(PlotStateTest, TheViewComesBack)
{
    m_saved.addLine(kX, kY, "A");
    m_restored.addLine(kX, kY, "A");
    m_saved.xAxis()->setRange(1.5, 3.25);
    m_saved.yAxis()->setScaleType(ScaleType::LOGARITHMIC);
    m_saved.yAxis()->setRange(5.0, 500.0);

    carryOver();

    EXPECT_EQ(m_restored.xAxis()->range(), (Range{.min = 1.5, .max = 3.25}));
    EXPECT_FALSE(m_restored.xAxis()->autoscale());
    EXPECT_EQ(m_restored.yAxis()->scaleType(), ScaleType::LOGARITHMIC);
    EXPECT_EQ(m_restored.yAxis()->range(), (Range{.min = 5.0, .max = 500.0}));
    EXPECT_TRUE(m_restored.yAxis2()->autoscale());
}

TEST_F(PlotStateTest, AxisSettingsComeBack)
{
    rocketplot::Axis& axis = *m_saved.xAxis();
    axis.setScaleType(ScaleType::DATE_TIME);
    axis.setNumberFormat(NumberFormat::SI);
    axis.setTimeZone(QTimeZone(QByteArrayLiteral("UTC+02:00")));
    axis.setAutoscaleMode(AutoscaleMode::FOLLOW_LATEST);
    axis.setAutoscaleMargin(0.1);
    axis.setFollowWindow(60.0);
    axis.setGridVisible(false);
    axis.setMinorGridVisible(true);
    axis.setVisible(false);

    carryOver();

    const rocketplot::Axis& restored = *m_restored.xAxis();
    EXPECT_EQ(restored.scaleType(), ScaleType::DATE_TIME);
    EXPECT_EQ(restored.numberFormat(), NumberFormat::SI);
    EXPECT_EQ(restored.timeZone(), QTimeZone(QByteArrayLiteral("UTC+02:00")));
    EXPECT_EQ(restored.autoscaleMode(), AutoscaleMode::FOLLOW_LATEST);
    EXPECT_DOUBLE_EQ(restored.autoscaleMargin(), 0.1);
    EXPECT_DOUBLE_EQ(restored.followWindow(), 60.0);
    EXPECT_FALSE(restored.isGridVisible());
    EXPECT_TRUE(restored.isMinorGridVisible());
    EXPECT_FALSE(restored.isVisible());
    EXPECT_TRUE(restored.autoscale());
}

TEST_F(PlotStateTest, AnAutoscalingAxisFitsTheDataThereIsNow)
{
    m_saved.addLine(kX, kY, "A");
    m_restored.addLine(std::vector<double>{100.0, 200.0}, std::vector<double>{-5.0, 5.0}, "A");
    m_restored.xAxis()->setRange(0.0, 1.0);

    carryOver();

    EXPECT_TRUE(m_restored.xAxis()->autoscale());
    EXPECT_LE(m_restored.xAxis()->min(), 100.0);
    EXPECT_GE(m_restored.xAxis()->max(), 200.0);
    EXPECT_GT(m_restored.xAxis()->min(), 50.0);  // not the saved plot's 1 to 4
}

TEST_F(PlotStateTest, ARangeWithoutAWordOnAutoscaleIsShown)
{
    m_restored.addLine(kX, kY, "A");
    const QJsonObject range{{QLatin1String("min"), 2.0}, {QLatin1String("max"), 3.0}};

    ASSERT_TRUE(m_restored.restoreState(stateOf(QLatin1String("xAxis"), range)));

    EXPECT_EQ(m_restored.xAxis()->range(), (Range{.min = 2.0, .max = 3.0}));
    EXPECT_FALSE(m_restored.xAxis()->autoscale());
    EXPECT_TRUE(m_restored.yAxis()->autoscale());  // what the state doesn't hold stays
}

TEST_F(PlotStateTest, PlotSettingsComeBack)
{
    m_saved.setThemeMode(ThemeMode::DARK);
    m_saved.setCrosshairEnabled(true);

    carryOver();

    EXPECT_EQ(m_restored.themeMode(), ThemeMode::DARK);
    EXPECT_EQ(m_restored.theme(), Theme::dark());
    EXPECT_TRUE(m_restored.isCrosshairEnabled());
}

TEST_F(PlotStateTest, ACustomThemeIsNotSaved)
{
    Theme theme      = Theme::light();
    theme.background = QColor(Qt::yellow);
    m_saved.setTheme(theme);
    m_restored.setThemeMode(ThemeMode::PRINT);

    carryOver();

    EXPECT_EQ(m_restored.themeMode(), ThemeMode::PRINT);
}

TEST_F(PlotStateTest, TheLegendComesBackWhereItWasDragged)
{
    m_saved.legend()->setPosition(QPointF(0.25, 0.75));
    m_saved.legend()->setVisible(true);
    m_saved.legend()->setValuesVisible(false);

    carryOver();

    EXPECT_EQ(m_restored.legend()->anchor(), LegendAnchor::CUSTOM);
    EXPECT_EQ(m_restored.legend()->position(), QPointF(0.25, 0.75));
    EXPECT_FALSE(m_restored.legend()->areValuesVisible());
    EXPECT_TRUE(m_restored.legend()->isShownFor(1));  // set visible, not the default
}

TEST_F(PlotStateTest, AnAnchoredLegendStaysAnchored)
{
    m_saved.legend()->setAnchor(LegendAnchor::BOTTOM_LEFT);
    m_restored.legend()->setPosition(QPointF(0.5, 0.5));
    m_restored.legend()->setVisible(true);

    carryOver();

    EXPECT_EQ(m_restored.legend()->anchor(), LegendAnchor::BOTTOM_LEFT);
    EXPECT_FALSE(m_restored.legend()->isShownFor(1));  // back to the default: two or more entries
}

TEST_F(PlotStateTest, SeriesSettingsComeBack)
{
    LineSeries* saved = m_saved.addLine(kX, kY, "A");
    saved->setVisible(false);
    saved->setColor(QColor(10, 20, 30, 128));
    saved->setMarker(Marker::DIAMOND);
    saved->setMarkerSize(11.0);
    saved->setOnSecondaryYAxis(true);
    saved->setLineWidth(3.5);
    saved->setLineStyle(Qt::DashDotLine);
    saved->setErrorStyle(rocketplot::ErrorStyle::BARS);
    saved->setErrorCapSize(9.0);
    saved->setBandOpacity(0.5);
    const LineSeries* restored = m_restored.addLine(kX, kY, "A");

    carryOver();

    EXPECT_FALSE(restored->isVisible());
    EXPECT_EQ(restored->color(), QColor(10, 20, 30, 128));
    EXPECT_EQ(restored->marker(), Marker::DIAMOND);
    EXPECT_DOUBLE_EQ(restored->markerSize(), 11.0);
    EXPECT_TRUE(restored->isOnSecondaryYAxis());
    EXPECT_DOUBLE_EQ(restored->lineWidth(), 3.5);
    EXPECT_EQ(restored->lineStyle(), Qt::DashDotLine);
    EXPECT_EQ(restored->errorStyle(), rocketplot::ErrorStyle::BARS);
    EXPECT_DOUBLE_EQ(restored->errorCapSize(), 9.0);
    EXPECT_DOUBLE_EQ(restored->bandOpacity(), 0.5);
}

TEST_F(PlotStateTest, ASeriesFollowingTheThemeGoesOnFollowingIt)
{
    m_saved.addLine(kX, kY, "A");
    LineSeries* restored = m_restored.addLine(kX, kY, "A");
    restored->setColor(Qt::red);
    restored->setLineWidth(7.0);
    restored->setMarkerSize(20.0);

    carryOver();

    EXPECT_EQ(restored->color(), Theme::light().seriesColor(0));
    EXPECT_DOUBLE_EQ(restored->lineWidth(), Theme::light().lineWidth);
    EXPECT_DOUBLE_EQ(restored->markerSize(), Theme::light().markerSize);
    m_restored.setThemeMode(ThemeMode::DARK);
    EXPECT_EQ(restored->color(), Theme::dark().seriesColor(0));
}

TEST_F(PlotStateTest, SeriesAreMatchedByName)
{
    m_saved.addLine(kX, kY, "A")->setVisible(false);
    m_saved.addLine(kX, kY, "B")->setColor(Qt::red);
    const auto* b     = m_restored.addScatter(kX, kY, "B");  // another order, another kind
    const auto* other = m_restored.addLine(kX, kY, "C");
    const auto* a     = m_restored.addLine(kX, kY, "A");

    carryOver();

    EXPECT_FALSE(a->isVisible());
    EXPECT_EQ(b->color(), QColor(Qt::red));
    EXPECT_TRUE(b->isVisible());
    EXPECT_TRUE(other->isVisible());
    EXPECT_EQ(other->color(), Theme::light().seriesColor(1));
}

TEST_F(PlotStateTest, SeriesSharingANameAreMatchedInOrder)
{
    m_saved.addLine(kX, kY)->setColor(Qt::red);
    m_saved.addLine(kX, kY)->setColor(Qt::blue);
    const auto* first  = m_restored.addLine(kX, kY);
    const auto* second = m_restored.addLine(kX, kY);
    const auto* third  = m_restored.addLine(kX, kY);

    carryOver();

    EXPECT_EQ(first->color(), QColor(Qt::red));
    EXPECT_EQ(second->color(), QColor(Qt::blue));
    EXPECT_EQ(third->color(), Theme::light().seriesColor(2));
}

TEST_F(PlotStateTest, WhatThePlotShowsIsNotPartOfTheState)
{
    m_saved.setTitle("Saved title");
    m_saved.xAxis()->setLabel("Saved label");
    m_saved.addHorizontalLine(3.0, "Saved limit");
    m_restored.setTitle("Title");
    m_restored.xAxis()->setLabel("Label");

    const QJsonObject state = m_saved.saveState();
    ASSERT_TRUE(m_restored.restoreState(state));

    const QByteArray text = QJsonDocument(state).toJson();
    EXPECT_FALSE(text.contains("Saved title"));
    EXPECT_FALSE(text.contains("Saved label"));
    EXPECT_FALSE(text.contains("Saved limit"));
    EXPECT_EQ(m_restored.title(), QString("Title"));
    EXPECT_EQ(m_restored.xAxis()->label(), QString("Label"));
    EXPECT_TRUE(m_restored.annotations().isEmpty());
}

TEST_F(PlotStateTest, SurvivesBeingWrittenAsText)
{
    m_saved.addLine(kX, kY, "A")->setColor(QColor(1, 2, 3));
    m_saved.xAxis()->setRange(0.1 + 0.2, 1.0e9 / 3.0);  // numbers that need every digit
    m_restored.addLine(kX, kY, "A");

    const QByteArray text = QJsonDocument(m_saved.saveState()).toJson();
    ASSERT_TRUE(m_restored.restoreState(QJsonDocument::fromJson(text).object()));

    EXPECT_EQ(m_restored.xAxis()->range(), m_saved.xAxis()->range());
    EXPECT_EQ(m_restored.saveState(), m_saved.saveState());
}

TEST_F(PlotStateTest, RejectsWhatIsNotAState)
{
    QJsonObject newer = m_saved.saveState();
    newer.insert(QLatin1String("version"), 2);
    newer.insert(QLatin1String("themeMode"), QLatin1String("DARK"));
    QJsonObject other = m_saved.saveState();
    other.insert(QLatin1String("format"), QLatin1String("something else"));
    other.insert(QLatin1String("themeMode"), QLatin1String("DARK"));

    EXPECT_FALSE(m_restored.restoreState(QJsonObject()));
    EXPECT_FALSE(m_restored.restoreState(newer));
    EXPECT_FALSE(m_restored.restoreState(other));

    EXPECT_EQ(m_restored.themeMode(), ThemeMode::LIGHT);
}

TEST_F(PlotStateTest, ValuesOfTheWrongKindAreSkipped)
{
    m_restored.addLine(kX, kY, "A");
    const Range       before = m_restored.xAxis()->range();
    const QJsonObject axis{
        {QLatin1String("min"), QLatin1String("left")},
        {QLatin1String("max"), 3.0},
        {QLatin1String("scaleType"), QLatin1String("CUBIC")},
        {QLatin1String("gridVisible"), 1},
        {QLatin1String("followWindow"), -5.0},
        {QLatin1String("timeZone"), QLatin1String("Mars/Olympus_Mons")},
    };
    QJsonObject state = stateOf(QLatin1String("xAxis"), axis);
    state.insert(QLatin1String("themeMode"), 3);
    state.insert(QLatin1String("series"), QLatin1String("all of them"));
    state.insert(QLatin1String("legend"), QJsonArray{1, 2});

    EXPECT_TRUE(m_restored.restoreState(state));

    EXPECT_EQ(m_restored.xAxis()->range(), before);
    EXPECT_EQ(m_restored.xAxis()->scaleType(), ScaleType::LINEAR);
    EXPECT_TRUE(m_restored.xAxis()->isGridVisible());
    EXPECT_DOUBLE_EQ(m_restored.xAxis()->followWindow(), 10.0);
    EXPECT_EQ(m_restored.xAxis()->timeZone(), QTimeZone::utc());
    EXPECT_EQ(m_restored.themeMode(), ThemeMode::LIGHT);
}

TEST_F(PlotStateTest, ARangeTheAxisCannotShowIsSkipped)
{
    m_restored.yAxis()->setScaleType(ScaleType::LOGARITHMIC);
    m_restored.yAxis()->setRange(1.0, 100.0);
    const QJsonObject range{
        {QLatin1String("autoscale"), false},
        {QLatin1String("min"), -10.0},
        {QLatin1String("max"), 10.0},
    };

    EXPECT_TRUE(m_restored.restoreState(stateOf(QLatin1String("yAxis"), range)));

    EXPECT_EQ(m_restored.yAxis()->range(), (Range{.min = 1.0, .max = 100.0}));
}

TEST_F(PlotStateTest, NullPutsASettingBackOnItsDefault)
{
    LineSeries* line = m_restored.addLine(kX, kY, "A");
    line->setColor(Qt::red);
    line->setLineWidth(8.0);
    const QJsonObject settings{
        {QLatin1String("name"), QLatin1String("A")},
        {QLatin1String("color"), QJsonValue(QJsonValue::Null)},
    };

    ASSERT_TRUE(m_restored.restoreState(seriesStateOf(settings)));

    EXPECT_EQ(line->color(), Theme::light().seriesColor(0));
    EXPECT_DOUBLE_EQ(line->lineWidth(), 8.0);  // not in the state: left alone
}

TEST_F(PlotStateTest, LinkedPlotsFollowARestoredRange)
{
    rocketplot::PlotLink link;
    link.addPlot(&m_saved);
    link.addPlot(&m_restored);
    PlotWidget source;
    source.xAxis()->setRange(2.0, 3.0);

    ASSERT_TRUE(m_saved.restoreState(source.saveState()));

    EXPECT_EQ(m_restored.xAxis()->range(), (Range{.min = 2.0, .max = 3.0}));
    EXPECT_FALSE(m_restored.xAxis()->autoscale());
}

TEST_F(PlotStateTest, TheViewHistoryDoesNotRecordARestore)
{
    m_saved.xAxis()->setRange(2.0, 3.0);

    carryOver();

    EXPECT_FALSE(m_restored.canGoBack());
}

}  // namespace

// What PlotWidget exports: images, SVG and PDF drawings, the clipboard and CSV text.

#include <QAction>
#include <QApplication>
#include <QByteArray>
#include <QClipboard>
#include <QColor>
#include <QContextMenuEvent>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIODevice>
#include <QImage>
#include <QMenu>
#include <QPainter>
#include <QPen>
#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QSize>
#include <QSizeF>
#include <QString>
#include <QSvgRenderer>
#include <QTemporaryDir>
#include <QTest>
#include <QWidget>
#include <Qt>
#include <cstdlib>
#include <vector>

#include <gtest/gtest.h>

#include "RenderedPlotTest.h"
#include "rocketplot/Axis.h"
#include "rocketplot/Legend.h"
#include "rocketplot/LineSeries.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/ScatterSeries.h"
#include "rocketplot/Theme.h"

namespace
{

using rocketplot::LineSeries;
using rocketplot::PlotWidget;
using rocketplot::Theme;

// Whether two colors are the same but for antialiasing and the rounding of another renderer.
bool isNearly(const QColor& a, const QColor& b)
{
    constexpr int kTolerance = 12;
    return std::abs(a.red() - b.red()) <= kTolerance &&
           std::abs(a.green() - b.green()) <= kTolerance &&
           std::abs(a.blue() - b.blue()) <= kTolerance;
}

// A broad level line across and beyond the plot, and a second series so that there is a legend.
class ExportTest : public rocketplot::test::RenderedPlotTest
{
protected:
    void SetUp() override
    {
        RenderedPlotTest::SetUp();
        ASSERT_TRUE(m_directory.isValid());
        m_plot.xAxis()->setLabel("Time (s)");
        m_level = m_plot.addLine(std::vector<double>{-5, 2.5, 5, 15},
                                 std::vector<double>{5.5, 5.5, 5.5, 5.5}, "Level");
        m_level->setLineWidth(6.0);
        m_plot.addLine(std::vector<double>{0, 10}, std::vector<double>{1, 2}, "Ramp");
    }

    [[nodiscard]] QString path(const QString& name) const { return m_directory.filePath(name); }

    [[nodiscard]] static QByteArray contentsOf(const QString& fileName)
    {
        QFile file(fileName);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    }

    // How many pixels of a column of @p image the level line covers: how broad it is drawn.
    [[nodiscard]] int rowsOfLevel(const QImage& image, int column) const
    {
        int rows = 0;
        for (int y = 0; y < image.height(); ++y)
        {
            rows += image.pixelColor(column, y) == m_level->color() ? 1 : 0;
        }
        return rows;
    }

    QTemporaryDir m_directory;
    LineSeries*   m_level = nullptr;
};

TEST_F(ExportTest, AnImageLooksLikeTheWidget)
{
    const QImage image = m_plot.renderToImage();
    ASSERT_FALSE(image.isNull());
    EXPECT_EQ(image.size(), m_plot.size());
    EXPECT_EQ(image.pixelColor(2, 2), m_plot.theme().background);
    EXPECT_EQ(image.pixelColor(pixelAt(7.0, 5.5)), m_level->color());
    // The legend is part of it.
    const QImage shown  = render();
    const QRect  legend = m_plot.legendArea().toRect();
    ASSERT_FALSE(legend.isEmpty());
    m_plot.legend()->setVisible(false);
    EXPECT_TRUE(differ(image, m_plot.renderToImage(), legend));
}

TEST_F(ExportTest, AnImageOfAnySizeAndSharpness)
{
    const QImage small = m_plot.renderToImage({.size = {300, 200}, .dpi = 96.0});
    EXPECT_EQ(small.size(), QSize(300, 200));
    EXPECT_DOUBLE_EQ(small.devicePixelRatio(), 1.0);
    EXPECT_NEAR(rowsOfLevel(small, 150), 6, 1);

    // The same plot, twice as sharp: laid out in 300 × 200, drawn on 600 × 400 pixels, where the
    // line is twice as broad.
    const QImage sharp = m_plot.renderToImage({.size = {300, 200}, .dpi = 192.0});
    EXPECT_EQ(sharp.size(), QSize(600, 400));
    EXPECT_DOUBLE_EQ(sharp.devicePixelRatio(), 2.0);
    EXPECT_NEAR(sharp.dotsPerMeterX(), 192.0 / 0.0254, 1.0);  // it knows its resolution
    EXPECT_EQ(sharp.pixelColor(2, 2), m_plot.theme().background);
    EXPECT_NEAR(rowsOfLevel(sharp, 300), 12, 1);
}

TEST_F(ExportTest, DenseDataIsDrawnRightAtAnySharpness)
{
    // 50,000 points along the diagonal: many to a pixel column, so the line is drawn as its
    // outline, column by column. At 120 and 300 dpi the columns are 0.8 and 0.32 pixels wide.
    std::vector<double> ramp;
    for (int i = 0; i <= 50000; ++i)
    {
        ramp.push_back(i / 5000.0);
    }
    LineSeries* diagonal = m_plot.addLine(ramp, ramp, "Diagonal");
    diagonal->setLineWidth(6.0);  // broad enough that the pixel on its middle is all line
    for (const double dpi : {96.0, 120.0, 144.0, 300.0})
    {
        const QImage image  = m_plot.renderToImage({.dpi = dpi});
        int          missed = 0;
        for (int quarter = 2; quarter < 40; ++quarter)
        {
            const double  at    = quarter / 4.0;
            const QPointF pixel = m_plot.mapFromData(QPointF(at, at)) * (dpi / 96.0);
            missed += image.pixelColor(pixel.toPoint()) == diagonal->color() ? 0 : 1;
        }
        EXPECT_EQ(missed, 0) << dpi << " dpi";
    }
}

TEST_F(ExportTest, AnImageTooLargeToMakeIsNull)
{
    EXPECT_TRUE(m_plot.renderToImage({.size = {100000, 100}, .dpi = 96.0}).isNull());
    EXPECT_FALSE(m_plot.exportImage(path("huge.png"), {.size = {100000, 100}, .dpi = 96.0}));
}

TEST_F(ExportTest, WhatFollowsThePointerIsLeftOut)
{
    m_plot.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&m_plot));
    const QImage before = m_plot.renderToImage();
    m_plot.setCrosshairEnabled(true);
    QTest::mouseMove(&m_plot, pixelAt(3.0, 3.0));
    EXPECT_TRUE(m_plot.crosshairPosition().has_value());
    EXPECT_NE(render(), before);  // the widget shows the crosshair
    EXPECT_EQ(m_plot.renderToImage(), before);
}

TEST_F(ExportTest, WithAnotherThemeThanThePlots)
{
    const QSignalSpy changed(&m_plot, &PlotWidget::themeChanged);
    const QImage     dark = m_plot.renderToImage({.theme = Theme::dark()});
    EXPECT_EQ(dark.pixelColor(2, 2), Theme::dark().background);
    EXPECT_EQ(dark.pixelColor(pixelAt(7.0, 5.5)), Theme::dark().seriesColor(0));
    // The plot keeps its own.
    EXPECT_EQ(m_plot.theme(), Theme::light());
    EXPECT_EQ(changed.count(), 0);
    EXPECT_EQ(m_plot.renderToImage().pixelColor(2, 2), Theme::light().background);

    // A background that isn't opaque stays that way.
    Theme clear      = Theme::light();
    clear.background = QColor(0, 0, 0, 0);
    EXPECT_EQ(m_plot.renderToImage({.theme = clear}).pixelColor(2, 2).alpha(), 0);
}

TEST_F(ExportTest, PaintsIntoAPartOfAnotherDrawing)
{
    QImage page(1000, 800, QImage::Format_ARGB32_Premultiplied);
    page.fill(Qt::magenta);
    {
        QPainter painter(&page);
        painter.setPen(QPen(Qt::green, 3.0));
        m_plot.paint(painter, QRectF(100.0, 50.0, 800.0, 600.0));
        EXPECT_EQ(painter.pen().color(), QColor(Qt::green));  // left as it was
    }
    EXPECT_EQ(page.pixelColor(50, 400), QColor(Qt::magenta));
    EXPECT_EQ(page.pixelColor(102, 52), m_plot.theme().background);
    EXPECT_EQ(page.pixelColor(pixelAt(7.0, 5.5) + QPoint(100, 50)), m_level->color());

    // Inside a clip of the caller's, the plot stays inside it.
    page.fill(Qt::magenta);
    {
        QPainter painter(&page);
        painter.setClipRect(QRectF(0.0, 0.0, 500.0, 800.0));
        m_plot.paint(painter, QRectF(100.0, 50.0, 800.0, 600.0));
    }
    EXPECT_EQ(page.pixelColor(pixelAt(3.0, 5.5) + QPoint(100, 50)), m_level->color());
    EXPECT_EQ(page.pixelColor(pixelAt(7.0, 5.5) + QPoint(100, 50)), QColor(Qt::magenta));
}

TEST_F(ExportTest, AnSvgDrawing)
{
    const QString fileName = path("plot.svg");
    m_plot.setTitle("Thrust <i>F</i><sub>vac</sub>");
    ASSERT_TRUE(m_plot.exportSvg(fileName));
    const QByteArray text = contentsOf(fileName);
    EXPECT_TRUE(text.contains("<svg"));
    EXPECT_TRUE(text.contains("<title>Thrust Fvac</title>"));
    EXPECT_TRUE(text.contains("<text"));  // text stays text

    // The series are clipped to the plot area.
    EXPECT_TRUE(text.contains("<clipPath"));
    EXPECT_TRUE(text.contains("clip-path=\"url(#"));

    // Drawn again from the file, it is the plot.
    QSvgRenderer renderer(fileName);
    ASSERT_TRUE(renderer.isValid());
    EXPECT_EQ(renderer.viewBoxF(), QRectF(QPointF(), QSizeF(m_plot.size())));
    QImage image(m_plot.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::magenta);
    {
        QPainter painter(&image);
        renderer.render(&painter);
    }
    EXPECT_TRUE(isNearly(image.pixelColor(2, 2), m_plot.theme().background));
    EXPECT_TRUE(isNearly(image.pixelColor(pixelAt(7.0, 5.5)), m_level->color()));
    // The line runs on far beyond the axes. QtSvg ignores the clip path, but even so the line
    // stops within a few pixels of the plot area's edge.
    EXPECT_TRUE(isNearly(image.pixelColor(pixelAt(0.0, 5.5, QPoint(4, 0))), m_level->color()));
    for (const QPoint beside :
         {pixelAt(0.0, 5.5, QPoint(-14, 0)), pixelAt(10.0, 5.5, QPoint(14, 0))})
    {
        EXPECT_FALSE(isNearly(image.pixelColor(beside), m_level->color()));
    }
}

TEST_F(ExportTest, APdfDocument)
{
    const QString fileName = path("plot.pdf");
    ASSERT_TRUE(m_plot.exportPdf(fileName, {.size = {480, 360}}));
    const QByteArray text = contentsOf(fileName);
    EXPECT_TRUE(text.startsWith("%PDF-"));
    // One page, the size of the plot: 480 × 360 device-independent pixels, in points.
    const QRegularExpression pageBox(QStringLiteral(R"(/MediaBox \[0 0 ([0-9.]+) ([0-9.]+)\])"));
    const QRegularExpressionMatch match = pageBox.match(QString::fromLatin1(text));
    ASSERT_TRUE(match.hasMatch());
    const double pointsPerPixel = 72.0 / m_plot.logicalDpiY();
    EXPECT_NEAR(match.captured(1).toDouble(), 480.0 * pointsPerPixel, 0.5);
    EXPECT_NEAR(match.captured(2).toDouble(), 360.0 * pointsPerPixel, 0.5);
}

TEST_F(ExportTest, TheDataInViewAsCsv)
{
    // Level has points at -5, 2.5, 5 and 15; the x axis shows 0 to 10.
    EXPECT_EQ(m_plot.toCsv(),
              "Time (s),Level,Time (s) (Ramp),Ramp\n"
              "2.5,5.5,0,1\n"
              "5,5.5,10,2\n");
    m_plot.xAxis()->setRange(-10.0, 20.0);
    EXPECT_EQ(m_plot.toCsv(QLatin1Char(';')),
              "Time (s);Level;Time (s) (Ramp);Ramp\n"
              "-5;5.5;0;1\n"
              "2.5;5.5;10;2\n"
              "5;5.5;;\n"
              "15;5.5;;\n");
}

TEST_F(ExportTest, CsvNamesAndWhatIsLeftOut)
{
    m_plot.clearSeries();
    m_plot.xAxis()->setLabel("<i>t</i> (s)");  // rich text: written without its markup
    m_plot.addLine(std::vector<double>{1, 2}, std::vector<double>{3, 4}, "v<sub>z</sub>");
    m_plot.addLine(std::vector<double>{1, 2}, std::vector<double>{5, 6}, "hidden")
        ->setVisible(false);
    auto* points = m_plot.addScatter(std::vector<double>{1, 2}, std::vector<double>{7, 8});
    points->setYErrors(std::vector<double>{0.5, 0.5});
    EXPECT_EQ(m_plot.toCsv(),
              "t (s),vz,Series 3,Series 3 (low),Series 3 (high)\n"
              "1,3,7,6.5,7.5\n"
              "2,4,8,7.5,8.5\n");
    m_plot.clearSeries();
    EXPECT_EQ(m_plot.toCsv(), QString());
}

TEST_F(ExportTest, ACsvFile)
{
    const QString fileName = path("data.csv");
    ASSERT_TRUE(m_plot.exportCsv(fileName));
    EXPECT_EQ(QString::fromUtf8(contentsOf(fileName)), m_plot.toCsv());
}

TEST_F(ExportTest, TheSuffixNamesTheFormat)
{
    ASSERT_TRUE(m_plot.exportTo(path("plot.png")));
    EXPECT_EQ(QImage(path("plot.png")).size(), m_plot.size());
    ASSERT_TRUE(m_plot.exportTo(path("small.JPG"), {.size = {200, 100}, .dpi = 96.0}));
    EXPECT_EQ(QImage(path("small.JPG")).size(), QSize(200, 100));
    ASSERT_TRUE(m_plot.exportTo(path("plot.svg")));
    EXPECT_TRUE(contentsOf(path("plot.svg")).contains("<svg"));
    ASSERT_TRUE(m_plot.exportTo(path("plot.pdf")));
    EXPECT_TRUE(contentsOf(path("plot.pdf")).startsWith("%PDF-"));
    ASSERT_TRUE(m_plot.exportTo(path("data.csv")));
    EXPECT_TRUE(contentsOf(path("data.csv")).startsWith("Time (s),Level"));
    ASSERT_TRUE(m_plot.exportTo(path("data.tsv")));
    EXPECT_TRUE(contentsOf(path("data.tsv")).startsWith("Time (s)\tLevel"));
}

TEST_F(ExportTest, AFileThatCannotBeWritten)
{
    // A format nobody knows, and a directory that isn't there.
    EXPECT_FALSE(m_plot.exportTo(path("plot.nonsense")));
    EXPECT_FALSE(QFileInfo::exists(path("plot.nonsense")));
    for (const char* name : {"plot.png", "plot.svg", "plot.pdf", "data.csv"})
    {
        EXPECT_FALSE(m_plot.exportTo(path(QStringLiteral("missing/") + QLatin1String(name))))
            << name;
    }
}

TEST_F(ExportTest, OntoTheClipboard)
{
    QGuiApplication::clipboard()->clear();
    m_plot.copyToClipboard();
    const QImage image = QGuiApplication::clipboard()->image();
    ASSERT_FALSE(image.isNull());
    EXPECT_EQ(image.size(), m_plot.size());
    EXPECT_EQ(image.pixelColor(pixelAt(7.0, 5.5)), m_level->color());
}

TEST_F(ExportTest, TheContextMenuCopiesAndExports)
{
    m_plot.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&m_plot));
    const QPoint      position = pixelAt(3.0, 3.0);
    QContextMenuEvent event(QContextMenuEvent::Mouse, position, m_plot.mapToGlobal(position));
    QApplication::sendEvent(&m_plot, &event);
    auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
    ASSERT_NE(menu, nullptr);
    QAction* copy     = nullptr;
    bool     exporter = false;
    for (QAction* action : menu->actions())
    {
        copy     = action->text() == "Copy image" ? action : copy;
        exporter = exporter || action->text() == "Export…";
    }
    EXPECT_TRUE(exporter);
    ASSERT_NE(copy, nullptr);
    QGuiApplication::clipboard()->clear();
    copy->trigger();
    EXPECT_EQ(QGuiApplication::clipboard()->image().size(), m_plot.size());
    menu->close();
}

}  // namespace

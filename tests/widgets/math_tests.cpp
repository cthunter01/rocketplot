// Math between $ signs in labels: where it goes, at what size, in which colour.

#include <QColor>
#include <QFont>
#include <QFontDatabase>
#include <QImage>
#include <QMessageLogContext>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QString>
#include <QStringList>
#include <QtLogging>

#include <gtest/gtest.h>

#include "RenderedPlotTest.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/TextAnnotation.h"

namespace
{

using MathTest = rocketplot::test::RenderedPlotTest;

// Collects the warnings Qt and the library log while it lives.
class WarningCatcher
{
public:
    WarningCatcher() : m_previous(install()) { }
    ~WarningCatcher() { qInstallMessageHandler(m_previous); }
    WarningCatcher(const WarningCatcher&)            = delete;
    WarningCatcher& operator=(const WarningCatcher&) = delete;
    WarningCatcher(WarningCatcher&&)                 = delete;
    WarningCatcher& operator=(WarningCatcher&&)      = delete;

    [[nodiscard]] static QStringList& messages()
    {
        static QStringList s_messages;
        return s_messages;
    }

private:
    static QtMessageHandler install()
    {
        messages().clear();
        return qInstallMessageHandler(&WarningCatcher::caught);
    }

    static void caught(QtMsgType type, const QMessageLogContext& /*context*/, const QString& text)
    {
        if (type != QtDebugMsg && type != QtInfoMsg)
        {
            messages().append(text);
        }
    }

    QtMessageHandler m_previous;
};

// Columns of @p ink that no pixel of the difference between @p a and @p b is in, from the left:
// the first gap between two things drawn side by side. -1 if there is none.
int firstGap(const QImage& a, const QImage& b, QRect ink)
{
    for (int x = ink.left(); x <= ink.right(); ++x)
    {
        bool empty = true;
        for (int y = ink.top(); y <= ink.bottom() && empty; ++y)
        {
            empty = a.pixel(x, y) == b.pixel(x, y);
        }
        if (empty)
        {
            return x;
        }
    }
    return -1;
}

class MathLabelTest : public MathTest
{
protected:
    void SetUp() override
    {
        MathTest::SetUp();
        QFont font = m_plot.font();
        font.setPixelSize(40);  // large glyphs, for measuring
        m_plot.setFont(font);
        m_plain = render();
    }

    // The plot with a text annotation of @p text in its middle, without a box around it.
    QImage withText(const QString& text)
    {
        m_note = m_plot.addText(5.0, 5.0, text);
        m_note->setBackgroundVisible(false);
        return render();
    }

    [[nodiscard]] QRect middle() const
    {
        return {pixelAt(5.0, 5.0) - QPoint(250, 120), QSize(500, 240)};
    }

    QImage                      m_plain;
    rocketplot::TextAnnotation* m_note = nullptr;
};

TEST_F(MathLabelTest, AFormulaSitsOnTheBaselineOfTheText)
{
    // The text in Latin Modern Math too, whose upright x sits on the baseline: where a system
    // has no fonts, Qt draws boxes, which end above it (Windows CI's offscreen platform).
    const int id = QFontDatabase::addApplicationFont(ROCKETPLOT_MATH_FONT_FILE);
    ASSERT_GE(id, 0);
    QFont font(QFontDatabase::applicationFontFamilies(id).value(0));
    font.setPixelSize(40);
    m_plot.setFont(font);
    m_plain = render();

    const WarningCatcher warnings;
    const QImage         image = withText("x $x$");  // a text x, then a math x
    const QRect          ink   = inkRect(image, m_plain, middle());
    ASSERT_FALSE(ink.isEmpty());
    const int gap = firstGap(image, m_plain, ink);
    ASSERT_GT(gap, ink.left());
    const QRect text =
        inkRect(image, m_plain, QRect(QPoint(ink.left(), ink.top()), QPoint(gap, ink.bottom())));
    const QRect formula = inkRect(image, m_plain, QRect(QPoint(gap, ink.top()), ink.bottomRight()));
    ASSERT_FALSE(formula.isEmpty());
    EXPECT_NEAR(formula.bottom(), text.bottom(), 1);
    // About as tall as the text's x: math is set at the size of the text around it.
    EXPECT_NEAR(formula.height(), text.height(), text.height() / 4.0);
    EXPECT_TRUE(WarningCatcher::messages().isEmpty())
        << WarningCatcher::messages().join('\n').toStdString();
    m_plot.setFont(QFont());
    QFontDatabase::removeApplicationFont(id);
}

TEST_F(MathLabelTest, AFormulaHasTheColourOfTheTextAroundIt)
{
    const QImage image = withText(R"(<span style="color:#ff0000">$x$</span>)");
    const QRect  ink   = inkRect(image, m_plain, middle());
    ASSERT_FALSE(ink.isEmpty());
    bool red = false;
    for (int y = ink.top(); y <= ink.bottom() && !red; ++y)
    {
        for (int x = ink.left(); x <= ink.right() && !red; ++x)
        {
            const QColor pixel = image.pixelColor(x, y);
            red                = pixel.red() > 200 && pixel.green() < 60 && pixel.blue() < 60;
        }
    }
    EXPECT_TRUE(red);
}

TEST_F(MathLabelTest, AFormulaHasTheSizeOfTheTextAroundIt)
{
    const QRect normal = inkRect(withText("$x$"), m_plain, middle());
    m_plot.removeAnnotation(m_note);
    // Twice the annotations' font: 0.9 of the plot's 40 pixels (Theme::annotationFontScale).
    const QRect large =
        inkRect(withText(R"(<span style="font-size:72px">$x$</span>)"), m_plain, middle());
    ASSERT_FALSE(normal.isEmpty());
    EXPECT_NEAR(large.height(), 2 * normal.height(), 3);
}

TEST_F(MathLabelTest, RichTextAndMathMix)
{
    const WarningCatcher warnings;
    const QImage         both = withText(R"(<b>Drag</b> $F_d$)");
    EXPECT_FALSE(inkRect(both, m_plain, middle()).isEmpty());
    EXPECT_TRUE(WarningCatcher::messages().isEmpty())
        << WarningCatcher::messages().join('\n').toStdString();
}

TEST_F(MathLabelTest, ADollarWithoutAPartnerIsText)
{
    const QImage price = withText("Cost ($)");
    m_plot.removeAnnotation(m_note);
    const QImage escaped = withText(R"(Cost (\$))");
    EXPECT_EQ(price, escaped);  // the backslash isn't drawn
}

TEST_F(MathLabelTest, AFormulaThatCantBeTypesetIsShownAsTyped)
{
    const WarningCatcher warnings;
    const QImage         image = withText(R"(Ratio $\fracc{1}{2}$)");
    EXPECT_FALSE(inkRect(image, m_plain, middle()).isEmpty());
    ASSERT_EQ(WarningCatcher::messages().size(), 1);
    EXPECT_TRUE(WarningCatcher::messages().front().contains("Unknown command \\fracc"));
    // Once: the plot remembers it.
    m_note->setOffset(QPointF(10.0, 10.0));
    (void)render();
    EXPECT_EQ(WarningCatcher::messages().size(), 1);
}

TEST_F(MathLabelTest, AFractionIsTallerThanALetter)
{
    const QRect letter = inkRect(withText("$a$"), m_plain, middle());
    m_plot.removeAnnotation(m_note);
    const QRect fraction = inkRect(withText(R"($\frac{a}{b}$)"), m_plain, middle());
    EXPECT_GT(fraction.height(), letter.height() * 3 / 2);
}

TEST_F(MathLabelTest, ASubscriptGoesBelowTheBaseline)
{
    const QRect x = inkRect(withText("$x$"), m_plain, middle());
    m_plot.removeAnnotation(m_note);
    const QRect subscript = inkRect(withText("$x_i$"), m_plain, middle());
    EXPECT_GT(subscript.bottom(), x.bottom() + 5);
    EXPECT_GT(subscript.width(), x.width());
}

TEST_F(MathLabelTest, ASumInDisplayStyleIsLarger)
{
    const QRect text = inkRect(withText(R"($\sum$)"), m_plain, middle());
    m_plot.removeAnnotation(m_note);
    const QRect display = inkRect(withText(R"($\displaystyle\sum$)"), m_plain, middle());
    EXPECT_GT(display.height(), text.height() * 5 / 4);
}

TEST_F(MathLabelTest, DelimitersGrowWithWhatTheyHold)
{
    const QRect small = inkRect(withText(R"($\left(x\right)$)"), m_plain, middle());
    m_plot.removeAnnotation(m_note);
    const QRect large = inkRect(withText(R"($\left(\dfrac{a}{b}\right)$)"), m_plain, middle());
    EXPECT_GT(large.height(), small.height() * 3 / 2);
}

TEST_F(MathLabelTest, AMatrixHasRows)
{
    // Rows of pixels with ink, in runs: one for each row of the matrix.
    const QImage image = withText(R"($\begin{matrix}x\\x\end{matrix}$)");
    const QRect  ink   = inkRect(image, m_plain, middle());
    int          runs  = 0;
    bool         inRun = false;
    for (int y = ink.top(); y <= ink.bottom(); ++y)
    {
        const bool inked = !inkRect(image, m_plain, QRect(ink.left(), y, ink.width(), 1)).isEmpty();
        runs += inked && !inRun ? 1 : 0;
        inRun = inked;
    }
    EXPECT_EQ(runs, 2);
}

TEST_F(MathLabelTest, TranslucentMathIsBlendedOnce)
{
    // Parts of a tall parenthesis overlap, as do a radical's surd and bar: in a translucent
    // colour, no pixel may be darker than one blend of it (as where two would be drawn).
    const QImage image =
        withText(R"($\left(\begin{matrix}a\\b\\c\\d\end{matrix}\right)\sqrt{\frac{a}{b}}$)");
    m_note->setColor(QColor(0, 0, 0, 128));
    const QImage translucent = render();
    const QRect  ink         = inkRect(image, m_plain, middle());
    ASSERT_FALSE(ink.isEmpty());
    int covered = 0;
    for (int y = ink.top(); y <= ink.bottom(); ++y)
    {
        for (int x = ink.left(); x <= ink.right(); ++x)
        {
            const auto under = static_cast<double>(QColor(m_plain.pixel(x, y)).redF());
            const auto drawn = static_cast<double>(QColor(translucent.pixel(x, y)).redF());
            ASSERT_GE(drawn, (0.5 * under) - 0.03) << x << ", " << y;
            covered += drawn < (0.5 * under) + 0.03 ? 1 : 0;
        }
    }
    EXPECT_GT(covered, 200);  // and much of it is covered fully
}

TEST_F(MathLabelTest, AnInstalledMathFont)
{
    // Latin Modern Math once more, but installed: the font of that family is used, quietly.
    const int id = QFontDatabase::addApplicationFont(ROCKETPLOT_MATH_FONT_FILE);
    ASSERT_GE(id, 0);
    const QString family = QFontDatabase::applicationFontFamilies(id).value(0);
    EXPECT_EQ(family, "Latin Modern Math");
    const QImage bundled = withText("$x^2$");
    {
        const WarningCatcher warnings;
        m_plot.setMathFontFamily(family);
        EXPECT_EQ(render(), bundled);
        EXPECT_TRUE(WarningCatcher::messages().isEmpty())
            << WarningCatcher::messages().join('\n').toStdString();
    }
    m_plot.resetMathFontFamily();
    QFontDatabase::removeApplicationFont(id);
}

TEST_F(MathLabelTest, AFontThatIsntInstalledIsntUsed)
{
    // (A font that is installed but has no MATH table isn't used either; but which fonts have
    // one differs from system to system: DejaVu Sans has one.)
    const QImage bundled = withText("$x^2$");
    m_plot.setMathFontFamily("No Such Math");
    EXPECT_EQ(render(), bundled);
}

TEST_F(MathLabelTest, MathGoesEverywhereTextDoes)
{
    m_plot.setTitle("Speed $v$");
    m_plot.xAxis()->setLabel("Time $t$ (s)");
    m_plot.yAxis()->setLabel("$v$ (m/s)");
    EXPECT_NE(render(), m_plain);
}

}  // namespace

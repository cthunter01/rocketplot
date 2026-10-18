// literalText(): text that a plot shows as it is written.

#include "rocketplot/text.h"

#include <QImage>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <vector>

#include <gtest/gtest.h>

#include "rocketplot/Axis.h"
#include "rocketplot/LineSeries.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/enums.h"

namespace
{

using rocketplot::literalText;
using rocketplot::PlotWidget;

// The header a plot's CSV text gives a series named @p name: its x column is "x", its own column
// the name.
QString csvHeaderOf(const QString& name)
{
    PlotWidget plot;
    plot.resize(400, 300);
    plot.xAxis()->setLabel("x");
    plot.addLine(std::vector<double>{0, 1}, std::vector<double>{0, 1}, name);
    const QString csv = plot.toCsv();
    return csv.left(csv.indexOf(QLatin1String("\n0,")));
}

// The width of the legend of a plot with a series named @p name (and a second one, "a", so that
// there is a legend).
double legendWidthOf(const QString& name)
{
    PlotWidget plot;
    plot.resize(400, 300);
    plot.setThemeMode(rocketplot::ThemeMode::LIGHT);
    plot.addLine(std::vector<double>{0, 1}, std::vector<double>{0, 1}, name);
    plot.addLine(std::vector<double>{0, 1}, std::vector<double>{1, 0}, "a");
    static_cast<void>(plot.grab());  // lays the legend out
    return plot.legendArea().width();
}

TEST(LiteralText, LeavesTextThatIsShownAsWrittenAsItIs)
{
    for (const QString& text : QStringList{"", "Stage 1", "a  b", "  indented", "x < y", "&amp;",
                                           R"(a\b)", "Booster\nsustainer", "50 % (°C)"})
    {
        EXPECT_EQ(literalText(text), text) << text.toStdString();
    }
}

TEST(LiteralText, TheCharactersComeBackInCsvHeaders)
{
    // Each of these would otherwise be markup, math, or lose a character.
    for (const QString& text : QStringList{"<b>$5", "$a$", R"(\$)", "&lt;", "<i>a  b</i>",
                                           "  <b> leading", "From $5 to $10", R"(a\$)"})
    {
        const QString literal = literalText(text);
        EXPECT_NE(literal, text) << text.toStdString();
        EXPECT_FALSE(literal.contains(u'$')) << text.toStdString();
        EXPECT_EQ(csvHeaderOf(literal), "x," + text) << text.toStdString();
    }
    EXPECT_EQ(csvHeaderOf(literalText("<b>\n$5")), "x,\"<b>\n$5\"");  // a line break, quoted
}

TEST(LiteralText, DrawsTheCharactersAsPlainTextWould)
{
    // "<q>$5" is drawn as typed: a single $ is a dollar sign, and q is no HTML tag. literalText()
    // makes rich text of it, which must look the same.
    EXPECT_NEAR(legendWidthOf(literalText("<q>$5")), legendWidthOf("<q>$5"), 1.0);
    EXPECT_NEAR(legendWidthOf(literalText("Cost ($)")), legendWidthOf("Cost ($)"), 1.0);

    // "<b>$5" would be a bold "$5". Shown as typed, it is as wide as the same five characters in
    // an order that isn't markup.
    EXPECT_NEAR(legendWidthOf(literalText("<b>$5")), legendWidthOf("b>$5<"), 1.0);
    EXPECT_GT(legendWidthOf(literalText("<b>$5")), legendWidthOf("<b>$5") + 5.0);

    // Two spaces stay two spaces.
    EXPECT_NEAR(legendWidthOf(literalText("$a  b")), legendWidthOf("$a  b"), 1.0);
    EXPECT_GT(legendWidthOf(literalText("$a  b")), legendWidthOf(literalText("$a b")) + 1.0);
}

}  // namespace

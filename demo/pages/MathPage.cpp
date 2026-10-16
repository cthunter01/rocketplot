#include <QComboBox>
#include <QFont>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QRawFont>
#include <QString>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <vector>

#include "DemoPage.h"
#include "rocketplot/Axis.h"
#include "rocketplot/LineSeries.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/ReferenceLine.h"
#include "rocketplot/TextAnnotation.h"

namespace rocketplot::demo
{

namespace
{

// The installed fonts that math can be set in: those with an OpenType MATH table. (Only the
// families with "Math" in their names are looked at: looking into every font takes long.)
QStringList mathFonts()
{
    QStringList fonts;
    for (const QString& family : QFontDatabase::families())
    {
        if (family.contains(QStringLiteral("Math"), Qt::CaseInsensitive) &&
            !QRawFont::fromFont(QFont(family)).fontTable("MATH").isEmpty())
        {
            fonts.append(family);
        }
    }
    return fonts;
}

// A damped oscillator released from 1: x(t) for damping ratio @p zeta, at 1 Hz.
std::vector<double> oscillation(const std::vector<double>& time, double zeta)
{
    constexpr double    kOmega = 2.0 * std::numbers::pi;
    const double        damped = kOmega * std::sqrt(1.0 - (zeta * zeta));
    std::vector<double> x;
    x.reserve(time.size());
    for (const double t : time)
    {
        x.push_back(std::exp(-zeta * kOmega * t) * std::cos(damped * t));
    }
    return x;
}

QWidget* create(QWidget* parent)
{
    auto* page   = new QWidget(parent);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    std::vector<double> time(601);
    for (std::size_t i = 0; i < time.size(); ++i)
    {
        time[i] = static_cast<double>(i) * 0.005;
    }
    // [snippet]
    // Math between $ signs, wherever text goes: the title, axis labels, legend names, annotations.
    auto* plot = new rocketplot::PlotWidget(page);
    plot->setTitle(
        R"($x(t) = e^{-\zeta\omega_n t}\cos(\omega_d t)$, $\omega_d = \omega_n\sqrt{1 - \zeta^2}$)");
    plot->xAxis()->setLabel("Time $t$ (s)");
    plot->yAxis()->setLabel("Displacement $x/x_0$");
    plot->addLine(time, oscillation(time, 0.05), R"($\zeta = 0.05$)");
    plot->addLine(time, oscillation(time, 0.2), R"($\zeta = 0.2$)");
    plot->addLine(time, oscillation(time, 0.5), R"($\zeta = 0.5$)");
    plot->addHorizontalLine(0.5, R"($\tfrac{1}{2}x_0$)");
    plot->addText(1.0, oscillation({1.0}, 0.05).front(),
                  R"(<b>First period</b>: $T_d = \frac{2\pi}{\omega_d}$)")
        ->setOffset({90.0, -40.0});
    // The font of the formulas: an installed OpenType math font, or Latin Modern Math (empty).
    auto* fonts = new QComboBox(page);
    fonts->addItem(QStringLiteral("Latin Modern Math (built in)"), QString());
    for (const QString& family : mathFonts())
    {
        fonts->addItem(family, family);
    }
    QObject::connect(fonts, &QComboBox::currentIndexChanged, plot,
                     [plot, fonts] { plot->setMathFontFamily(fonts->currentData().toString()); });
    // [/snippet]

    auto* title = new QLineEdit(plot->title(), page);
    QObject::connect(title, &QLineEdit::textChanged, plot, &rocketplot::PlotWidget::setTitle);
    auto* controls = new QHBoxLayout;
    controls->addWidget(new QLabel(QStringLiteral("Title:"), page));
    controls->addWidget(title, 1);
    controls->addWidget(new QLabel(QStringLiteral("Math font:"), page));
    controls->addWidget(fonts);
    layout->addWidget(plot, 1);
    layout->addLayout(controls);
    return page;
}

}  // namespace

DemoPage mathPage()
{
    return {
        .title       = QStringLiteral("Math notation"),
        .description = QStringLiteral(
            "Titles, axis labels, legend names and annotations take math between $ signs, written "
            "as in LaTeX and typeset as TeX does (fractions, roots, scripts, sums, matrices), in "
            "Latin Modern Math, which comes with the library. Edit the title below to try it; math "
            "mixes with Qt rich text."),
        .sourceFile = QStringLiteral("MathPage.cpp"),
        .create     = create,
    };
}

}  // namespace rocketplot::demo

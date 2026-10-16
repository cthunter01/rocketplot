// Examples of math.md.

#include <QFont>
#include <QString>
#include <Qt>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include "Examples.h"
#include "SampleData.h"
#include "rocketplot/Axis.h"
#include "rocketplot/LineSeries.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/ReferenceLine.h"
#include "rocketplot/TextAnnotation.h"

namespace rocketplot::guide
{

namespace
{

// Drag in kN at @p speed (m/s) of a body of 1 m² with drag coefficient @p cd, at sea level.
std::vector<double> drag(const std::vector<double>& speed, double cd)
{
    std::vector<double> force;
    force.reserve(speed.size());
    for (const double v : speed)
    {
        force.push_back(0.5 * 1.225 * v * v * cd / 1000.0);
    }
    return force;
}

void labels(PlotWidget* plot)
{
    const std::vector<double> speed = linspace(0.0, 300.0, 301);
    const std::vector<double> low   = drag(speed, 0.30);
    const std::vector<double> high  = drag(speed, 0.45);
    // [math-labels]
    plot->setTitle(R"(Drag $F_d = \tfrac{1}{2}\rho v^2 C_d A$)");
    plot->xAxis()->setLabel("Speed $v$ (m/s)");
    plot->yAxis()->setLabel("Drag $F_d$ (kN)");
    plot->addLine(speed, low, "$C_d = 0.30$");
    plot->addLine(speed, high, "$C_d = 0.45$");
    // [/math-labels]
}

// Formulas without a plot around them: axes and grid hidden, the text without its box.
void gallery(PlotWidget* plot)
{
    QFont font = plot->font();
    font.setPixelSize(17);
    plot->setFont(font);
    for (Axis* axis : {plot->xAxis(), plot->yAxis()})
    {
        axis->setRange(0.0, 10.0);
        axis->setVisible(false);
        axis->setGridVisible(false);
    }
    const auto formula = [plot](double x, double y, const char* text) {
        TextAnnotation* note = plot->addText(x, y, QString::fromUtf8(text));
        note->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        note->setBackgroundVisible(false);
    };
    formula(0.2, 8.8, R"($\displaystyle\sum_{k=1}^{n} k^2 = \frac{n(n+1)(2n+1)}{6}$)");
    formula(0.2, 6.2, R"($\displaystyle\int_0^\infty e^{-x^2}\,dx = \frac{\sqrt{\pi}}{2}$)");
    formula(
        0.2, 3.7,
        R"($R(\theta) = \begin{pmatrix}\cos\theta & -\sin\theta\\ \sin\theta & \cos\theta\end{pmatrix}$)");
    formula(0.2, 1.3, R"($|x| = \begin{cases}x & x \geq 0\\ -x & \text{otherwise}\end{cases}$)");
    formula(5.6, 8.8, R"($\Delta v = v_e \ln\frac{m_0}{m_f}$)");
    formula(5.6, 6.2, R"($\mathbf{F} = m\,\ddot{\mathbf{r}}$, $\hat{\mathbf{n}}\cdot\nabla\phi$)");
    formula(5.6, 3.7, R"($f'(x) = \lim_{h\to 0}\frac{f(x+h) - f(x)}{h}$)");
    formula(5.6, 1.3, R"($\binom{n}{k}$, $\sqrt[3]{x}$, $x \in \mathbb{R}^n$)");
}

void richText(PlotWidget* plot)
{
    const Ascent        ascent = sampleAscent();
    std::vector<double> pressure;  // dynamic pressure, kPa
    pressure.reserve(ascent.time.size());
    for (std::size_t i = 0; i < ascent.time.size(); ++i)
    {
        const double density = 1.225 * std::exp(-ascent.altitude[i] / 8.5);
        pressure.push_back(0.5 * density * ascent.velocity[i] * ascent.velocity[i] / 1000.0);
    }
    const auto   peak = std::ranges::max_element(pressure);
    const double at   = ascent.time[static_cast<std::size_t>(peak - pressure.begin())];
    plot->addLine(ascent.time, pressure);
    plot->xAxis()->setRange(0.0, 200.0);
    plot->yAxis()->setRange(0.0, 50.0);
    plot->xAxis()->setLabel("Time (s)");
    plot->yAxis()->setLabel("Dynamic pressure $q$ (kPa)");
    // [math-rich-text]
    plot->addText(at, *peak, R"(<b>Max Q</b>: $q = \frac{1}{2}\rho v^2$)")
        ->setOffset({120.0, 60.0});
    plot->addHorizontalLine(40.0, R"(<span style="color:#b03a2e">Limit $q_\max$</span>)");
    // [/math-rich-text]
}

QString csv()
{
    PlotWidget  window;
    PlotWidget* plot = &window;
    plot->xAxis()->setRange(0.0, 1.0);
    // [math-csv]
    plot->xAxis()->setLabel("Time $t$ (s)");
    plot->addLine(std::vector<double>{0.0, 1.0}, std::vector<double>{2.0, 4.5}, "$v_z$");
    plot->addLine(std::vector<double>{0.0, 1.0}, std::vector<double>{0.0, 9.8}, R"($\frac{F}{m}$)");
    const QString text = plot->toCsv();
    // [/math-csv]
    return text;
}

}  // namespace

void addMathExamples(Examples& examples)
{
    examples.addPlot(QStringLiteral("math-labels"), labels);
    examples.addPlot(QStringLiteral("math-gallery"), gallery, {720, 360});
    examples.addPlot(QStringLiteral("math-rich-text"), richText);
    examples.outputs.push_back({.name = QStringLiteral("math-csv"), .text = csv});
}

}  // namespace rocketplot::guide

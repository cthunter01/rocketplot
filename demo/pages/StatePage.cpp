#include <QFontDatabase>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QObject>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>
#include <vector>

#include "DemoPage.h"
#include "Generators.h"
#include "rocketplot/Axis.h"
#include "rocketplot/PlotWidget.h"

namespace rocketplot::demo
{

namespace
{

constexpr int kTextWidth = 340;

QWidget* create(QWidget* parent)
{
    auto* page   = new QWidget(parent);
    auto* layout = new QHBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* plot = new rocketplot::PlotWidget(page);
    plot->setTitle(QStringLiteral("Engine chill-down"));
    plot->xAxis()->setLabel(QStringLiteral("Time (s)"));
    plot->yAxis()->setLabel(QStringLiteral("Temperature (°C)"));
    const std::vector<double> time = linspace(0.0, 600.0, 6001);
    plot->addLine(time, warmUp(time, 18.0, -160.0, 120.0, 1.5, 1), QStringLiteral("LOX feed line"));
    plot->addLine(time, warmUp(time, 21.0, -95.0, 200.0, 1.0, 2),
                  QStringLiteral("Turbopump housing"));
    plot->addLine(time, warmUp(time, 20.0, -40.0, 300.0, 0.8, 3), QStringLiteral("Tank wall"));
    layout->addWidget(plot, 1);

    auto* side    = new QVBoxLayout;
    auto* buttons = new QHBoxLayout;
    auto* save    = new QPushButton(QStringLiteral("Save state"), page);
    auto* restore = new QPushButton(QStringLiteral("Restore state"), page);
    auto* status  = new QLabel(page);
    auto* text    = new QPlainTextEdit(page);
    text->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    text->setLineWrapMode(QPlainTextEdit::NoWrap);
    text->setFixedWidth(kTextWidth);
    status->setWordWrap(true);
    status->setFixedWidth(kTextWidth);
    buttons->addWidget(save);
    buttons->addWidget(restore);
    side->addLayout(buttons);
    side->addWidget(text, 1);
    side->addWidget(status);
    layout->addLayout(side);

    // [snippet]
    // How the plot is set up (not what it shows), as JSON: keep it in a file or in QSettings.
    const auto saveState = [plot, text, status] {
        const QJsonObject state = plot->saveState();
        text->setPlainText(QString::fromUtf8(QJsonDocument(state).toJson()));
        status->setText(QStringLiteral("Saved. Now change the plot, then restore."));
    };
    // And back. The plot keeps its data; what the state says about the view, the legend and
    // each series (found by name) is applied to it.
    const auto restoreState = [plot, text, status] {
        const QJsonDocument document = QJsonDocument::fromJson(text->toPlainText().toUtf8());
        const bool          restored = plot->restoreState(document.object());
        status->setText(restored ? QStringLiteral("Restored.")
                                 : QStringLiteral("That text is not a saved state."));
    };
    // [/snippet]
    QObject::connect(save, &QPushButton::clicked, page, saveState);
    QObject::connect(restore, &QPushButton::clicked, page, restoreState);
    saveState();
    status->setText(QStringLiteral("The state of the plot as it started."));
    return page;
}

}  // namespace

DemoPage statePage()
{
    return {
        .title       = QStringLiteral("Save and restore"),
        .description = QStringLiteral(
            "A plot's state is how it is set up to show its data: the ranges, scales and grids "
            "of its axes, where the legend is, which series are hidden and how each is drawn. "
            "Pan and zoom, hide a series, drag the legend, recolor a line from its legend "
            "entry's menu, then restore the saved state. The text can be edited first: try "
            "\"scaleType\": \"LOGARITHMIC\" for the x axis. An application keeps the state in "
            "its settings to open a plot the way the user left it."),
        .sourceFile = QStringLiteral("StatePage.cpp"),
        .create     = create,
    };
}

}  // namespace rocketplot::demo

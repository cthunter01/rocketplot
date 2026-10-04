#include <QByteArray>
#include <QDate>
#include <QDateTime>
#include <QString>
#include <QTime>
#include <QTimeZone>
#include <QWidget>
#include <Qt>
#include <cstddef>
#include <vector>

#include "DataImportWidget.h"
#include "DemoPage.h"
#include "Generators.h"

namespace rocketplot::demo
{

namespace
{

// A table as a logger would write it: ten minutes of temperatures, a line a second.
QByteArray sampleText()
{
    const std::vector<double> time     = linspace(0.0, 600.0, 601);
    const std::vector<double> feedLine = warmUp(time, 18.0, -160.0, 120.0, 1.5, 1);
    const std::vector<double> housing  = warmUp(time, 21.0, -95.0, 200.0, 1.0, 2);
    const std::vector<double> tankWall = warmUp(time, 20.0, -40.0, 300.0, 0.8, 3);
    const QDateTime           start(QDate(2026, 10, 3), QTime(14, 0), QTimeZone::UTC);
    QByteArray text("Time (UTC),LOX feed line (°C),Turbopump housing (°C),Tank wall (°C)\n");
    for (std::size_t i = 0; i < time.size(); ++i)
    {
        text += start.addSecs(static_cast<qint64>(i)).toString(Qt::ISODate).toLatin1();
        for (const std::vector<double>* column : {&feedLine, &housing, &tankWall})
        {
            text += ',';
            text += QByteArray::number((*column)[i], 'f', 2);
        }
        text += '\n';
    }
    return text;
}

QWidget* create(QWidget* parent)
{
    auto* importer = new DataImportWidget(parent);
    importer->loadText(sampleText(), QStringLiteral("Sample: engine chill-down"));
    return importer;
}

}  // namespace

DemoPage importPage()
{
    return {
        .title       = QStringLiteral("Your data"),
        .description = QStringLiteral(
            "Plot a table of your own: open a CSV file, drop one anywhere on the window, or copy "
            "cells in a spreadsheet and paste them here. Commas, semicolons, tabs and spaces "
            "all separate columns, a first line of names is recognized, and ISO dates and times "
            "become a time axis. Pick the x column and the columns to draw; a million rows are "
            "fine. The page starts with a sample that went through the same reader."),
        .sourceFile = QStringLiteral("DataImportWidget.cpp"),
        .create     = create,
    };
}

}  // namespace rocketplot::demo

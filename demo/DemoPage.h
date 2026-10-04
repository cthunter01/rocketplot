#pragma once

#include <QString>
#include <functional>
#include <vector>

class QWidget;

namespace rocketplot::demo
{

/// One page of the demo gallery. Each page lives in pages/<Name>Page.cpp; the code between its
/// "// [snippet]" and "// [/snippet]" markers is shown in the source pane.
struct DemoPage
{
    QString                                  title;
    QString                                  description;  ///< Rich text shown above the page
    QString                                  sourceFile;   ///< Under :/sources (demo.qrc)
    std::function<QWidget*(QWidget* parent)> create;
};

DemoPage basicLinesPage();
DemoPage scatterPage();
DemoPage pointStylesPage();
DemoPage errorBarsPage();
DemoPage gapsPage();
DemoPage uniformSamplingPage();
DemoPage logScalePage();
DemoPage dateTimePage();
DemoPage twoAxesPage();
DemoPage numberFormatsPage();
DemoPage linkedPlotsPage();
DemoPage gridPage();
DemoPage interactionPage();
DemoPage legendPage();
DemoPage annotationsPage();
DemoPage exportPage();
DemoPage statePage();
DemoPage largeDataPage();
DemoPage liveAppendPage();
DemoPage telemetryPage();
DemoPage importPage();

}  // namespace rocketplot::demo

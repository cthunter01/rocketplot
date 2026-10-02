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
DemoPage gapsPage();
DemoPage uniformSamplingPage();
DemoPage largeDataPage();
DemoPage liveAppendPage();

}  // namespace rocketplot::demo

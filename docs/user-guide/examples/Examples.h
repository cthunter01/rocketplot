#pragma once

#include <QPointF>
#include <QSize>
#include <QString>
#include <functional>
#include <vector>

class QWidget;

namespace rocketplot
{
class PlotWidget;
}  // namespace rocketplot

// The examples of the user guide (docs/user-guide). Each chapter's examples are in one source
// file; the code between a `// [name]` and a `// [/name]` line there is what the guide shows
// under `<!-- example: name -->`. rocketplot_guide_examples keeps the two the same, draws the
// guide's figures and runs every example (see main.cpp).
namespace rocketplot::guide
{

/// A figure of the guide: a widget one of the examples sets up, drawn into images/<name>.png.
struct Figure
{
    QString                                  name;
    std::function<QWidget*(QWidget* parent)> make;
    QSize                                    size{720, 400};
    /// Where the pointer rests, in the coordinates of the figure's first plot once the figure is
    /// laid out: for figures of what follows the pointer (the crosshair, a legend entry's
    /// highlight). Without one the pointer is elsewhere.
    // NOLINTNEXTLINE(readability-redundant-member-init): lets designated initializers leave it out
    std::function<QPointF(const PlotWidget& plot)> pointer = {};
};

/// An example that has no figure: run to see that it works.
struct Demonstration
{
    QString name;
    /// @p scratch is a folder the example may write files to.
    std::function<void(const QString& scratch)> run;
};

/// Text an example produces that the guide shows under `<!-- output: name -->`: the JSON of a
/// saved state, a few lines of CSV.
struct Output
{
    QString                  name;
    std::function<QString()> text;
};

struct Examples
{
    std::vector<Figure>        figures;
    std::vector<Demonstration> demonstrations;
    std::vector<Output>        outputs;

    /// A figure of one plot: @p setUp fills a new plot in the light theme.
    void addPlot(const QString& name, const std::function<void(PlotWidget* plot)>& setUp,
                 QSize                                                 size    = {720, 400},
                 const std::function<QPointF(const PlotWidget& plot)>& pointer = {});
};

/// Every example of the guide.
[[nodiscard]] Examples allExamples();

// One per chapter.
void addGettingStartedExamples(Examples& examples);
void addDataExamples(Examples& examples);
void addSeriesExamples(Examples& examples);
void addAxesExamples(Examples& examples);
void addAnnotationExamples(Examples& examples);
void addInteractionExamples(Examples& examples);
void addLegendExamples(Examples& examples);
void addMultiplePlotExamples(Examples& examples);
void addAppearanceExamples(Examples& examples);
void addExportExamples(Examples& examples);
void addStateExamples(Examples& examples);
void addPerformanceExamples(Examples& examples);

}  // namespace rocketplot::guide

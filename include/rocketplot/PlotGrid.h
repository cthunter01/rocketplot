#pragma once

#include <QList>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QWidget>
#include <memory>
#include <optional>
#include <vector>

#include "rocketplot/ExportOptions.h"
#include "rocketplot/Theme.h"
#include "rocketplot/enums.h"
#include "rocketplot/export.h"

class QEvent;
class QImage;
class QJsonObject;
class QPaintEvent;
class QPainter;
class QResizeEvent;

namespace rocketplot
{

class PlotLink;
class PlotWidget;
class TextPainter;
struct LayoutConstraints;

/// Plots in rows and columns: channels stacked on a shared time axis, or a page of figures.
///
/// @code
/// auto* grid = new rocketplot::PlotGrid(3, 1, parent);   // three plots, one above the other
/// grid->setTitle("Ascent");
/// grid->plot(0)->addLine(time, altitude);
/// grid->plot(1)->addLine(time, velocity);
/// grid->plot(2)->addLine(time, acceleration);
/// grid->plot(2)->xAxis()->setLabel("Time (s)");
/// @endcode
///
/// The grid makes and owns its plots; plot() gives each one, to add data to and set up like any
/// other (but not to delete, or to move to another parent). It lines their plot areas up, row by
/// row and column by column, however much room each plot's labels take. By default the plots of a
/// column share their x axis through a PlotLink (GridLink), with all that comes with one: panning
/// or zooming one moves the others, they share a crosshair and a view history. Only the bottom plot
/// of such a column then writes the values along the x axis, which leaves more room for the data.
///
/// The theme and the crosshair are set for all the plots at once, and the whole grid exports as
/// one image or drawing, as a plot does.
class ROCKETPLOT_EXPORT PlotGrid : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int rowCount READ rowCount WRITE setRowCount NOTIFY gridSizeChanged)
    Q_PROPERTY(int columnCount READ columnCount WRITE setColumnCount NOTIFY gridSizeChanged)
    Q_PROPERTY(rocketplot::GridLink xLink READ xLink WRITE setXLink NOTIFY changed)
    Q_PROPERTY(bool innerTickLabelsVisible READ areInnerTickLabelsVisible WRITE
                   setInnerTickLabelsVisible NOTIFY changed)
    Q_PROPERTY(int spacing READ spacing WRITE setSpacing NOTIFY changed)
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY changed)
    Q_PROPERTY(rocketplot::ThemeMode themeMode READ themeMode WRITE setThemeMode NOTIFY changed)
    Q_PROPERTY(
        bool crosshairEnabled READ isCrosshairEnabled WRITE setCrosshairEnabled NOTIFY changed)
    Q_PROPERTY(rocketplot::CrosshairMode crosshairMode READ crosshairMode WRITE setCrosshairMode
                   NOTIFY changed)

public:
    /// A grid of one plot.
    explicit PlotGrid(QWidget* parent = nullptr);
    PlotGrid(int rows, int columns, QWidget* parent = nullptr);
    ~PlotGrid() override;
    Q_DISABLE_COPY_MOVE(PlotGrid)

    // Plots
    // ----------------------------------------------------------------------------------------------------

    [[nodiscard]] int rowCount() const noexcept { return m_rows; }
    [[nodiscard]] int columnCount() const noexcept { return m_columns; }
    /// Makes the grid @p rows by @p columns (at least 1 by 1). The plots that are in both the old
    /// and the new grid stay as they are; the others are deleted, or made new.
    void setGridSize(int rows, int columns);
    void setRowCount(int rows) { setGridSize(rows, m_columns); }
    void setColumnCount(int columns) { setGridSize(m_rows, columns); }

    /// The plot in @p row and @p column, counted from the top left; null if there is none.
    [[nodiscard]] PlotWidget* plot(int row, int column = 0) const;
    /// Every plot, row by row.
    [[nodiscard]] QList<PlotWidget*> plots() const { return m_plots; }

    /// How much of the height a row gets, and of the width a column, relative to the others
    /// (1 to begin with; at least 1).
    [[nodiscard]] int rowStretch(int row) const;
    void              setRowStretch(int row, int stretch);
    [[nodiscard]] int columnStretch(int column) const;
    void              setColumnStretch(int column, int stretch);

    /// The gap between neighboring plots in device-independent pixels. 0 to begin with: each
    /// plot has margins of its own.
    [[nodiscard]] int spacing() const noexcept { return m_spacing; }
    void              setSpacing(int spacing);

    // Shared axes
    // ---------------------------------------------------------------------------------------------

    /// Which plots share their x axis: those of each column (the default), all of them, or none.
    [[nodiscard]] GridLink xLink() const noexcept { return m_xLink; }
    void                   setXLink(GridLink link);
    /// The link that ties the x axes of @p column's plots (with GridLink::ALL, of all the plots);
    /// null with GridLink::NONE. It belongs to the grid, and is replaced when the grid's size or
    /// xLink() changes.
    [[nodiscard]] PlotLink* link(int column = 0) const;

    /// Whether the plots above the bottom row write the values along a shared x axis too. Off by
    /// default: the bottom plot's are right below. (Sets Axis::setTickLabelsVisible() of the
    /// plots' x axes; without shared x axes every plot writes its own.)
    [[nodiscard]] bool areInnerTickLabelsVisible() const noexcept { return m_innerTickLabels; }
    void               setInnerTickLabelsVisible(bool visible);

    // For all the plots
    // ---------------------------------------------------------------------------------------

    /// A title above the grid. Qt rich text works too.
    [[nodiscard]] QString title() const { return m_title; }
    void                  setTitle(const QString& title);

    /// The theme of every plot, now and when the grid grows (see PlotWidget::setThemeMode()).
    [[nodiscard]] ThemeMode themeMode() const noexcept { return m_themeMode; }
    void                    setThemeMode(ThemeMode mode);
    /// Uses @p theme for every plot (and switches to ThemeMode::CUSTOM).
    void setTheme(const Theme& theme);
    /// The theme in use.
    [[nodiscard]] const Theme& theme() const;

    /// The crosshair of every plot (see PlotWidget::setCrosshairEnabled()).
    [[nodiscard]] bool isCrosshairEnabled() const noexcept { return m_crosshair; }
    void               setCrosshairEnabled(bool enabled);
    /// What the crosshair of every plot follows: the pointer, or the data (see
    /// PlotWidget::setCrosshairMode()).
    [[nodiscard]] CrosshairMode crosshairMode() const noexcept { return m_crosshairMode; }
    void                        setCrosshairMode(CrosshairMode mode);

    /// Turns autoscale back on for every axis of every plot.
    void resetView();

    // Output
    // ---------------------------------------------------------------------------------------------------

    /// The whole grid as an image, each plot as PlotWidget::renderToImage() draws it. Null if it
    /// can't be made (too large).
    [[nodiscard]] QImage renderToImage(const ExportOptions& options = {}) const;
    /// Draws the same into @p rect of @p painter.
    void paint(QPainter& painter, const QRectF& rect) const;
    /// Writes the grid to @p fileName in the format its suffix names: .svg, .pdf, or an image
    /// format Qt can write. False if the file can't be written.
    [[nodiscard]] bool exportTo(const QString& fileName, const ExportOptions& options = {}) const;
    [[nodiscard]] bool exportImage(const QString&       fileName,
                                   const ExportOptions& options = {}) const;
    [[nodiscard]] bool exportSvg(const QString& fileName, const ExportOptions& options = {}) const;
    [[nodiscard]] bool exportPdf(const QString& fileName, const ExportOptions& options = {}) const;
    /// Puts the grid on the clipboard as an image.
    void copyToClipboard(const ExportOptions& options = {}) const;

    // State
    // ----------------------------------------------------------------------------------------------------

    /// The state of every plot (PlotWidget::saveState()), row by row, as one JSON object.
    [[nodiscard]] QJsonObject saveState() const;
    /// Applies a state from saveState(): the first plot's to this grid's first plot, and so on,
    /// as far as both go. Returns false, changing nothing, if @p state isn't one this version
    /// of the library reads.
    bool restoreState(const QJsonObject& state);

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

Q_SIGNALS:
    /// The number of rows or columns changed: plots were made or deleted.
    void gridSizeChanged();
    /// Another property changed.
    void changed();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    friend class PlotWidget;

    // Where everything goes in @p bounds: the title, and each plot's cell (row by row).
    struct Arrangement
    {
        QRectF              title;
        std::vector<QRectF> cells;
    };
    [[nodiscard]] Arrangement arrange(const QRectF& bounds, const Theme& theme) const;
    void                      placePlots();
    PlotWidget*               createPlot();
    void                      relink();
    void                      applyTickLabels();
    // Redraws the plots: one of them changed, and their margins follow each other's.
    void realign();
    // The margins @p plot must leave so that its plot area lines up with those of its row and
    // its column, on screen.
    [[nodiscard]] LayoutConstraints constraintsFor(const PlotWidget* plot) const;
    void                paintGrid(QPainter& painter, const QRectF& bounds, double devicePixelRatio,
                                  const std::optional<Theme>& theme) const;
    [[nodiscard]] QSize exportSize(const ExportOptions& options) const;

    std::unique_ptr<TextPainter> m_text;   // measures and draws the title
    QList<PlotWidget*>           m_plots;  // row by row
    QList<PlotLink*>             m_links;  // per column, or one for all, or none
    std::vector<int>             m_rowStretch;
    std::vector<int>             m_columnStretch;
    QString                      m_title;
    std::optional<Theme>         m_customTheme;
    int                          m_rows            = 0;
    int                          m_columns         = 0;
    int                          m_spacing         = 0;
    GridLink                     m_xLink           = GridLink::COLUMNS;
    ThemeMode                    m_themeMode       = ThemeMode::SYSTEM;
    CrosshairMode                m_crosshairMode   = CrosshairMode::FREE;
    bool                         m_innerTickLabels = false;
    bool                         m_crosshair       = false;
};

}  // namespace rocketplot

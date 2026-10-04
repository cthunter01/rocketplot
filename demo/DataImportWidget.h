#pragma once

#include <QByteArrayView>
#include <QString>
#include <QWidget>
#include <vector>

#include "DelimitedText.h"

class QComboBox;
class QDragEnterEvent;
class QDropEvent;
class QLabel;
class QListWidget;
class QMimeData;

namespace rocketplot
{
class PlotWidget;
class Series;
}  // namespace rocketplot

namespace rocketplot::demo
{

/// Plots a table of numbers from a file or the clipboard: a CSV file opened or dropped on it, or
/// cells copied from a spreadsheet and pasted. One column gives the x values (or the row number
/// does) and the others are a series each, picked from a list.
class DataImportWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DataImportWidget(QWidget* parent = nullptr);

    [[nodiscard]] PlotWidget*      plot() const noexcept { return m_plot; }
    [[nodiscard]] const DataTable& table() const noexcept { return m_table; }
    /// What the status line says about the last thing read.
    [[nodiscard]] QString status() const;

    /// Reads a table and plots it. False, leaving the plot as it is and saying why in the status
    /// line, if there is nothing in it to plot.
    bool loadFile(const QString& path);
    bool loadText(QByteArrayView text, const QString& source);
    /// Reads what was copied or dragged: a file, or a table as text.
    bool                      loadMimeData(const QMimeData* mime, const QString& source);
    [[nodiscard]] static bool canRead(const QMimeData* mime);

    /// The column the x values come from, or -1 for the row number.
    [[nodiscard]] int xColumn() const;
    void              setXColumn(int column);
    /// Whether a column is drawn.
    [[nodiscard]] bool isPlotted(int column) const;
    void               setPlotted(int column, bool plotted);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    void    listColumns();
    void    plotColumns();
    Series* plotColumn(const DataTable::Column& y, int x, bool asPoints);
    void    openFile();

    DataTable            m_table;
    std::vector<Series*> m_series;  // per column; null for the x column and those without numbers
    PlotWidget*          m_plot    = nullptr;
    QComboBox*           m_x       = nullptr;
    QComboBox*           m_style   = nullptr;
    QListWidget*         m_columns = nullptr;
    QLabel*              m_status  = nullptr;
};

}  // namespace rocketplot::demo

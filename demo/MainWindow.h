#pragma once

#include <QList>
#include <QMainWindow>
#include <QString>
#include <vector>

#include "DemoPage.h"
#include "rocketplot/enums.h"

class QAction;
class QComboBox;
class QDockWidget;
class QDragEnterEvent;
class QDropEvent;
class QLabel;
class QListWidget;
class QMimeData;
class QPlainTextEdit;
class QVBoxLayout;

namespace rocketplot
{
class PlotWidget;
}  // namespace rocketplot

namespace rocketplot::demo
{

class PropertyInspector;

/// The demo: a list of gallery pages on the left; the selected page, with its description and
/// source, in the middle; and, when asked for, an inspector of the page's plots on the right. The
/// toolbar's theme and debug overlay settings apply to every plot on the page. A file of data
/// dropped anywhere on the window is plotted on the "Your data" page.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

    [[nodiscard]] int     pageCount() const noexcept { return static_cast<int>(m_pages.size()); }
    [[nodiscard]] QString pageTitle(int index) const;
    [[nodiscard]] int     currentPage() const;
    void                  selectPage(int index);
    /// The page shown, and the plots on it.
    [[nodiscard]] QWidget*           page() const noexcept { return m_page; }
    [[nodiscard]] QList<PlotWidget*> plots() const;
    /// Sets the toolbar's theme choice.
    void selectTheme(ThemeMode mode);

    /// The inspector of the page's plots, and showing or hiding it.
    [[nodiscard]] PropertyInspector* inspector() const noexcept { return m_inspector; }
    void                             setInspectorVisible(bool visible);

    /// Goes to the "Your data" page and plots the table in the file at @p path there; false if
    /// there is none to plot.
    bool openData(const QString& path);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    void showPage(int index);
    void applySettings();
    bool importData(const QMimeData* mime);

    std::vector<DemoPage> m_pages;
    QListWidget*          m_list          = nullptr;
    QLabel*               m_title         = nullptr;
    QLabel*               m_description   = nullptr;
    QVBoxLayout*          m_pageLayout    = nullptr;
    QWidget*              m_page          = nullptr;
    QPlainTextEdit*       m_source        = nullptr;
    QComboBox*            m_theme         = nullptr;
    QAction*              m_overlay       = nullptr;
    PropertyInspector*    m_inspector     = nullptr;
    QDockWidget*          m_inspectorDock = nullptr;
};

}  // namespace rocketplot::demo

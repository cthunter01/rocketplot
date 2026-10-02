#pragma once

#include <QMainWindow>
#include <QString>
#include <vector>

#include "DemoPage.h"
#include "rocketplot/enums.h"

class QAction;
class QComboBox;
class QLabel;
class QListWidget;
class QPlainTextEdit;
class QVBoxLayout;

namespace rocketplot::demo
{

/// The demo: a list of gallery pages on the left; the selected page, with its description and
/// source, on the right. The toolbar's theme and debug overlay settings apply to every plot on the
/// page.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

    [[nodiscard]] int     pageCount() const noexcept { return static_cast<int>(m_pages.size()); }
    [[nodiscard]] QString pageTitle(int index) const;
    void                  selectPage(int index);
    /// Sets the toolbar's theme choice (SYSTEM, LIGHT or DARK).
    void selectTheme(ThemeMode mode);

private:
    void showPage(int index);
    void applySettings();

    std::vector<DemoPage> m_pages;
    QListWidget*          m_list        = nullptr;
    QLabel*               m_title       = nullptr;
    QLabel*               m_description = nullptr;
    QVBoxLayout*          m_pageLayout  = nullptr;
    QWidget*              m_page        = nullptr;
    QPlainTextEdit*       m_source      = nullptr;
    QComboBox*            m_theme       = nullptr;
    QAction*              m_overlay     = nullptr;
};

}  // namespace rocketplot::demo

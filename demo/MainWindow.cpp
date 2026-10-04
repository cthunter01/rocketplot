#include "MainWindow.h"

#include <QAction>
#include <QComboBox>
#include <QDockWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QLabel>
#include <QList>
#include <QListWidget>
#include <QMainWindow>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QString>
#include <QStringList>
#include <QToolBar>
#include <QUrl>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>
#include <Qt>
#include <algorithm>
#include <cstddef>
#include <limits>
#include <utility>

#include "DataImportWidget.h"
#include "DemoPage.h"
#include "PropertyInspector.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/enums.h"

namespace rocketplot::demo
{

namespace
{

constexpr int    kListWidth    = 220;
constexpr int    kSourceHeight = 260;
constexpr double kTitleScale   = 1.4;

// The page's source between its snippet markers, without their common indentation.
QString snippet(const QString& sourceFile)
{
    QFile file(QStringLiteral(":/sources/") + sourceFile);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return QStringLiteral("// %1 is missing from demo.qrc").arg(sourceFile);
    }
    const QStringList lines = QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'));
    QStringList       inside;
    bool              within = false;
    for (const QString& line : lines)
    {
        const QString trimmed = line.trimmed();
        if (trimmed == QLatin1String("// [snippet]"))
        {
            within = true;
        }
        else if (trimmed == QLatin1String("// [/snippet]"))
        {
            within = false;
        }
        else if (within)
        {
            inside.append(line);
        }
    }
    if (inside.isEmpty())
    {
        return lines.join(QLatin1Char('\n'));
    }
    qsizetype indent = std::numeric_limits<qsizetype>::max();
    for (const QString& line : std::as_const(inside))
    {
        if (!line.trimmed().isEmpty())
        {
            qsizetype spaces = 0;
            while (spaces < line.size() && line.at(spaces).isSpace())
            {
                ++spaces;
            }
            indent = std::min(indent, spaces);
        }
    }
    for (QString& line : inside)
    {
        line = line.mid(std::min(indent, line.size()));
    }
    return inside.join(QLatin1Char('\n'));
}

}  // namespace

MainWindow::MainWindow(QWidget* parent)
  : QMainWindow(parent),
    m_pages{
        basicLinesPage(), scatterPage(),         pointStylesPage(), errorBarsPage(),
        gapsPage(),       uniformSamplingPage(), logScalePage(),    dateTimePage(),
        twoAxesPage(),    numberFormatsPage(),   linkedPlotsPage(), interactionPage(),
        legendPage(),     annotationsPage(),     exportPage(),      statePage(),
        largeDataPage(),  liveAppendPage(),      telemetryPage(),   importPage(),
    },
    m_list(new QListWidget(this)),
    m_pageLayout(new QVBoxLayout),
    m_source(new QPlainTextEdit(this)),
    m_inspector(new PropertyInspector(this)),
    m_inspectorDock(new QDockWidget(QStringLiteral("Inspector"), this))
{
    setWindowTitle(QStringLiteral("rocketplot demo"));

    auto* toolBar = addToolBar(QStringLiteral("Settings"));
    toolBar->setMovable(false);
    toolBar->addWidget(new QLabel(QStringLiteral(" Theme: "), toolBar));
    m_theme = new QComboBox(toolBar);
    m_theme->addItem(QStringLiteral("System"), QVariant::fromValue(ThemeMode::SYSTEM));
    m_theme->addItem(QStringLiteral("Light"), QVariant::fromValue(ThemeMode::LIGHT));
    m_theme->addItem(QStringLiteral("Dark"), QVariant::fromValue(ThemeMode::DARK));
    m_theme->addItem(QStringLiteral("High contrast"),
                     QVariant::fromValue(ThemeMode::HIGH_CONTRAST));
    m_theme->addItem(QStringLiteral("Print"), QVariant::fromValue(ThemeMode::PRINT));
    toolBar->addWidget(m_theme);
    toolBar->addSeparator();
    m_overlay = toolBar->addAction(QStringLiteral("Debug overlay"));
    m_overlay->setCheckable(true);
    m_overlay->setToolTip(QStringLiteral("Layout boxes, frame time and points drawn per series"));
    auto* showSource = toolBar->addAction(QStringLiteral("Source"));
    showSource->setCheckable(true);
    showSource->setChecked(true);

    // The inspector: off to begin with, so that the plots have the room.
    m_inspectorDock->setWidget(m_inspector);
    m_inspectorDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::RightDockWidgetArea, m_inspectorDock);
    m_inspectorDock->hide();
    QAction* showInspector = m_inspectorDock->toggleViewAction();
    showInspector->setToolTip(
        QStringLiteral("Every property of the page's plots, their axes, series and annotations"));
    toolBar->addAction(showInspector);
    setAcceptDrops(true);

    for (const DemoPage& page : m_pages)
    {
        m_list->addItem(page.title);
    }
    m_list->setMinimumWidth(kListWidth / 2);

    auto* pageArea  = new QWidget(this);
    auto* layout    = new QVBoxLayout(pageArea);
    m_title         = new QLabel(pageArea);
    QFont titleFont = m_title->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() * kTitleScale);
    titleFont.setWeight(QFont::DemiBold);
    m_title->setFont(titleFont);
    m_description = new QLabel(pageArea);
    m_description->setWordWrap(true);
    m_description->setTextFormat(Qt::RichText);
    layout->addWidget(m_title);
    layout->addWidget(m_description);
    layout->addLayout(m_pageLayout, 1);

    m_source->setReadOnly(true);
    m_source->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_source->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    auto* right = new QSplitter(Qt::Vertical, this);
    right->addWidget(pageArea);
    right->addWidget(m_source);
    right->setStretchFactor(0, 1);
    right->setSizes({1, kSourceHeight});
    auto* main = new QSplitter(Qt::Horizontal, this);
    main->addWidget(m_list);
    main->addWidget(right);
    main->setStretchFactor(1, 1);
    main->setSizes({kListWidth, 1});
    setCentralWidget(main);

    connect(m_list, &QListWidget::currentRowChanged, this, &MainWindow::showPage);
    connect(m_theme, &QComboBox::currentIndexChanged, this, &MainWindow::applySettings);
    connect(m_overlay, &QAction::toggled, this, &MainWindow::applySettings);
    connect(showSource, &QAction::toggled, m_source, &QWidget::setVisible);
    m_list->setCurrentRow(0);
}

QString MainWindow::pageTitle(int index) const
{
    return index >= 0 && index < pageCount() ? m_pages[static_cast<std::size_t>(index)].title
                                             : QString();
}

int MainWindow::currentPage() const
{
    return m_list->currentRow();
}

void MainWindow::selectPage(int index)
{
    m_list->setCurrentRow(index);
}

QList<PlotWidget*> MainWindow::plots() const
{
    if (m_page == nullptr)
    {
        return {};
    }
    QList<PlotWidget*> plots = m_page->findChildren<PlotWidget*>();
    if (auto* plot = qobject_cast<PlotWidget*>(m_page))
    {
        plots.prepend(plot);
    }
    return plots;
}

void MainWindow::setInspectorVisible(bool visible)
{
    m_inspectorDock->setVisible(visible);
}

bool MainWindow::importData(const QMimeData* mime)
{
    const auto page = std::ranges::find(m_pages, importPage().title, &DemoPage::title);
    if (page == m_pages.end())
    {
        return false;
    }
    selectPage(static_cast<int>(page - m_pages.begin()));  // stays as it is if already shown
    auto* importer = qobject_cast<DataImportWidget*>(m_page);
    return importer != nullptr && importer->loadMimeData(mime, QStringLiteral("Dropped text"));
}

bool MainWindow::openData(const QString& path)
{
    QMimeData mime;
    mime.setUrls({QUrl::fromLocalFile(path)});
    return importData(&mime);
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (DataImportWidget::canRead(event->mimeData()))
    {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent* event)
{
    if (importData(event->mimeData()))
    {
        event->acceptProposedAction();
    }
}

void MainWindow::selectTheme(ThemeMode mode)
{
    m_theme->setCurrentIndex(m_theme->findData(QVariant::fromValue(mode)));
}

void MainWindow::showPage(int index)
{
    if (index < 0 || static_cast<std::size_t>(index) >= m_pages.size())
    {
        return;
    }
    const DemoPage& page = m_pages[static_cast<std::size_t>(index)];
    delete m_page;  // a fresh page every time: no state carries over
    m_page = page.create(this);
    m_pageLayout->addWidget(m_page);
    m_title->setText(page.title);
    m_description->setText(page.description);
    m_source->setPlainText(snippet(page.sourceFile));
    applySettings();
    m_inspector->setPlots(plots());
}

void MainWindow::applySettings()
{
    if (m_page == nullptr)
    {
        return;
    }
    const auto               mode  = m_theme->currentData().value<ThemeMode>();
    const QList<PlotWidget*> shown = plots();
    for (PlotWidget* plot : shown)
    {
        plot->setThemeMode(mode);
        // The large data page turns its overlay on by itself; the toolbar can only add it
        // elsewhere.
        if (!plot->property("keepOverlay").toBool())
        {
            plot->setDebugOverlay(m_overlay->isChecked());
        }
    }
}

}  // namespace rocketplot::demo

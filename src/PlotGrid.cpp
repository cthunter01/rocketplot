#include "rocketplot/PlotGrid.h"

#include <QClipboard>
#include <QEvent>
#include <QFileInfo>
#include <QFont>
#include <QGuiApplication>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QLatin1String>
#include <QList>
#include <QMarginsF>
#include <QPaintDevice>
#include <QPaintEvent>
#include <QPainter>
#include <QRectF>
#include <QResizeEvent>
#include <QSize>
#include <QString>
#include <QWidget>
#include <Qt>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <numeric>
#include <optional>
#include <utility>
#include <vector>

#include "Logging.h"
#include "PlotExport.h"
#include "PlotLayout.h"
#include "PlotState.h"
#include "TextPainter.h"
#include "rocketplot/Axis.h"
#include "rocketplot/ExportOptions.h"
#include "rocketplot/PlotLink.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/Theme.h"
#include "rocketplot/enums.h"

namespace rocketplot
{

namespace
{

constexpr int    kMaxCount     = 32;    // rows or columns: more plots than that can't be read
constexpr double kTitlePadding = 10.0;  // above the title, like a plot's outer padding
constexpr double kTitleScale   = 1.15;  // the grid's title, relative to a plot's
constexpr double kBaseDpi      = 96.0;  // one image pixel per device-independent pixel
constexpr QSize  kPlotSizeHint{480, 240};
constexpr QSize  kPlotMinimumSize{160, 100};

QFont titleFont(const QFont& base, const Theme& theme)
{
    QFont font = scaledFont(base, theme.titleFontScale * kTitleScale);
    font.setWeight(QFont::DemiBold);
    return font;
}

// Where each of the parts that share @p length by their @p stretch starts and ends, with
// @p spacing between neighbors: on whole pixels, so that widgets can be put there.
std::vector<std::pair<double, double>> divide(double start, double length, double spacing,
                                              const std::vector<int>& stretch)
{
    const double total = std::accumulate(stretch.begin(), stretch.end(), 0.0);
    const double usable =
        std::max(0.0, length - (spacing * static_cast<double>(stretch.size() - 1)));
    std::vector<std::pair<double, double>> parts;
    parts.reserve(stretch.size());
    double before = 0.0;  // the stretch of the parts so far
    for (std::size_t i = 0; i < stretch.size(); ++i)
    {
        const double gaps = spacing * static_cast<double>(i);
        const double from = start + gaps + std::round(usable * before / total);
        before += stretch[i];
        const double to = start + gaps + std::round(usable * before / total);
        parts.emplace_back(from, to);
    }
    return parts;
}

}  // namespace

PlotGrid::PlotGrid(QWidget* parent) : PlotGrid(1, 1, parent) { }

PlotGrid::PlotGrid(int rows, int columns, QWidget* parent)
  : QWidget(parent), m_text(std::make_unique<TextPainter>())
{
    setAttribute(Qt::WA_OpaquePaintEvent);  // paintEvent fills what the plots don't
    setGridSize(rows, columns);
}

PlotGrid::~PlotGrid()
{
    // The plots outlive this destructor's body (QWidget deletes them): they mustn't ask a grid
    // that is half gone for their margins.
    for (PlotWidget* plot : std::as_const(m_plots))
    {
        plot->setGrid(nullptr);
    }
}

// Plots
// ---------------------------------------------------------------------------------------------------------

void PlotGrid::setGridSize(int rows, int columns)
{
    rows    = std::clamp(rows, 1, kMaxCount);
    columns = std::clamp(columns, 1, kMaxCount);
    if (rows == m_rows && columns == m_columns)
    {
        return;
    }
    QList<PlotWidget*> kept;
    kept.reserve(static_cast<qsizetype>(rows) * columns);
    for (int row = 0; row < rows; ++row)
    {
        for (int column = 0; column < columns; ++column)
        {
            PlotWidget* existing = plot(row, column);
            kept.append(existing != nullptr ? existing : createPlot());
        }
    }
    for (PlotWidget* old : std::as_const(m_plots))
    {
        if (!kept.contains(old))
        {
            delete old;  // its link lets go of it
        }
    }
    m_plots   = std::move(kept);
    m_rows    = rows;
    m_columns = columns;
    m_rowStretch.resize(static_cast<std::size_t>(rows), 1);
    m_columnStretch.resize(static_cast<std::size_t>(columns), 1);
    relink();
    applyTickLabels();
    placePlots();
    updateGeometry();
    Q_EMIT gridSizeChanged();
}

PlotWidget* PlotGrid::createPlot()
{
    auto* plot = new PlotWidget(this);
    if (m_customTheme)
    {
        plot->setTheme(*m_customTheme);
    }
    else
    {
        plot->setThemeMode(m_themeMode);
    }
    plot->setCrosshairEnabled(m_crosshair);
    plot->setGrid(this);
    // The grid's own background and title are drawn in the plots' theme.
    connect(plot, &PlotWidget::themeChanged, this, [this] {
        placePlots();
        update();
    });
    plot->show();
    return plot;
}

PlotWidget* PlotGrid::plot(int row, int column) const
{
    if (row < 0 || row >= m_rows || column < 0 || column >= m_columns)
    {
        return nullptr;
    }
    return m_plots.at((static_cast<qsizetype>(row) * m_columns) + column);
}

int PlotGrid::rowStretch(int row) const
{
    return row >= 0 && row < m_rows ? m_rowStretch[static_cast<std::size_t>(row)] : 0;
}

void PlotGrid::setRowStretch(int row, int stretch)
{
    if (row < 0 || row >= m_rows || std::max(stretch, 1) == rowStretch(row))
    {
        return;
    }
    m_rowStretch[static_cast<std::size_t>(row)] = std::max(stretch, 1);
    placePlots();
    Q_EMIT changed();
}

int PlotGrid::columnStretch(int column) const
{
    return column >= 0 && column < m_columns ? m_columnStretch[static_cast<std::size_t>(column)]
                                             : 0;
}

void PlotGrid::setColumnStretch(int column, int stretch)
{
    if (column < 0 || column >= m_columns || std::max(stretch, 1) == columnStretch(column))
    {
        return;
    }
    m_columnStretch[static_cast<std::size_t>(column)] = std::max(stretch, 1);
    placePlots();
    Q_EMIT changed();
}

void PlotGrid::setSpacing(int spacing)
{
    spacing = std::max(spacing, 0);
    if (spacing == m_spacing)
    {
        return;
    }
    m_spacing = spacing;
    placePlots();
    update();
    Q_EMIT changed();
}

// Shared axes
// --------------------------------------------------------------------------------------------------

void PlotGrid::setXLink(GridLink link)
{
    if (link == m_xLink)
    {
        return;
    }
    m_xLink = link;
    relink();
    applyTickLabels();
    Q_EMIT changed();
}

PlotLink* PlotGrid::link(int column) const
{
    if (column < 0 || column >= m_columns || m_links.isEmpty())
    {
        return nullptr;
    }
    return m_xLink == GridLink::ALL ? m_links.front() : m_links.at(column);
}

void PlotGrid::relink()
{
    qDeleteAll(m_links);  // each lets go of its plots
    m_links.clear();
    if (m_xLink == GridLink::NONE)
    {
        return;
    }
    const int count = m_xLink == GridLink::ALL ? 1 : m_columns;
    for (int i = 0; i < count; ++i)
    {
        auto* shared = new PlotLink(this);
        shared->setAlignMargins(false);  // the grid lines the plots up, on every side
        m_links.append(shared);
    }
    for (int row = 0; row < m_rows; ++row)
    {
        for (int column = 0; column < m_columns; ++column)
        {
            link(column)->addPlot(plot(row, column));
        }
    }
}

void PlotGrid::setInnerTickLabelsVisible(bool visible)
{
    if (visible == m_innerTickLabels)
    {
        return;
    }
    m_innerTickLabels = visible;
    applyTickLabels();
    Q_EMIT changed();
}

void PlotGrid::applyTickLabels()
{
    for (int row = 0; row < m_rows; ++row)
    {
        const bool labeled = m_xLink == GridLink::NONE || m_innerTickLabels || row == m_rows - 1;
        for (int column = 0; column < m_columns; ++column)
        {
            plot(row, column)->xAxis()->setTickLabelsVisible(labeled);
        }
    }
}

// For all the plots
// --------------------------------------------------------------------------------------------

void PlotGrid::setTitle(const QString& title)
{
    if (title == m_title)
    {
        return;
    }
    m_title = title;
    placePlots();
    update();
    Q_EMIT changed();
}

void PlotGrid::setThemeMode(ThemeMode mode)
{
    if (mode == m_themeMode)
    {
        return;
    }
    m_themeMode = mode;
    if (mode != ThemeMode::CUSTOM)
    {
        m_customTheme.reset();
    }
    for (PlotWidget* plot : std::as_const(m_plots))
    {
        plot->setThemeMode(mode);
    }
    update();
    Q_EMIT changed();
}

void PlotGrid::setTheme(const Theme& theme)
{
    m_themeMode   = ThemeMode::CUSTOM;
    m_customTheme = theme;
    for (PlotWidget* plot : std::as_const(m_plots))
    {
        plot->setTheme(theme);
    }
    update();
    Q_EMIT changed();
}

const Theme& PlotGrid::theme() const
{
    return m_plots.front()->theme();
}

void PlotGrid::setCrosshairEnabled(bool enabled)
{
    if (enabled == m_crosshair)
    {
        return;
    }
    m_crosshair = enabled;
    for (PlotWidget* plot : std::as_const(m_plots))
    {
        plot->setCrosshairEnabled(enabled);
    }
    Q_EMIT changed();
}

void PlotGrid::resetView()
{
    for (PlotWidget* plot : std::as_const(m_plots))
    {
        plot->resetView();
    }
}

// Layout
// -------------------------------------------------------------------------------------------------------

PlotGrid::Arrangement PlotGrid::arrange(const QRectF& bounds, const Theme& theme) const
{
    Arrangement arrangement;
    double      top = bounds.top();
    if (!m_title.isEmpty())
    {
        const double height = m_text->size(m_title, titleFont(font(), theme)).height();
        arrangement.title   = QRectF(bounds.left(), top + kTitlePadding, bounds.width(), height);
        top                 = std::ceil(arrangement.title.bottom());
    }
    const auto rows    = divide(top, bounds.bottom() - top, m_spacing, m_rowStretch);
    const auto columns = divide(bounds.left(), bounds.width(), m_spacing, m_columnStretch);
    arrangement.cells.reserve(rows.size() * columns.size());
    for (const auto& [rowTop, rowBottom] : rows)
    {
        for (const auto& [columnLeft, columnRight] : columns)
        {
            arrangement.cells.emplace_back(columnLeft, rowTop, columnRight - columnLeft,
                                           rowBottom - rowTop);
        }
    }
    return arrangement;
}

void PlotGrid::placePlots()
{
    const Arrangement arrangement = arrange(QRectF(rect()), theme());
    for (qsizetype i = 0; i < m_plots.size(); ++i)
    {
        m_plots.at(i)->setGeometry(arrangement.cells[static_cast<std::size_t>(i)].toRect());
    }
    realign();
}

void PlotGrid::realign()
{
    for (PlotWidget* plot : std::as_const(m_plots))
    {
        plot->markDirty();
    }
}

LayoutConstraints PlotGrid::constraintsFor(const PlotWidget* plot) const
{
    LayoutConstraints constraints;
    const qsizetype   index = m_plots.indexOf(plot);
    if (index < 0)
    {
        return constraints;
    }
    const qsizetype row    = index / m_columns;
    const qsizetype column = index % m_columns;
    for (qsizetype i = 0; i < m_plots.size(); ++i)
    {
        const PlotWidget* other      = m_plots.at(i);
        const bool        sameRow    = i / m_columns == row;
        const bool        sameColumn = i % m_columns == column;
        if ((!sameRow && !sameColumn) || other->isHidden() || other->rect().isEmpty())
        {
            continue;
        }
        const QMarginsF margins = other->naturalMargins();
        if (sameColumn)
        {
            constraints.minLeft  = std::max(constraints.minLeft, margins.left());
            constraints.minRight = std::max(constraints.minRight, margins.right());
        }
        if (sameRow)
        {
            constraints.minTop    = std::max(constraints.minTop, margins.top());
            constraints.minBottom = std::max(constraints.minBottom, margins.bottom());
        }
    }
    return constraints;
}

QSize PlotGrid::sizeHint() const
{
    return {m_columns * kPlotSizeHint.width(), m_rows * kPlotSizeHint.height()};
}

QSize PlotGrid::minimumSizeHint() const
{
    return {m_columns * kPlotMinimumSize.width(), m_rows * kPlotMinimumSize.height()};
}

// Output
// -------------------------------------------------------------------------------------------------------

void PlotGrid::paintGrid(QPainter& painter, const QRectF& bounds, double devicePixelRatio,
                         const std::optional<Theme>& theme) const
{
    const Theme&      used        = theme ? *theme : this->theme();
    const Arrangement arrangement = arrange(bounds, used);
    painter.fillRect(bounds, used.background);
    if (!m_title.isEmpty())
    {
        m_text->draw(painter, m_title, titleFont(font(), used), used.text, arrangement.title,
                     Qt::AlignHCenter | Qt::AlignVCenter);
    }
    // The plots line up in this arrangement as they do on screen: by the margins each needs here.
    std::vector<QMarginsF> margins;
    margins.reserve(arrangement.cells.size());
    for (qsizetype i = 0; i < m_plots.size(); ++i)
    {
        margins.push_back(m_plots.at(i)->naturalMargins(
            arrangement.cells[static_cast<std::size_t>(i)], devicePixelRatio, theme));
    }
    for (qsizetype i = 0; i < m_plots.size(); ++i)
    {
        if (m_plots.at(i)->isHidden())
        {
            continue;
        }
        LayoutConstraints constraints;
        for (qsizetype other = 0; other < m_plots.size(); ++other)
        {
            const QMarginsF& needed = margins[static_cast<std::size_t>(other)];
            if (m_plots.at(other)->isHidden())
            {
                continue;
            }
            if (other % m_columns == i % m_columns)
            {
                constraints.minLeft  = std::max(constraints.minLeft, needed.left());
                constraints.minRight = std::max(constraints.minRight, needed.right());
            }
            if (other / m_columns == i / m_columns)
            {
                constraints.minTop    = std::max(constraints.minTop, needed.top());
                constraints.minBottom = std::max(constraints.minBottom, needed.bottom());
            }
        }
        painter.save();
        m_plots.at(i)->paintExport(painter, arrangement.cells[static_cast<std::size_t>(i)],
                                   devicePixelRatio, constraints, theme);
        painter.restore();
    }
}

QSize PlotGrid::exportSize(const ExportOptions& options) const
{
    return options.size.isEmpty() ? size() : options.size;
}

QImage PlotGrid::renderToImage(const ExportOptions& options) const
{
    const double pixelRatio = options.dpi > 0.0 ? options.dpi / kBaseDpi : devicePixelRatioF();
    return paintImage(exportSize(options), pixelRatio,
                      [this, &options](QPainter& painter, const QRectF& bounds, double ratio) {
                          paintGrid(painter, bounds, ratio, options.theme);
                      });
}

void PlotGrid::paint(QPainter& painter, const QRectF& rect) const
{
    const QPaintDevice* device = painter.device();
    painter.save();
    paintGrid(painter, rect, device != nullptr ? device->devicePixelRatioF() : 1.0, std::nullopt);
    painter.restore();
}

bool PlotGrid::exportImage(const QString& fileName, const ExportOptions& options) const
{
    const QImage image = renderToImage(options);
    if (image.isNull() || !image.save(fileName))
    {
        qCWarning(lcRender) << "PlotGrid::exportImage: cannot write" << fileName;
        return false;
    }
    return true;
}

bool PlotGrid::exportSvg(const QString& fileName, const ExportOptions& options) const
{
    const bool written =
        writeSvg(fileName, exportSize(options), logicalDpiY(), plainText(m_title),
                 [this, &options](QPainter& painter, const QRectF& bounds, double ratio) {
                     paintGrid(painter, bounds, ratio, options.theme);
                 });
    if (!written)
    {
        qCWarning(lcRender) << "PlotGrid::exportSvg: cannot write" << fileName;
    }
    return written;
}

bool PlotGrid::exportPdf(const QString& fileName, const ExportOptions& options) const
{
    const bool written =
        writePdf(fileName, exportSize(options), logicalDpiY(), plainText(m_title),
                 [this, &options](QPainter& painter, const QRectF& bounds, double ratio) {
                     paintGrid(painter, bounds, ratio, options.theme);
                 });
    if (!written)
    {
        qCWarning(lcRender) << "PlotGrid::exportPdf: cannot write" << fileName;
    }
    return written;
}

bool PlotGrid::exportTo(const QString& fileName, const ExportOptions& options) const
{
    const QString suffix = QFileInfo(fileName).suffix().toLower();
    if (suffix == QLatin1String("svg"))
    {
        return exportSvg(fileName, options);
    }
    if (suffix == QLatin1String("pdf"))
    {
        return exportPdf(fileName, options);
    }
    return exportImage(fileName, options);
}

void PlotGrid::copyToClipboard(const ExportOptions& options) const
{
    QGuiApplication::clipboard()->setImage(renderToImage(options));
}

// State
// --------------------------------------------------------------------------------------------------------

QJsonObject PlotGrid::saveState() const
{
    QJsonArray plots;
    for (const PlotWidget* plot : std::as_const(m_plots))
    {
        plots.append(plot->saveState());
    }
    QJsonObject state;
    state.insert(QLatin1String("format"), QString(state::kFormat));
    state.insert(QLatin1String("version"), state::kVersion);
    state.insert(QLatin1String("plots"), plots);
    return state;
}

bool PlotGrid::restoreState(const QJsonObject& state)
{
    const int version = state.value(QLatin1String("version")).toInt();
    if (state.value(QLatin1String("format")).toString() != state::kFormat || version < 1 ||
        version > state::kVersion)
    {
        qCWarning(lcData) << "PlotGrid::restoreState: not a state this version reads";
        return false;
    }
    const QJsonArray plots = state.value(QLatin1String("plots")).toArray();
    const qsizetype  count = std::min(plots.size(), m_plots.size());
    for (qsizetype i = 0; i < count; ++i)
    {
        m_plots.at(i)->restoreState(plots.at(i).toObject());
    }
    applyTickLabels();  // which plots write the values along a shared axis is the grid's to say
    return true;
}

// Events
// -------------------------------------------------------------------------------------------------------

void PlotGrid::paintEvent(QPaintEvent* /*event*/)
{
    QPainter     painter(this);
    const Theme& used = theme();
    painter.fillRect(rect(), used.background);
    if (!m_title.isEmpty())
    {
        m_text->draw(painter, m_title, titleFont(font(), used), used.text,
                     arrange(QRectF(rect()), used).title, Qt::AlignHCenter | Qt::AlignVCenter);
    }
}

void PlotGrid::resizeEvent(QResizeEvent* event)
{
    placePlots();
    QWidget::resizeEvent(event);
}

void PlotGrid::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange)
    {
        placePlots();  // the title may be another height
        update();
    }
    QWidget::changeEvent(event);
}

}  // namespace rocketplot

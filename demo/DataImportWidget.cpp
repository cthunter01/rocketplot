#include "DataImportWidget.h"

#include <QByteArray>
#include <QByteArrayView>
#include <QClipboard>
#include <QComboBox>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIODevice>
#include <QKeySequence>
#include <QLabel>
#include <QList>
#include <QListWidget>
#include <QListWidgetItem>
#include <QLocale>
#include <QMimeData>
#include <QPushButton>
#include <QShortcut>
#include <QSignalBlocker>
#include <QString>
#include <QUrl>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>
#include <Qt>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <utility>

#include "DelimitedText.h"
#include "rocketplot/Axis.h"
#include "rocketplot/LineSeries.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/ScatterSeries.h"
#include "rocketplot/Series.h"
#include "rocketplot/UniformX.h"
#include "rocketplot/enums.h"

namespace rocketplot::demo
{

namespace
{

constexpr int kListWidth      = 200;
constexpr int kRowNumber      = -1;  // "x column": the row number
constexpr int kPlottedAtFirst = 6;   // more lines than that at once are hard to tell apart

enum class Style : std::uint8_t
{
    LINES,
    POINTS,
    BOTH,
};

// Whether the values never go down (gaps aside): a time base rather than a measurement.
bool neverDecreases(std::span<const double> values)
{
    double last = -std::numeric_limits<double>::infinity();
    for (const double value : values)
    {
        if (value < last)
        {
            return false;
        }
        last = std::isnan(value) ? last : value;
    }
    return true;
}

// The column to take x from when a table is first shown: the first one of times; else the first
// column if it looks like a time base; else none (the row number).
int likelyXColumn(const DataTable& table)
{
    const auto time = std::ranges::find_if(table.columns, &DataTable::Column::isTime);
    if (time != table.columns.end())
    {
        return static_cast<int>(time - table.columns.begin());
    }
    const auto numeric = std::ranges::count_if(table.columns, &DataTable::Column::isNumeric);
    if (numeric > 1 && table.columns.front().isNumeric() &&
        neverDecreases(table.columns.front().values))
    {
        return 0;
    }
    return kRowNumber;
}

QString separatorName(char separator)
{
    switch (separator)
    {
        case '\t':
            return QStringLiteral("tab-separated");
        case ';':
            return QStringLiteral("semicolon-separated");
        case ',':
            return QStringLiteral("comma-separated");
        default:
            return QStringLiteral("space-separated");
    }
}

QString firstLocalFile(const QMimeData& mime)
{
    const QList<QUrl> urls = mime.urls();
    for (const QUrl& url : urls)
    {
        if (url.isLocalFile())
        {
            return url.toLocalFile();
        }
    }
    return {};
}

}  // namespace

DataImportWidget::DataImportWidget(QWidget* parent)
  : QWidget(parent),
    m_plot(new PlotWidget(this)),
    m_x(new QComboBox(this)),
    m_style(new QComboBox(this)),
    m_columns(new QListWidget(this)),
    m_status(new QLabel(this))
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* controls = new QHBoxLayout;
    auto* open     = new QPushButton(QStringLiteral("Open…"), this);
    auto* paste    = new QPushButton(QStringLiteral("Paste"), this);
    paste->setToolTip(
        QStringLiteral("Plot the table on the clipboard (%1)")
            .arg(QKeySequence(QKeySequence::Paste).toString(QKeySequence::NativeText)));
    m_style->addItem(QStringLiteral("Lines"), QVariant::fromValue(Style::LINES));
    m_style->addItem(QStringLiteral("Points"), QVariant::fromValue(Style::POINTS));
    m_style->addItem(QStringLiteral("Lines and points"), QVariant::fromValue(Style::BOTH));
    controls->addWidget(open);
    controls->addWidget(paste);
    controls->addWidget(new QLabel(QStringLiteral("X:"), this));
    controls->addWidget(m_x);
    controls->addWidget(new QLabel(QStringLiteral("Draw:"), this));
    controls->addWidget(m_style);
    controls->addWidget(m_status, 1);
    layout->addLayout(controls);

    auto* content = new QHBoxLayout;
    content->addWidget(m_plot, 1);
    m_columns->setFixedWidth(kListWidth);
    content->addWidget(m_columns);
    layout->addLayout(content, 1);

    setAcceptDrops(true);
    connect(open, &QPushButton::clicked, this, &DataImportWidget::openFile);
    const auto pasteClipboard = [this] {
        loadMimeData(QGuiApplication::clipboard()->mimeData(), QStringLiteral("Clipboard"));
    };
    connect(paste, &QPushButton::clicked, this, pasteClipboard);
    connect(new QShortcut(QKeySequence::Paste, this), &QShortcut::activated, this, pasteClipboard);
    connect(m_x, &QComboBox::currentIndexChanged, this, [this] { plotColumns(); });
    connect(m_style, &QComboBox::currentIndexChanged, this, [this] { plotColumns(); });
    connect(m_columns, &QListWidget::itemChanged, this, [this](const QListWidgetItem* item) {
        Series* series = m_series.at(static_cast<std::size_t>(item->data(Qt::UserRole).toInt()));
        if (series != nullptr)
        {
            series->setVisible(item->checkState() == Qt::Checked);
        }
    });
}

QString DataImportWidget::status() const
{
    return m_status->text();
}

bool DataImportWidget::canRead(const QMimeData* mime)
{
    return mime != nullptr && (!firstLocalFile(*mime).isEmpty() || mime->hasText());
}

bool DataImportWidget::loadMimeData(const QMimeData* mime, const QString& source)
{
    if (mime == nullptr)
    {
        return false;
    }
    if (const QString file = firstLocalFile(*mime); !file.isEmpty())
    {
        return loadFile(file);
    }
    if (mime->hasText())
    {
        return loadText(mime->text().toUtf8(), source);
    }
    m_status->setText(QStringLiteral("%1: no file or text there").arg(source));
    return false;
}

bool DataImportWidget::loadFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        m_status->setText(QStringLiteral("Cannot read %1: %2")
                              .arg(QDir::toNativeSeparators(path), file.errorString()));
        return false;
    }
    return loadText(file.readAll(), QFileInfo(path).fileName());
}

bool DataImportWidget::loadText(QByteArrayView text, const QString& source)
{
    QGuiApplication::setOverrideCursor(Qt::WaitCursor);  // a large file takes a moment
    DataTable table = parseDelimitedText(text);
    QGuiApplication::restoreOverrideCursor();
    if (table.isEmpty())
    {
        m_status->setText(QStringLiteral("%1: no numbers to plot").arg(source));
        return false;
    }
    // The series read the table's columns in place: they go before the table does.
    m_plot->clearSeries();
    m_series.clear();
    m_table = std::move(table);
    m_plot->setTitle(source);
    const QLocale locale;
    m_status->setText(
        QStringLiteral("%1 rows × %2 columns, %3%4")
            .arg(locale.toString(static_cast<qulonglong>(m_table.rows)),
                 locale.toString(static_cast<qulonglong>(m_table.columns.size())),
                 separatorName(m_table.separator),
                 m_table.hasHeader ? QStringLiteral(", named by the first line") : QString()));
    listColumns();
    plotColumns();
    return true;
}

void DataImportWidget::listColumns()
{
    const QSignalBlocker blockX(m_x);
    const QSignalBlocker blockColumns(m_columns);
    const int            x = likelyXColumn(m_table);
    m_x->clear();
    m_columns->clear();
    m_x->addItem(QStringLiteral("Row number"), kRowNumber);
    int plotted = 0;
    for (std::size_t i = 0; i < m_table.columns.size(); ++i)
    {
        const DataTable::Column& column = m_table.columns[i];
        if (!column.isNumeric())
        {
            continue;  // text: nothing to plot
        }
        const int index = static_cast<int>(i);
        m_x->addItem(column.name, index);
        auto* item = new QListWidgetItem(column.name, m_columns);
        item->setData(Qt::UserRole, index);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        const bool show = index != x && plotted < kPlottedAtFirst;
        item->setCheckState(show ? Qt::Checked : Qt::Unchecked);
        plotted += show ? 1 : 0;
    }
    m_x->setCurrentIndex(m_x->findData(x));
}

Series* DataImportWidget::plotColumn(const DataTable::Column& y, int x, bool asPoints)
{
    // [snippet]
    // The table stays where it is: a view reads a column in place, without copying it.
    const std::span<const double> values(y.values);
    if (x == kRowNumber)
    {
        // Against the row number: 0, 1, 2, ...
        if (asPoints)
        {
            return m_plot->addScatterView(UniformX{}, values, y.name);
        }
        return m_plot->addLineView(UniformX{}, values, y.name);
    }
    const std::span<const double> time(m_table.columns[static_cast<std::size_t>(x)].values);
    if (asPoints)
    {
        return m_plot->addScatterView(time, values, y.name);
    }
    return m_plot->addLineView(time, values, y.name);
    // [/snippet]
}

void DataImportWidget::plotColumns()
{
    m_plot->clearSeries();
    m_series.assign(m_table.columns.size(), nullptr);
    const int  x     = xColumn();
    const auto style = m_style->currentData().value<Style>();
    for (int row = 0; row < m_columns->count(); ++row)
    {
        QListWidgetItem* item   = m_columns->item(row);
        const int        column = item->data(Qt::UserRole).toInt();
        item->setHidden(column == x);
        if (column == x)
        {
            continue;
        }
        Series* series = plotColumn(m_table.columns[static_cast<std::size_t>(column)], x,
                                    style == Style::POINTS);
        if (style == Style::BOTH)
        {
            series->setMarker(Marker::CIRCLE);
        }
        series->setVisible(item->checkState() == Qt::Checked);
        // The legend hides and shows series too: keep the list in step.
        connect(series, &Series::changed, this, [item, series] {
            item->setCheckState(series->isVisible() ? Qt::Checked : Qt::Unchecked);
        });
        m_series[static_cast<std::size_t>(column)] = series;
    }
    // [snippet]

    // Dates and times were read as seconds since 1970, which is what a DATE_TIME axis takes.
    const bool isTime = x != kRowNumber && m_table.columns[static_cast<std::size_t>(x)].isTime;
    m_plot->xAxis()->setScaleType(isTime ? ScaleType::DATE_TIME : ScaleType::LINEAR);
    // [/snippet]
    m_plot->xAxis()->setLabel(x == kRowNumber ? QStringLiteral("Row")
                                              : m_table.columns[static_cast<std::size_t>(x)].name);
    m_plot->resetView();
}

void DataImportWidget::openFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Open data"), QString(),
        QStringLiteral("Delimited text (*.csv *.tsv *.txt *.dat);;All files (*)"));
    if (!path.isEmpty())
    {
        loadFile(path);
    }
}

int DataImportWidget::xColumn() const
{
    return m_x->currentData().toInt();
}

void DataImportWidget::setXColumn(int column)
{
    const int index = m_x->findData(column);
    if (index >= 0)
    {
        m_x->setCurrentIndex(index);
    }
}

bool DataImportWidget::isPlotted(int column) const
{
    const Series* series = column >= 0 && std::cmp_less(column, m_series.size())
                               ? m_series[static_cast<std::size_t>(column)]
                               : nullptr;
    return series != nullptr && series->isVisible();
}

void DataImportWidget::setPlotted(int column, bool plotted)
{
    for (int row = 0; row < m_columns->count(); ++row)
    {
        if (m_columns->item(row)->data(Qt::UserRole).toInt() == column)
        {
            m_columns->item(row)->setCheckState(plotted ? Qt::Checked : Qt::Unchecked);
        }
    }
}

void DataImportWidget::dragEnterEvent(QDragEnterEvent* event)
{
    if (canRead(event->mimeData()))
    {
        event->acceptProposedAction();
    }
}

void DataImportWidget::dropEvent(QDropEvent* event)
{
    if (loadMimeData(event->mimeData(), QStringLiteral("Dropped text")))
    {
        event->acceptProposedAction();
    }
}

}  // namespace rocketplot::demo

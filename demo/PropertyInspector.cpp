#include "PropertyInspector.h"

#include <QAbstractItemDelegate>
#include <QAbstractItemView>
#include <QByteArray>
#include <QColor>
#include <QColorDialog>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLatin1String>
#include <QLineEdit>
#include <QList>
#include <QLocale>
#include <QMetaEnum>
#include <QMetaMethod>
#include <QMetaObject>
#include <QMetaProperty>
#include <QMetaType>
#include <QModelIndex>
#include <QObject>
#include <QPixmap>
#include <QPointF>
#include <QPointer>
#include <QScrollBar>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QString>
#include <QStringList>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>
#include <QTimeZone>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVariant>
#include <QWidget>
#include <Qt>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

#include "rocketplot/Annotation.h"
#include "rocketplot/Axis.h"
#include "rocketplot/Legend.h"
#include "rocketplot/LineSeries.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/Series.h"

namespace rocketplot::demo
{

namespace
{

constexpr int kNameColumn   = 0;
constexpr int kValueColumn  = 1;
constexpr int kButtonColumn = 2;
constexpr int kKeyRole      = Qt::UserRole;  // a row's place in the tree, for remembering it open
constexpr int kIndentation  = 12;            // of each level of the tree
constexpr int kLevels       = 4;   // plot, its parts, a series or annotation, its properties
constexpr int kPadding      = 16;  // around the text of a cell
constexpr int kButtonsWidth = 64;  // the column of the buttons, and the scroll bar
constexpr const char* kLongestName  = "includedInAutoscale";
constexpr const char* kLongestValue = "LOGARITHMIC  #";
constexpr int         kSwatchSize   = 12;
constexpr int         kDigits       = 12;  // of a number: enough to tell two times of day apart
constexpr int         kPointDigits  = 6;
constexpr int kPenStyleCount        = 6;  // NoPen to DashDotDotLine: the rest aren't styles to pick

// How a property's value is shown and edited.
enum class Kind : std::uint8_t
{
    FLAG,       // bool: a checkbox
    NUMBER,     // double
    INTEGER,    // int
    TEXT,       // QString
    COLOR,      // QColor, as "#rrggbb"
    CHOICE,     // an enumeration: a list of its enumerators
    ALIGNMENT,  // Qt::Alignment: a list of the nine places
    POINT,      // QPointF, as "x, y"
    TIME_ZONE,  // QTimeZone, as its IANA name
    OTHER,      // shown as text if it has any, not edited
};

Kind kindOf(const QMetaProperty& property)
{
    const QMetaType type = property.metaType();
    if (type == QMetaType::fromType<Qt::Alignment>())
    {
        return Kind::ALIGNMENT;
    }
    if (property.isEnumType())
    {
        return Kind::CHOICE;
    }
    if (type == QMetaType::fromType<QTimeZone>())
    {
        return Kind::TIME_ZONE;
    }
    switch (type.id())
    {
        case QMetaType::Bool:
            return Kind::FLAG;
        case QMetaType::Double:
            return Kind::NUMBER;
        case QMetaType::Int:
            return Kind::INTEGER;
        case QMetaType::QString:
            return Kind::TEXT;
        case QMetaType::QColor:
            return Kind::COLOR;
        case QMetaType::QPointF:
            return Kind::POINT;
        default:
            return Kind::OTHER;
    }
}

struct AlignmentName
{
    Qt::AlignmentFlag flag;
    QLatin1String     name;
};
constexpr std::array kHorizontal{
    AlignmentName{.flag = Qt::AlignLeft, .name = QLatin1String("Left")},
    AlignmentName{.flag = Qt::AlignHCenter, .name = QLatin1String("Center")},
    AlignmentName{.flag = Qt::AlignRight, .name = QLatin1String("Right")},
};
constexpr std::array kVertical{
    AlignmentName{.flag = Qt::AlignTop, .name = QLatin1String("Top")},
    AlignmentName{.flag = Qt::AlignVCenter, .name = QLatin1String("Middle")},
    AlignmentName{.flag = Qt::AlignBottom, .name = QLatin1String("Bottom")},
};

QString alignmentText(const AlignmentName& horizontal, const AlignmentName& vertical)
{
    return QStringLiteral("%1 | %2").arg(horizontal.name, vertical.name);
}

QString alignmentText(Qt::Alignment alignment)
{
    const auto named = [alignment](const std::array<AlignmentName, 3>& names) {
        for (const AlignmentName& name : names)
        {
            if (alignment.testFlag(name.flag))
            {
                return name;
            }
        }
        return names.front();
    };
    return alignmentText(named(kHorizontal), named(kVertical));
}

std::optional<Qt::Alignment> alignmentFrom(const QString& text)
{
    for (const AlignmentName& horizontal : kHorizontal)
    {
        for (const AlignmentName& vertical : kVertical)
        {
            if (text == alignmentText(horizontal, vertical))
            {
                return horizontal.flag | vertical.flag;
            }
        }
    }
    return std::nullopt;
}

// What a list editor offers for @p property; nothing for the kinds edited as text.
QStringList choicesOf(const QMetaProperty& property)
{
    QStringList choices;
    switch (kindOf(property))
    {
        case Kind::ALIGNMENT:
            for (const AlignmentName& vertical : kVertical)
            {
                for (const AlignmentName& horizontal : kHorizontal)
                {
                    choices.append(alignmentText(horizontal, vertical));
                }
            }
            break;
        case Kind::CHOICE:
        {
            const QMetaEnum enumerator = property.enumerator();
            const bool      penStyle   = QLatin1String(enumerator.enumName()) == "PenStyle";
            const int       count      = penStyle ? kPenStyleCount : enumerator.keyCount();
            for (int i = 0; i < count; ++i)
            {
                choices.append(QString::fromLatin1(enumerator.key(i)));
            }
            break;
        }
        default:
            break;
    }
    return choices;
}

QString colorName(const QColor& color)
{
    return color.name(color.alpha() == 255 ? QColor::HexRgb : QColor::HexArgb);
}

// A value as its row shows it (a flag shows a checkbox instead).
QString textOf(Kind kind, const QVariant& value)
{
    switch (kind)
    {
        case Kind::FLAG:
            return {};
        case Kind::NUMBER:
            return QString::number(value.toDouble(), 'g', kDigits);
        case Kind::COLOR:
            return colorName(value.value<QColor>());
        case Kind::ALIGNMENT:
            return alignmentText(value.value<Qt::Alignment>());
        case Kind::POINT:
        {
            const QPointF point = value.toPointF();
            return QStringLiteral("%1, %2").arg(QString::number(point.x(), 'g', kPointDigits),
                                                QString::number(point.y(), 'g', kPointDigits));
        }
        case Kind::TIME_ZONE:
            return QString::fromLatin1(value.value<QTimeZone>().id());
        case Kind::INTEGER:
        case Kind::TEXT:
        case Kind::CHOICE:  // an enumeration's QVariant converts to its enumerator's name
        case Kind::OTHER:
            break;
    }
    return value.toString();
}

std::optional<double> numberFrom(const QString& text)
{
    bool   isNumber = false;
    double number   = text.trimmed().toDouble(&isNumber);
    if (!isNumber)
    {
        number = QLocale().toDouble(text.trimmed(), &isNumber);  // "1,5" where that's a number
    }
    return isNumber ? std::optional(number) : std::nullopt;
}

// The value @p text stands for, for a property of @p kind; invalid if it doesn't stand for one.
QVariant valueFrom(Kind kind, const QString& text)
{
    switch (kind)
    {
        case Kind::NUMBER:
            if (const std::optional<double> number = numberFrom(text))
            {
                return *number;
            }
            break;
        case Kind::INTEGER:
        {
            bool      isNumber = false;
            const int number   = text.trimmed().toInt(&isNumber);
            return isNumber ? QVariant(number) : QVariant();
        }
        case Kind::COLOR:
            if (const QColor color = QColor::fromString(text.trimmed()); color.isValid())
            {
                return color;
            }
            break;
        case Kind::ALIGNMENT:
            if (const std::optional<Qt::Alignment> alignment = alignmentFrom(text))
            {
                return QVariant::fromValue(*alignment);
            }
            break;
        case Kind::POINT:
        {
            const QStringList           parts = text.split(QLatin1Char(','));
            const std::optional<double> x     = numberFrom(parts.value(0));
            const std::optional<double> y     = numberFrom(parts.value(1));
            return parts.size() == 2 && x && y ? QVariant(QPointF(*x, *y)) : QVariant();
        }
        case Kind::TIME_ZONE:
            if (const QTimeZone zone(text.trimmed().toLatin1()); zone.isValid())
            {
                return QVariant::fromValue(zone);
            }
            break;
        case Kind::TEXT:
        case Kind::CHOICE:  // QMetaProperty::write() takes an enumerator's name
            return text;
        case Kind::FLAG:
        case Kind::OTHER:
            break;
    }
    return {};
}

QPixmap swatch(const QColor& color)
{
    QPixmap pixmap(kSwatchSize, kSwatchSize);
    pixmap.fill(color);
    return pixmap;
}

// "rocketplot::LineSeries" -> "LineSeries"
QString className(const QObject& object)
{
    return QString::fromLatin1(object.metaObject()->className()).section(QLatin1String("::"), -1);
}

// What the row holding @p object's properties says; @p number counts the objects of its kind.
QString titleOf(const QObject& object, int number)
{
    if (const auto* plot = qobject_cast<const PlotWidget*>(&object))
    {
        return plot->title().isEmpty() ? QStringLiteral("Plot %1").arg(number)
                                       : QStringLiteral("Plot: %1").arg(plot->title());
    }
    if (const auto* axis = qobject_cast<const Axis*>(&object))
    {
        if (axis->orientation() == Qt::Horizontal)
        {
            return QStringLiteral("X axis");
        }
        return axis->isSecondary() ? QStringLiteral("Y axis 2 (right)") : QStringLiteral("Y axis");
    }
    if (qobject_cast<const Legend*>(&object) != nullptr)
    {
        return QStringLiteral("Legend");
    }
    if (const auto* series = qobject_cast<const Series*>(&object))
    {
        const QString name =
            series->name().isEmpty() ? QStringLiteral("Series %1").arg(number) : series->name();
        return QStringLiteral("%1 (%2)").arg(name, className(object));
    }
    // An annotation: its kind, and what it says if it says anything.
    QString text = object.property("label").toString();
    if (text.isEmpty())
    {
        text = object.property("text").toString();
    }
    return text.isEmpty() ? className(object)
                          : QStringLiteral("%1: %2").arg(className(object), text);
}

// The first property that is the object's own rather than QObject's or QWidget's.
int firstOwnProperty(const QObject& object)
{
    return object.isWidgetType() ? QWidget::staticMetaObject.propertyCount()
                                 : QObject::staticMetaObject.propertyCount();
}

}  // namespace

// Edits a value in place: a list for the kinds with choices, a line of text for the others.
class PropertyInspector::ValueDelegate : public QStyledItemDelegate
{
public:
    explicit ValueDelegate(PropertyInspector* inspector)
      : QStyledItemDelegate(inspector), m_inspector(inspector)
    {
    }

    QWidget* createEditor(QWidget*           parent, const QStyleOptionViewItem& /*option*/,
                          const QModelIndex& index) const override
    {
        const Binding* binding = m_inspector->bindingAt(index);
        if (binding == nullptr || index.column() != kValueColumn)
        {
            return nullptr;
        }
        const QStringList choices = choicesOf(binding->property);
        if (choices.isEmpty())
        {
            return new QLineEdit(parent);
        }
        auto* list = new QComboBox(parent);
        list->addItems(choices);
        // Choosing is the edit: nothing more to confirm.
        connect(list, &QComboBox::activated, m_inspector,
                [inspector = m_inspector, list] { inspector->finishEditing(list); });
        return list;
    }

    void setEditorData(QWidget* editor, const QModelIndex& index) const override
    {
        const QString text = index.data(Qt::DisplayRole).toString();
        if (auto* list = qobject_cast<QComboBox*>(editor))
        {
            list->setCurrentText(text);
        }
        else if (auto* line = qobject_cast<QLineEdit*>(editor))
        {
            line->setText(text);
        }
    }

    void setModelData(QWidget*           editor, QAbstractItemModel* /*model*/,
                      const QModelIndex& index) const override
    {
        QString text;
        if (const auto* list = qobject_cast<const QComboBox*>(editor))
        {
            text = list->currentText();
        }
        else if (const auto* line = qobject_cast<const QLineEdit*>(editor))
        {
            text = line->text();
        }
        m_inspector->setValue(m_inspector->itemFromIndex(index), text);
    }

private:
    PropertyInspector* m_inspector;
};

PropertyInspector::PropertyInspector(QWidget* parent) : QTreeWidget(parent)
{
    setColumnCount(3);
    setHeaderLabels({QStringLiteral("Property"), QStringLiteral("Value"), QString()});
    // Room for the longest property name three levels in and for a value like "LOGARITHMIC"; the
    // value column takes what a wider inspector has to spare. The rows that name a part of the
    // plot span the columns, so a long title doesn't widen the first.
    setIndentation(kIndentation);
    const int nameWidth =
        (kLevels * kIndentation) + fontMetrics().horizontalAdvance(QLatin1String(kLongestName));
    const int valueWidth = fontMetrics().horizontalAdvance(QLatin1String(kLongestValue));
    header()->setStretchLastSection(false);
    header()->setSectionResizeMode(kNameColumn, QHeaderView::Interactive);
    header()->resizeSection(kNameColumn, nameWidth + kPadding);
    header()->setSectionResizeMode(kValueColumn, QHeaderView::Stretch);
    header()->setSectionResizeMode(kButtonColumn, QHeaderView::ResizeToContents);
    setMinimumWidth(nameWidth + valueWidth + kButtonsWidth + (2 * kPadding));
    setUniformRowHeights(true);
    setAlternatingRowColors(true);
    setItemDelegate(new ValueDelegate(this));
    setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed |
                    QAbstractItemView::AnyKeyPressed);

    // One click on a value edits it.
    connect(this, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item, int column) {
        if (column == kValueColumn && item->flags().testFlag(Qt::ItemIsEditable))
        {
            editItem(item, kValueColumn);
        }
    });
    connect(this, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem* item, int column) {
        const Binding* binding = bindingOf(item);
        if (!m_updating && column == kValueColumn && binding != nullptr &&
            kindOf(binding->property) == Kind::FLAG)
        {
            setValue(item, item->checkState(kValueColumn) == Qt::Checked);
        }
    });
    connect(this, &QTreeWidget::itemExpanded, this, [this](const QTreeWidgetItem* item) {
        m_expanded.insert(item->data(kNameColumn, kKeyRole).toString());
    });
    connect(this, &QTreeWidget::itemCollapsed, this, [this](const QTreeWidgetItem* item) {
        m_expanded.remove(item->data(kNameColumn, kKeyRole).toString());
    });
}

void PropertyInspector::setPlots(const QList<PlotWidget*>& plots)
{
    for (const QPointer<PlotWidget>& plot : std::as_const(m_plots))
    {
        if (plot)
        {
            disconnect(plot, nullptr, this, nullptr);
        }
    }
    m_plots.clear();
    m_expanded.clear();
    for (PlotWidget* plot : plots)
    {
        // A plot's own rows start out open and the rows of its parts closed; with one plot there
        // is room for its axes and the list of its series too.
        const QString key = QStringLiteral("plot%1").arg(m_plots.size());
        m_expanded.insert(key);
        if (plots.size() == 1)
        {
            m_expanded.insert(key + QLatin1String("/x"));
            m_expanded.insert(key + QLatin1String("/y"));
            m_expanded.insert(key + QLatin1String("/series"));
        }
        m_plots.append(plot);
        connect(plot, &PlotWidget::seriesAdded, this, &PropertyInspector::scheduleRebuild);
        connect(plot, &PlotWidget::seriesRemoved, this, &PropertyInspector::scheduleRebuild);
        connect(plot, &PlotWidget::annotationAdded, this, &PropertyInspector::scheduleRebuild);
        connect(plot, &PlotWidget::annotationRemoved, this, &PropertyInspector::scheduleRebuild);
        connect(plot, &QObject::destroyed, this, &PropertyInspector::scheduleRebuild);
    }
    rebuild();
}

QTreeWidgetItem* PropertyInspector::propertyItem(const QObject* object, const char* name) const
{
    for (const Binding& binding : m_bindings)
    {
        if (binding.object == object && QLatin1String(binding.property.name()) == name)
        {
            return binding.item;
        }
    }
    return nullptr;
}

QTreeWidgetItem* PropertyInspector::objectItem(const QObject* object) const
{
    for (const Node& node : m_nodes)
    {
        if (node.object == object)
        {
            return node.item;
        }
    }
    return nullptr;
}

PropertyInspector::Binding* PropertyInspector::bindingOf(const QTreeWidgetItem* item)
{
    const auto row = m_rows.constFind(item);
    return row == m_rows.constEnd() ? nullptr : &m_bindings[static_cast<std::size_t>(*row)];
}

const PropertyInspector::Binding* PropertyInspector::bindingAt(const QModelIndex& index)
{
    return bindingOf(itemFromIndex(index));
}

bool PropertyInspector::setValue(QTreeWidgetItem* item, const QVariant& value)
{
    const Binding* binding = bindingOf(item);
    if (binding == nullptr || !binding->object || !binding->property.isWritable())
    {
        return false;
    }
    const QVariant converted = value.metaType() == QMetaType::fromType<QString>()
                                   ? valueFrom(kindOf(binding->property), value.toString())
                                   : value;
    const bool written = converted.isValid() && binding->property.write(binding->object, converted);
    // Show what the property holds now: its setter may have refused the value, or adjusted it.
    refresh();
    return written;
}

void PropertyInspector::resetValue(QTreeWidgetItem* item)
{
    if (const Binding* binding = bindingOf(item); binding != nullptr && binding->object)
    {
        binding->property.reset(binding->object);
        refresh();
    }
}

void PropertyInspector::chooseColor(QTreeWidgetItem* item)
{
    const Binding* binding = bindingOf(item);
    if (binding == nullptr || !binding->object)
    {
        return;
    }
    // The rows may be made anew while the dialog is open: remember the property, not the row.
    const QPointer<QObject> object = binding->object;
    const QByteArray        name   = binding->property.name();
    const QColor            chosen =
        QColorDialog::getColor(binding->shown.value<QColor>(), this, QStringLiteral("Color"),
                               QColorDialog::ShowAlphaChannel);
    if (chosen.isValid() && object)
    {
        setValue(propertyItem(object, name.constData()), chosen);
    }
}

void PropertyInspector::finishEditing(QWidget* editor)
{
    commitData(editor);
    closeEditor(editor, QAbstractItemDelegate::NoHint);
}

void PropertyInspector::showEvent(QShowEvent* event)
{
    QTreeWidget::showEvent(event);
    refresh();  // nothing was kept up to date while hidden
}

void PropertyInspector::scheduleRefresh()
{
    if (m_refreshPending || !isVisible())
    {
        return;
    }
    m_refreshPending = true;
    QTimer::singleShot(0, this, [this] {
        m_refreshPending = false;
        refresh();
    });
}

void PropertyInspector::scheduleRebuild()
{
    if (m_rebuildPending)
    {
        return;
    }
    m_rebuildPending = true;
    QTimer::singleShot(0, this, [this] {
        m_rebuildPending = false;
        rebuild();
    });
}

QTreeWidgetItem* PropertyInspector::addGroup(QTreeWidgetItem* parent, const QString& key)
{
    auto* item = new QTreeWidgetItem(parent);
    item->setData(kNameColumn, kKeyRole, key);
    item->setFlags(Qt::ItemIsEnabled);
    item->setFirstColumnSpanned(true);
    item->setExpanded(m_expanded.contains(key));
    return item;
}

QTreeWidgetItem* PropertyInspector::addObject(QTreeWidgetItem* parent, QObject* object,
                                              const QString& key, int number)
{
    QTreeWidgetItem* group = addGroup(parent, key);
    group->setText(kNameColumn, titleOf(*object, number));
    m_nodes.push_back({.object = object, .item = group, .number = number});

    const QMetaObject& meta = *object->metaObject();
    const QMetaMethod  refresh =
        metaObject()->method(metaObject()->indexOfSlot("scheduleRefresh()"));
    for (int i = firstOwnProperty(*object); i < meta.propertyCount(); ++i)
    {
        const QMetaProperty property = meta.property(i);
        const Kind          kind     = kindOf(property);
        auto*               item     = new QTreeWidgetItem(group);
        item->setText(kNameColumn, QString::fromLatin1(property.name()));
        item->setToolTip(kNameColumn, QString::fromLatin1(property.typeName()));
        Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
        if (property.isWritable() && kind == Kind::FLAG)
        {
            flags |= Qt::ItemIsUserCheckable;
        }
        else if (property.isWritable() && kind != Kind::OTHER)
        {
            flags |= Qt::ItemIsEditable;
        }
        item->setFlags(flags);
        if (!property.isWritable())
        {
            item->setForeground(kValueColumn, palette().placeholderText());
        }
        m_rows.insert(item, static_cast<int>(m_bindings.size()));
        Binding& binding = m_bindings.emplace_back(
            Binding{.object = object, .property = property, .item = item, .shown = {}});
        display(binding, property.read(object));
        addButtons(binding);
        if (property.hasNotifySignal())
        {
            connect(object, property.notifySignal(), this, refresh, Qt::UniqueConnection);
        }
    }
    return group;
}

void PropertyInspector::addButtons(Binding& binding)
{
    const bool color = kindOf(binding.property) == Kind::COLOR && binding.property.isWritable();
    if (!color && !binding.property.isResettable())
    {
        return;
    }
    auto* buttons = new QWidget(this);
    auto* layout  = new QHBoxLayout(buttons);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    QTreeWidgetItem* item = binding.item;
    if (color)
    {
        auto* choose = new QToolButton(buttons);
        choose->setText(QStringLiteral("…"));
        choose->setToolTip(QStringLiteral("Choose a color"));
        choose->setAutoRaise(true);
        connect(choose, &QToolButton::clicked, this, [this, item] { chooseColor(item); });
        layout->addWidget(choose);
    }
    if (binding.property.isResettable())
    {
        auto* reset = new QToolButton(buttons);
        reset->setText(QStringLiteral("↺"));
        reset->setToolTip(QStringLiteral("Back to the default"));
        reset->setAutoRaise(true);
        connect(reset, &QToolButton::clicked, this, [this, item] { resetValue(item); });
        layout->addWidget(reset);
    }
    setItemWidget(item, kButtonColumn, buttons);
}

void PropertyInspector::display(Binding& binding, const QVariant& value)
{
    const Kind kind = kindOf(binding.property);
    binding.shown   = value;
    if (kind == Kind::FLAG)
    {
        binding.item->setCheckState(kValueColumn, value.toBool() ? Qt::Checked : Qt::Unchecked);
        return;
    }
    binding.item->setText(kValueColumn, textOf(kind, value));
    if (kind == Kind::COLOR)
    {
        binding.item->setIcon(kValueColumn, swatch(value.value<QColor>()));
    }
}

void PropertyInspector::rebuild()
{
    const int scrolled = verticalScrollBar()->value();
    m_updating         = true;
    clear();
    m_bindings.clear();
    m_nodes.clear();
    m_rows.clear();
    for (qsizetype number = 0; number < m_plots.size(); ++number)
    {
        PlotWidget* plot = m_plots.at(number);
        if (plot == nullptr)
        {
            continue;
        }
        const QString    key = QStringLiteral("plot%1").arg(number);
        QTreeWidgetItem* top =
            addObject(invisibleRootItem(), plot, key, static_cast<int>(number) + 1);
        addObject(top, plot->xAxis(), key + QLatin1String("/x"), 1);
        addObject(top, plot->yAxis(), key + QLatin1String("/y"), 1);
        addObject(top, plot->yAxis2(), key + QLatin1String("/y2"), 1);
        addObject(top, plot->legend(), key + QLatin1String("/legend"), 1);

        const QList<Series*> series = plot->series();
        QTreeWidgetItem*     group  = addGroup(top, key + QLatin1String("/series"));
        group->setText(kNameColumn, QStringLiteral("Series (%1)").arg(series.size()));
        for (qsizetype i = 0; i < series.size(); ++i)
        {
            addObject(group, series.at(i), QStringLiteral("%1/series/%2").arg(key).arg(i),
                      static_cast<int>(i) + 1);
        }
        const QList<Annotation*> annotations = plot->annotations();
        group                                = addGroup(top, key + QLatin1String("/annotations"));
        group->setText(kNameColumn, QStringLiteral("Annotations (%1)").arg(annotations.size()));
        for (qsizetype i = 0; i < annotations.size(); ++i)
        {
            addObject(group, annotations.at(i), QStringLiteral("%1/annotations/%2").arg(key).arg(i),
                      static_cast<int>(i) + 1);
        }
    }
    m_updating = false;
    verticalScrollBar()->setValue(scrolled);
}

void PropertyInspector::refresh()
{
    m_updating = true;
    for (Binding& binding : m_bindings)
    {
        if (!binding.object)
        {
            continue;
        }
        if (const QVariant value = binding.property.read(binding.object); value != binding.shown)
        {
            display(binding, value);
        }
    }
    for (const Node& node : m_nodes)
    {
        if (node.object)
        {
            node.item->setText(kNameColumn, titleOf(*node.object, node.number));
        }
    }
    m_updating = false;
}

}  // namespace rocketplot::demo

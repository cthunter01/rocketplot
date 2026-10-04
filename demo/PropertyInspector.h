#pragma once

#include <QHash>
#include <QList>
#include <QMetaProperty>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QTreeWidget>
#include <QVariant>
#include <vector>

class QModelIndex;
class QShowEvent;
class QTreeWidgetItem;
class QWidget;

namespace rocketplot
{
class PlotWidget;
}  // namespace rocketplot

namespace rocketplot::demo
{

/// A tree of everything plots let you set: the properties (Q_PROPERTY) of each plot and of its
/// axes, legend, series and annotations, found through Qt's meta-object system. A value follows
/// its property as it changes (pan a plot and its axis ranges move), and editing a value sets the
/// property: a checkbox for a flag, a list for an enumeration, text for the rest. A property that
/// follows the theme until it is set has a button to hand it back.
class PropertyInspector : public QTreeWidget
{
    Q_OBJECT

public:
    explicit PropertyInspector(QWidget* parent = nullptr);

    /// Shows @p plots instead of the ones shown before.
    void setPlots(const QList<PlotWidget*>& plots);

    /// The row of @p object's property @p name, or null if there is none. Rows are made anew
    /// when a series or annotation comes or goes.
    [[nodiscard]] QTreeWidgetItem* propertyItem(const QObject* object, const char* name) const;
    /// The row that holds the properties of @p object.
    [[nodiscard]] QTreeWidgetItem* objectItem(const QObject* object) const;

    /// Sets the property of @p item: from a value of its type, or from text as an editor holds
    /// it ("2.5", "#ff8800", "LOGARITHMIC", "1, 0.5"). False if that isn't a value for it.
    bool setValue(QTreeWidgetItem* item, const QVariant& value);
    /// Hands the property of @p item back to its default (the theme's color, say).
    void resetValue(QTreeWidgetItem* item);

protected:
    void showEvent(QShowEvent* event) override;

private:
    // A property changed: bring the rows up to date, once control returns to the event loop. A
    // slot, so that any property's notify signal can be connected to it by its meta-method.
    Q_SLOT void scheduleRefresh();
    // A series or annotation came or went: make the rows anew.
    void scheduleRebuild();

    class ValueDelegate;
    friend class ValueDelegate;

    // A row and the property it shows.
    struct Binding
    {
        QPointer<QObject> object;
        QMetaProperty     property;
        QTreeWidgetItem*  item = nullptr;
        QVariant          shown;  // the value the row shows
    };
    // A row that holds an object's properties.
    struct Node
    {
        QPointer<QObject> object;
        QTreeWidgetItem*  item   = nullptr;
        int               number = 0;  // among its kind, from 1
    };

    void                   rebuild();
    void                   refresh();
    QTreeWidgetItem*       addGroup(QTreeWidgetItem* parent, const QString& key);
    QTreeWidgetItem*       addObject(QTreeWidgetItem* parent, QObject* object, const QString& key,
                                     int number);
    void                   addButtons(Binding& binding);
    static void            display(Binding& binding, const QVariant& value);
    void                   chooseColor(QTreeWidgetItem* item);
    [[nodiscard]] Binding* bindingOf(const QTreeWidgetItem* item);
    [[nodiscard]] const Binding* bindingAt(const QModelIndex& index);
    // The delegate's editor is done: take its value and close it.
    void finishEditing(QWidget* editor);

    QList<QPointer<PlotWidget>>        m_plots;
    std::vector<Binding>               m_bindings;
    std::vector<Node>                  m_nodes;
    QHash<const QTreeWidgetItem*, int> m_rows;      // into m_bindings
    QSet<QString>                      m_expanded;  // the keys of the rows that are open
    bool                               m_updating       = false;  // the rows are being written
    bool                               m_refreshPending = false;
    bool                               m_rebuildPending = false;
};

}  // namespace rocketplot::demo

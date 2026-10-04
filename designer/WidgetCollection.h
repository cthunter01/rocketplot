#pragma once

#include <QList>
#include <QObject>

#include <QtUiPlugin/QDesignerCustomWidgetCollectionInterface>

class QDesignerCustomWidgetInterface;

namespace rocketplot::designer
{

/// The plugin Qt Designer loads: it adds rocketplot's widgets to the widget box.
class WidgetCollection : public QObject, public QDesignerCustomWidgetCollectionInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QDesignerCustomWidgetCollectionInterface")
    Q_INTERFACES(QDesignerCustomWidgetCollectionInterface)

public:
    explicit WidgetCollection(QObject* parent = nullptr);

    [[nodiscard]] QList<QDesignerCustomWidgetInterface*> customWidgets() const override;

private:
    QList<QDesignerCustomWidgetInterface*> m_widgets;
};

}  // namespace rocketplot::designer

#include "WidgetCollection.h"

#include <QList>
#include <QObject>

#include "CustomWidget.h"

namespace rocketplot::designer
{

WidgetCollection::WidgetCollection(QObject* parent)
  : QObject(parent),
    m_widgets{
        new CustomWidget(plotWidgetDescription(), this),
        new CustomWidget(plotGridDescription(), this),
    }
{
}

QList<QDesignerCustomWidgetInterface*> WidgetCollection::customWidgets() const
{
    return m_widgets;
}

}  // namespace rocketplot::designer

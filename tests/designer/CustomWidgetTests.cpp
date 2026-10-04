#include "CustomWidget.h"

#include <QFileInfo>
#include <QIcon>
#include <QString>
#include <QWidget>
#include <QXmlStreamReader>
#include <memory>

#include <QtUiPlugin/QDesignerCustomWidgetInterface>
#include <gtest/gtest.h>

#include "rocketplot/PlotGrid.h"
#include "rocketplot/PlotWidget.h"

namespace
{

using rocketplot::designer::CustomWidget;
using rocketplot::designer::plotGridDescription;
using rocketplot::designer::plotWidgetDescription;

// What a widget's XML for Designer says: the class of the widget it makes, the class it declares
// as a custom widget, the properties it sets, and whether the XML is well formed.
struct DomXml
{
    QString     widgetClass;
    QString     customClass;
    QString     extends;
    QStringList properties;
    bool        wellFormed = false;

    explicit DomXml(const QString& xml)
    {
        QXmlStreamReader reader(xml);
        while (!reader.atEnd())
        {
            if (reader.readNext() != QXmlStreamReader::StartElement)
            {
                continue;
            }
            if (reader.name() == QLatin1String("widget"))
            {
                widgetClass = reader.attributes().value("class").toString();
            }
            else if (reader.name() == QLatin1String("property"))
            {
                properties.append(reader.attributes().value("name").toString());
            }
            else if (reader.name() == QLatin1String("class"))
            {
                customClass = reader.readElementText();
            }
            else if (reader.name() == QLatin1String("extends"))
            {
                extends = reader.readElementText();
            }
        }
        wellFormed = !reader.hasError();
    }
};

TEST(CustomWidget, DescribesThePlotWidget)
{
    CustomWidget widget(plotWidgetDescription());

    EXPECT_EQ(widget.name(), QString("rocketplot::PlotWidget"));
    EXPECT_EQ(widget.includeFile(), QString("rocketplot/PlotWidget.h"));
    EXPECT_EQ(widget.group(), QString("rocketplot"));
    EXPECT_FALSE(widget.toolTip().isEmpty());
    EXPECT_FALSE(widget.whatsThis().isEmpty());
    EXPECT_FALSE(widget.isContainer());
    EXPECT_FALSE(widget.icon().isNull());
    const std::unique_ptr<QWidget> made(widget.createWidget(nullptr));
    EXPECT_NE(qobject_cast<rocketplot::PlotWidget*>(made.get()), nullptr);
}

TEST(CustomWidget, DescribesThePlotGrid)
{
    CustomWidget widget(plotGridDescription());

    EXPECT_EQ(widget.name(), QString("rocketplot::PlotGrid"));
    EXPECT_EQ(widget.includeFile(), QString("rocketplot/PlotGrid.h"));
    EXPECT_FALSE(widget.icon().isNull());
    const std::unique_ptr<QWidget> made(widget.createWidget(nullptr));
    EXPECT_NE(qobject_cast<rocketplot::PlotGrid*>(made.get()), nullptr);
}

TEST(CustomWidget, TheHeadersAFormIncludesExist)
{
    for (const auto& description : {plotWidgetDescription(), plotGridDescription()})
    {
        const QString header = QStringLiteral(ROCKETPLOT_INCLUDE_DIR "/") + description.includeFile;
        EXPECT_TRUE(QFileInfo::exists(header)) << header.toStdString();
    }
}

// Expects the XML a widget gives Designer to be for the class the widget says it is.
void expectXmlNamesTheClass(const CustomWidget& widget)
{
    const DomXml xml(widget.domXml());

    EXPECT_TRUE(xml.wellFormed) << widget.domXml().toStdString();
    EXPECT_EQ(xml.widgetClass, widget.name());
    EXPECT_EQ(xml.customClass, widget.name());
    EXPECT_EQ(xml.extends, QString("QWidget"));
    EXPECT_TRUE(xml.properties.contains("geometry"));
}

TEST(CustomWidget, TheXmlForDesignerNamesTheClassItMakes)
{
    expectXmlNamesTheClass(CustomWidget(plotWidgetDescription()));
    expectXmlNamesTheClass(CustomWidget(plotGridDescription()));
}

TEST(CustomWidget, EveryPropertyANewWidgetStartsWithIsOneOfItsProperties)
{
    for (const auto& description : {plotWidgetDescription(), plotGridDescription()})
    {
        CustomWidget                   widget(description);
        const std::unique_ptr<QWidget> made(widget.createWidget(nullptr));
        for (const QString& property : DomXml(widget.domXml()).properties)
        {
            EXPECT_GE(made->metaObject()->indexOfProperty(property.toLatin1().constData()), 0)
                << description.name.toStdString() << "." << property.toStdString();
        }
    }
}

TEST(CustomWidget, IsInitializedOnceDesignerHasDoneSo)
{
    CustomWidget widget(plotWidgetDescription());
    EXPECT_FALSE(widget.isInitialized());

    widget.initialize(nullptr);

    EXPECT_TRUE(widget.isInitialized());
}

}  // namespace

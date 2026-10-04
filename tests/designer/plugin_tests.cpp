// The plugin as Qt Designer gets it: loaded from its file.

#include <QList>
#include <QPluginLoader>
#include <QString>
#include <QStringList>
#include <QWidget>
#include <memory>

#include <QtUiPlugin/QDesignerCustomWidgetCollectionInterface>
#include <QtUiPlugin/QDesignerCustomWidgetInterface>
#include <gtest/gtest.h>

namespace
{

TEST(DesignerPlugin, LoadsAndOffersTheWidgets)
{
    QPluginLoader loader(QStringLiteral(ROCKETPLOT_DESIGNER_PLUGIN));
    QObject*      plugin = loader.instance();
    ASSERT_NE(plugin, nullptr) << loader.errorString().toStdString();
    const auto* collection = qobject_cast<QDesignerCustomWidgetCollectionInterface*>(plugin);
    ASSERT_NE(collection, nullptr);

    QStringList names;
    for (QDesignerCustomWidgetInterface* widget : collection->customWidgets())
    {
        names.append(widget->name());
        // A plugin that holds a static rocketplot has classes of its own: tell by the name.
        const std::unique_ptr<QWidget> made(widget->createWidget(nullptr));
        ASSERT_NE(made, nullptr);
        EXPECT_EQ(QString::fromLatin1(made->metaObject()->className()), widget->name());
    }
    EXPECT_EQ(names, (QStringList{"rocketplot::PlotWidget", "rocketplot::PlotGrid"}));
}

TEST(DesignerPlugin, SaysWhatItIsAPluginFor)
{
    const QPluginLoader loader(QStringLiteral(ROCKETPLOT_DESIGNER_PLUGIN));

    EXPECT_EQ(loader.metaData().value("IID").toString(),
              QString("org.qt-project.Qt.QDesignerCustomWidgetCollectionInterface"));
}

}  // namespace

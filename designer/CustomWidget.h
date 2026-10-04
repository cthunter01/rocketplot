#pragma once

#include <QIcon>
#include <QObject>
#include <QString>
#include <functional>

#include <QtUiPlugin/QDesignerCustomWidgetInterface>

class QDesignerFormEditorInterface;
class QWidget;

namespace rocketplot::designer
{

/// What Qt Designer needs to know about one widget to offer it in its widget box.
struct WidgetDescription
{
    QString name;         ///< The class, with its namespace: "rocketplot::PlotWidget"
    QString includeFile;  ///< What a form that uses it includes: "rocketplot/PlotWidget.h"
    QString objectName;   ///< What a new one is called in a form: "plot"
    QString toolTip;
    QString whatsThis;
    /// Properties a new one starts with, as the <property> elements of a .ui file; a size, at
    /// least. They are written to the form, so they hold in the application as in Designer.
    QString                           properties;
    std::function<QWidget*(QWidget*)> create;
    std::function<QIcon()>            icon;
};

/// One of rocketplot's widgets as Qt Designer sees it.
class CustomWidget : public QObject, public QDesignerCustomWidgetInterface
{
    Q_OBJECT
    Q_INTERFACES(QDesignerCustomWidgetInterface)

public:
    explicit CustomWidget(WidgetDescription description, QObject* parent = nullptr);

    [[nodiscard]] QString  name() const override;
    [[nodiscard]] QString  group() const override;
    [[nodiscard]] QString  toolTip() const override;
    [[nodiscard]] QString  whatsThis() const override;
    [[nodiscard]] QString  includeFile() const override;
    [[nodiscard]] QIcon    icon() const override;
    [[nodiscard]] bool     isContainer() const override;
    [[nodiscard]] QWidget* createWidget(QWidget* parent) override;
    [[nodiscard]] bool     isInitialized() const override;
    void                   initialize(QDesignerFormEditorInterface* core) override;
    [[nodiscard]] QString  domXml() const override;

private:
    WidgetDescription m_description;
    bool              m_initialized = false;
};

/// The widgets the plugin offers: PlotWidget and PlotGrid.
[[nodiscard]] WidgetDescription plotWidgetDescription();
[[nodiscard]] WidgetDescription plotGridDescription();

}  // namespace rocketplot::designer

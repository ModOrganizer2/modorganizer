#ifndef SETTINGSDIALOGEXTENSIONINFO_H
#define SETTINGSDIALOGEXTENSIONINFO_H

#include <QWidget>

#include <uibase/extensions/extension.h>

namespace Ui
{
class ExtensionListInfoWidget;
}

class ExtensionManager;
class PluginManager;
class Settings;

class ExtensionSettingWidget : public QWidget
{
  Q_OBJECT
public:
  ExtensionSettingWidget(MOBase::Setting const& setting, QVariant const& value,
                         QWidget* parent = nullptr);

private:
  QVariant m_value;
};

class ExtensionListInfoWidget : public QWidget
{
public:
  ExtensionListInfoWidget(QWidget* parent = nullptr);

  // setup the widget, should be called before any other functions
  //
  void setup(Settings& settings, ExtensionManager& extensionManager,
             PluginManager& pluginManager);

  // set the extension to display
  //
  void setExtension(const MOBase::IExtension& extension);

private:
  Ui::ExtensionListInfoWidget* ui;

  Settings* m_settings;
  ExtensionManager* m_extensionManager;
  PluginManager* m_pluginManager;

  // currently displayed extension (default to nullptr)
  const MOBase::IExtension* m_extension{nullptr};
};

#endif

#ifndef SETTINGSDIALOGEXTENSIONINFO_H
#define SETTINGSDIALOGEXTENSIONINFO_H

#include <QWidget>

#include <uibase/extensions/extension.h>

namespace Ui
{
class ExtensionListInfoWidget;
}

class ExtensionListInfoWidget : public QWidget
{
public:
  ExtensionListInfoWidget(QWidget* parent = nullptr);

  // set the extension to display
  //
  void setExtension(const MOBase::IExtension& extension);

private:
  Ui::ExtensionListInfoWidget* ui;

  // currently displayed extension (default to nullptr)
  const MOBase::IExtension* m_extension{nullptr};
};

#endif

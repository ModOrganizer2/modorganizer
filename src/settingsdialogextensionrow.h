#ifndef SETTINGSDIALOGEXTENSIONROW_H
#define SETTINGSDIALOGEXTENSIONROW_H

#include <QWidget>

#include <uibase/extensions/extension.h>

namespace Ui
{
class ExtensionListItemWidget;
}

class ExtensionListItemWidget : public QWidget
{
public:
  ExtensionListItemWidget(MOBase::IExtension const& extension);

  // retrieve the extension associated with this widget
  //
  const auto& extension() const { return *m_extension; }

private:
  Ui::ExtensionListItemWidget* ui;
  const MOBase::IExtension* m_extension;
};

#endif

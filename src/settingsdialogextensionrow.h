#ifndef SETTINGSDIALOGEXTENSIONROW_H
#define SETTINGSDIALOGEXTENSIONROW_H

#include <QWidget>

#include "extension.h"

namespace Ui
{
class ExtensionListItemWidget;
}

class ExtensionListItemWidget : public QWidget
{
public:
  ExtensionListItemWidget(MOBase::IExtension const& extension);

private:
  Ui::ExtensionListItemWidget* ui;
  const MOBase::IExtension* m_extension;
};

#endif

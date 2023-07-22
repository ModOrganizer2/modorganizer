#include "settingsdialogextensionrow.h"

#include "ui_settingsdialogextensionrow.h"

using namespace MOBase;

ExtensionListItemWidget::ExtensionListItemWidget(const IExtension& extension)
    : ui{new Ui::ExtensionListItemWidget()}, m_extension{&extension}
{
  ui->setupUi(this);

  QIcon icon = style()->standardIcon(QStyle::SP_DialogOkButton);
  ui->extensionIcon->setPixmap(extension.metadata().icon().pixmap(QSize(48, 48)));
  ui->extensionName->setText(extension.metadata().name());

  ui->extensionDescription->setText(extension.metadata().description());
  ui->extensionAuthor->setText(extension.metadata().author().name());
}

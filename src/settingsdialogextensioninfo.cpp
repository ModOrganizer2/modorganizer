#include "settingsdialogextensioninfo.h"

#include "ui_settingsdialogextensioninfo.h"

#include <format>

#include <uibase/formatters.h>

using namespace MOBase;

ExtensionListInfoWidget::ExtensionListInfoWidget(QWidget* parent)
    : QWidget(parent), ui{new Ui::ExtensionListInfoWidget()}
{
  ui->setupUi(this);

  ui->authorLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
  ui->authorLabel->setOpenExternalLinks(true);
}

void ExtensionListInfoWidget::setExtension(const IExtension& extension)
{
  m_extension = &extension;

  const auto& metadata = m_extension->metadata();
  const auto& author   = metadata.author();

  if (author.homepage().isEmpty()) {
    ui->authorLabel->setText(metadata.author().name());
  } else {
    ui->authorLabel->setText(QString::fromStdString(
        std::format("<a href=\"{}\">{}</a>", author.homepage(), author.name())));
  }
  ui->descriptionLabel->setText(metadata.description());
  ui->versionLabel->setText(metadata.version().string(Version::FormatCondensed));

  if (metadata.type() == ExtensionType::THEME ||
      metadata.type() == ExtensionType::TRANSLATION) {
    ui->enabledCheckbox->setChecked(true);
    ui->enabledCheckbox->setEnabled(false);
    ui->enabledCheckbox->setToolTip(
        tr("Translation and theme extensions cannot be disabled."));
  } else {
    // TODO:
    // ui->enabledCheckbox->setChecked();
    ui->enabledCheckbox->setEnabled(true);
    ui->enabledCheckbox->setToolTip(QString());
  }
}

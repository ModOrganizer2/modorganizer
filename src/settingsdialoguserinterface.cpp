#include "settingsdialoguserinterface.h"
#include "ui_settingsdialog.h"
#include "shared/appconfig.h"
#include "categoriesdialog.h"
#include "colortable.h"
#include "modlist.h"
#include <utility.h>
#include <questionboxmemory.h>

using namespace MOBase;

UserInterfaceSettingsTab::UserInterfaceSettingsTab(Settings& s, SettingsDialog& d)
  : SettingsTab(s, d),
  m_columnToBox{
    { ModList::COL_CONFLICTFLAGS, ui->collapsibleSeparatorsIconsConflictsBox },
    { ModList::COL_FLAGS, ui->collapsibleSeparatorsIconsFlagsBox },
    { ModList::COL_CONTENT, ui->collapsibleSeparatorsIconsContentsBox},
    { ModList::COL_VERSION, ui->collapsibleSeparatorsIconsVersionBox }
  }
{

  // connect before setting to trigger
  QObject::connect(ui->collapsibleSeparatorsAscBox, &QCheckBox::toggled, [=] { updateCollapsibleSeparatorsGroup(); });
  QObject::connect(ui->collapsibleSeparatorsDscBox, &QCheckBox::toggled, [=] { updateCollapsibleSeparatorsGroup(); });

  // mod list
  ui->displayForeignBox->setChecked(settings().interface().displayForeign());
  ui->colorSeparatorsBox->setChecked(settings().colors().colorSeparatorScrollbar());
  ui->collapsibleSeparatorsAscBox->setChecked(settings().interface().collapsibleSeparators(Qt::AscendingOrder));
  ui->collapsibleSeparatorsDscBox->setChecked(settings().interface().collapsibleSeparators(Qt::DescendingOrder));
  ui->collapsibleSeparatorsHighlightFromBox->setChecked(settings().interface().collapsibleSeparatorsHighlightFrom());
  ui->collapsibleSeparatorsHighlightToBox->setChecked(settings().interface().collapsibleSeparatorsHighlightTo());
  ui->collapsibleSeparatorsPerProfileBox->setChecked(settings().interface().collapsibleSeparatorsPerProfile());
  ui->saveFiltersBox->setChecked(settings().interface().saveFilters());

  for (auto& p : m_columnToBox) {
    p.second->setChecked(settings().interface().collapsibleSeparatorsIcons(p.first));
  }

  // download list
  ui->compactBox->setChecked(settings().interface().compactDownloads());
  ui->showMetaBox->setChecked(settings().interface().metaDownloads());
  ui->hideDownloadInstallBox->setChecked(settings().interface().hideDownloadsAfterInstallation());

  // colors
  ui->colorTable->load(s);

  QObject::connect(ui->resetColorsBtn, &QPushButton::clicked, [&] { ui->colorTable->resetColors(); });

}

void UserInterfaceSettingsTab::update()
{
  // mod list
  settings().colors().setColorSeparatorScrollbar(ui->colorSeparatorsBox->isChecked());
  settings().interface().setDisplayForeign(ui->displayForeignBox->isChecked());
  settings().interface().setCollapsibleSeparators(
    ui->collapsibleSeparatorsAscBox->isChecked(), ui->collapsibleSeparatorsDscBox->isChecked());
  settings().interface().setCollapsibleSeparatorsHighlightFrom(ui->collapsibleSeparatorsHighlightFromBox->isChecked());
  settings().interface().setCollapsibleSeparatorsHighlightTo(ui->collapsibleSeparatorsHighlightToBox->isChecked());
  settings().interface().setCollapsibleSeparatorsPerProfile(ui->collapsibleSeparatorsPerProfileBox->isChecked());
  settings().interface().setSaveFilters(ui->saveFiltersBox->isChecked());

  for (auto& p : m_columnToBox) {
    settings().interface().setCollapsibleSeparatorsIcons(p.first, p.second->isChecked());
  }

  // download list
  settings().interface().setCompactDownloads(ui->compactBox->isChecked());
  settings().interface().setMetaDownloads(ui->showMetaBox->isChecked());
  settings().interface().setHideDownloadsAfterInstallation(ui->hideDownloadInstallBox->isChecked());

  // colors
  ui->colorTable->commitColors();
}

void UserInterfaceSettingsTab::updateCollapsibleSeparatorsGroup()
{
  const auto checked = ui->collapsibleSeparatorsAscBox->isChecked() ||
    ui->collapsibleSeparatorsDscBox->isChecked();
  for (auto* widget : ui->collapsibleSeparatorsWidget->findChildren<QWidget*>()) {
    widget->setEnabled(checked);
  }
  ui->collapsibleSeparatorsLabel->setEnabled(true);
  ui->collapsibleSeparatorsAscBox->setEnabled(true);
  ui->collapsibleSeparatorsDscBox->setEnabled(true);
}

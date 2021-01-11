#include "settingsdialoguserinterface.h"
#include "ui_settingsdialog.h"
#include "shared/appconfig.h"
#include "categoriesdialog.h"
#include "colortable.h"
#include <utility.h>
#include <questionboxmemory.h>

using namespace MOBase;

UserInterfaceSettingsTab::UserInterfaceSettingsTab(Settings& s, SettingsDialog& d)
  : SettingsTab(s, d)
{

  // connect before setting to trigger
  QObject::connect(ui->collapsibleSeparatorsBox, &QGroupBox::toggled, [=](auto&& on) {
    ui->collapsibleSeparatorsConflictsBox->setEnabled(on);
    ui->collapsibleSeparatorsPerProfileBox->setEnabled(on);
  });

  // mod list
  ui->displayForeignBox->setChecked(settings().interface().displayForeign());
  ui->colorSeparatorsBox->setChecked(settings().colors().colorSeparatorScrollbar());
  ui->collapsibleSeparatorsConflictsBox->setChecked(settings().interface().collapsibleSeparatorsConflicts());
  ui->collapsibleSeparatorsBox->setChecked(settings().interface().collapsibleSeparators());
  ui->collapsibleSeparatorsPerProfileBox->setChecked(settings().interface().collapsibleSeparatorsPerProfile());
  ui->saveFiltersBox->setChecked(settings().interface().saveFilters());

  // download list
  ui->compactBox->setChecked(settings().interface().compactDownloads());
  ui->showMetaBox->setChecked(settings().interface().metaDownloads());

  // colors
  ui->colorTable->load(s);

  QObject::connect(ui->resetColorsBtn, &QPushButton::clicked, [&] { ui->colorTable->resetColors(); });

}

void UserInterfaceSettingsTab::update()
{
  // mod list
  settings().colors().setColorSeparatorScrollbar(ui->colorSeparatorsBox->isChecked());
  settings().interface().setDisplayForeign(ui->displayForeignBox->isChecked());
  settings().interface().setCollapsibleSeparators(ui->collapsibleSeparatorsBox->isChecked());
  settings().interface().setCollapsibleSeparatorsConflicts(ui->collapsibleSeparatorsConflictsBox->isChecked());
  settings().interface().setCollapsibleSeparatorsPerProfile(ui->collapsibleSeparatorsPerProfileBox->isChecked());
  settings().interface().setSaveFilters(ui->saveFiltersBox->isChecked());

  // download list
  settings().interface().setCompactDownloads(ui->compactBox->isChecked());
  settings().interface().setMetaDownloads(ui->showMetaBox->isChecked());

  // colors
  ui->colorTable->commitColors();
}

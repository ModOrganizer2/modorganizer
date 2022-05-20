#include "settingsdialogtheme.h"
#include "categoriesdialog.h"
#include "colortable.h"
#include "modlist.h"
#include "shared/appconfig.h"
#include "ui_settingsdialog.h"
#include <questionboxmemory.h>
#include <utility.h>

using namespace MOBase;

ThemeSettingsTab::ThemeSettingsTab(Settings& s, ThemeManager const& manager,
                                   SettingsDialog& d)
    : SettingsTab(s, d)
{
  // style
  addStyles(manager);
  selectStyle();

  // colors
  ui->colorTable->load(s);

  QObject::connect(ui->resetColorsBtn, &QPushButton::clicked, [&] {
    ui->colorTable->resetColors();
  });
}

void ThemeSettingsTab::update()
{
  // style
  const QString oldStyle = settings().interface().themeName().value_or("");
  const QString newStyle =
      ui->styleBox->itemData(ui->styleBox->currentIndex()).toString();

  if (oldStyle != newStyle) {
    settings().interface().setThemeName(newStyle);
    emit settings().themeChanged(newStyle);
  }

  // colors
  ui->colorTable->commitColors();
}

void ThemeSettingsTab::addStyles(ThemeManager const& manager)
{
  ui->styleBox->addItem("None", "");

  auto themes = manager.themes();

  std::sort(themes.begin(), themes.end(), [&manager](auto&& lhs, auto&& rhs) {
    if (manager.isBuiltIn(lhs) == manager.isBuiltIn(rhs)) {
      return lhs->name() < rhs->name();
    } else {
      // put built-in before others
      return manager.isBuiltIn(rhs) < manager.isBuiltIn(lhs);
    }
  });

  bool separator = true;
  for (auto&& theme : themes) {
    if (separator && !manager.isBuiltIn(theme)) {
      ui->styleBox->insertSeparator(ui->styleBox->count());
      separator = false;
    }

    ui->styleBox->addItem(ToQString(theme->identifier()), ToQString(theme->name()));
  }
}

void ThemeSettingsTab::selectStyle()
{
  const int currentID =
      ui->styleBox->findData(settings().interface().themeName().value_or(""));

  if (currentID != -1) {
    ui->styleBox->setCurrentIndex(currentID);
  }
}

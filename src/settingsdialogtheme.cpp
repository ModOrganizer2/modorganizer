#include "settingsdialogtheme.h"
#include "categoriesdialog.h"
#include "colortable.h"
#include "modlist.h"
#include "shared/appconfig.h"
#include "ui_settingsdialog.h"
#include <questionboxmemory.h>
#include <utility.h>

using namespace MOBase;

ThemeSettingsTab::ThemeSettingsTab(Settings& s, SettingsDialog& d) : SettingsTab(s, d)
{
  // style
  addStyles();
  selectStyle();

  // colors
  ui->colorTable->load(s);

  QObject::connect(ui->resetColorsBtn, &QPushButton::clicked, [&] {
    ui->colorTable->resetColors();
  });

  QObject::connect(ui->exploreStyles, &QPushButton::clicked, [&] {
    onExploreStyles();
  });
}

void ThemeSettingsTab::update()
{
  // style
  const QString oldStyle = settings().interface().styleName().value_or("");
  const QString newStyle =
      ui->styleBox->itemData(ui->styleBox->currentIndex()).toString();

  if (oldStyle != newStyle) {
    settings().interface().setStyleName(newStyle);
    emit settings().styleChanged(newStyle);
  }

  // colors
  ui->colorTable->commitColors();
}

void ThemeSettingsTab::addStyles()
{
  ui->styleBox->addItem("None", "");
  for (auto&& key : QStyleFactory::keys()) {
    ui->styleBox->addItem(key, key);
  }

  ui->styleBox->insertSeparator(ui->styleBox->count());

  QDirIterator iter(QCoreApplication::applicationDirPath() + "/" +
                        QString::fromStdWString(AppConfig::stylesheetsPath()),
                    QStringList("*.qss"), QDir::Files);

  while (iter.hasNext()) {
    iter.next();

    ui->styleBox->addItem(iter.fileInfo().completeBaseName(), iter.fileName());
  }
}

void ThemeSettingsTab::selectStyle()
{
  const int currentID =
      ui->styleBox->findData(settings().interface().styleName().value_or(""));

  if (currentID != -1) {
    ui->styleBox->setCurrentIndex(currentID);
  }
}

void ThemeSettingsTab::onExploreStyles()
{
  QString ssPath = QCoreApplication::applicationDirPath() + "/" +
                   ToQString(AppConfig::stylesheetsPath());
  shell::Explore(ssPath);
}

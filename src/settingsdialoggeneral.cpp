#include "settingsdialoggeneral.h"
#include "ui_settingsdialog.h"
#include "appconfig.h"
#include "categoriesdialog.h"
#include <questionboxmemory.h>

using MOBase::QuestionBoxMemory;


class ColorItem : public QTableWidgetItem
{
public:
  ColorItem(
    const QColor& defaultColor,
    std::function<QColor ()> get,
    std::function<void (const QColor&)> commit)
     : m_default(defaultColor), m_get(get), m_commit(commit)
  {
    set(get());
  }

  QColor get() const
  {
    return m_temp;
  }

  bool set(const QColor& c)
  {
    if (m_temp != c) {
      m_temp = c;
      return true;
    }

    return false;
  }

  void commit()
  {
    m_commit(m_temp);
  }

  bool reset()
  {
    return set(m_default);
  }

private:
  const QColor m_default;
  std::function<QColor ()> m_get;
  std::function<void (const QColor&)> m_commit;
  QColor m_temp;
};


class ColorDelegate : public QStyledItemDelegate
{
public:
  ColorDelegate(QTableWidget* table)
    : m_table(table)
  {
  }

protected:
  void paint(
    QPainter* p, const QStyleOptionViewItem& option,
    const QModelIndex& index) const override
  {
    if (!paintColor(p, option, index)) {
      QStyledItemDelegate::paint(p, option, index);
    }
  }

private:
  QTableWidget* m_table;

  bool paintColor(
    QPainter* p, const QStyleOptionViewItem& option,
    const QModelIndex& index) const
  {
    if (index.column() != 1) {
      return false;
    }

    const auto* item = dynamic_cast<ColorItem*>(
      m_table->item(index.row(), index.column()));

    if (!item) {
      return false;
    }

    p->save();
    p->fillRect(option.rect, item->get());
    p->restore();

    return true;
  }
};


template <class F>
void forEachColorItem(QTableWidget* table, F&& f)
{
  const auto rowCount = table->rowCount();

  for (int i=0; i<rowCount; ++i) {
    if (auto* item=dynamic_cast<ColorItem*>(table->item(i, 1))) {
      f(item);
    }
  }
}


GeneralSettingsTab::GeneralSettingsTab(Settings& s, SettingsDialog& d)
  : SettingsTab(s, d)
{
  addLanguages();
  selectLanguage();

  addStyles();
  selectStyle();

  setColorTable();

  QObject::connect(ui->categoriesBtn, &QPushButton::clicked, [&]{ on_categoriesBtn_clicked(); });
  QObject::connect(ui->resetColorsBtn, &QPushButton::clicked, [&]{ on_resetColorsBtn_clicked(); });
  QObject::connect(ui->resetDialogsButton, &QPushButton::clicked, [&]{ on_resetDialogsButton_clicked(); });
}

void GeneralSettingsTab::update()
{
  const QString oldLanguage = settings().interface().language();
  const QString newLanguage = ui->languageBox->itemData(ui->languageBox->currentIndex()).toString();

  if (newLanguage != oldLanguage) {
    settings().interface().setLanguage(newLanguage);
    emit settings().languageChanged(newLanguage);
  }

  const QString oldStyle = settings().interface().styleName().value_or("");
  const QString newStyle = ui->styleBox->itemData(ui->styleBox->currentIndex()).toString();
  if (oldStyle != newStyle) {
    settings().interface().setStyleName(newStyle);
    emit settings().styleChanged(newStyle);
  }

  settings().geometry().setCenterDialogs(ui->centerDialogs->isChecked());
  settings().interface().setCompactDownloads(ui->compactBox->isChecked());
  settings().interface().setMetaDownloads(ui->showMetaBox->isChecked());
  settings().setCheckForUpdates(ui->checkForUpdates->isChecked());
  settings().setUsePrereleases(ui->usePrereleaseBox->isChecked());

  forEachColorItem(ui->colorTable, [](auto* item) {
    item->commit();
  });

  settings().colors().setColorSeparatorScrollbar(ui->colorSeparatorsBox->isChecked());
}

void GeneralSettingsTab::addLanguages()
{
  std::vector<std::pair<QString, QString>> languages;

  QDirIterator langIter(QCoreApplication::applicationDirPath() + "/translations", QDir::Files);
  QString pattern = QString::fromStdWString(AppConfig::translationPrefix()) +  "_([a-z]{2,3}(_[A-Z]{2,2})?).qm";
  QRegExp exp(pattern);
  while (langIter.hasNext()) {
    langIter.next();
    QString file = langIter.fileName();
    if (exp.exactMatch(file)) {
      QString languageCode = exp.cap(1);
      QLocale locale(languageCode);
      QString languageString = QString("%1 (%2)").arg(locale.nativeLanguageName()).arg(locale.nativeCountryName());  //QLocale::languageToString(locale.language());
      if (locale.language() == QLocale::Chinese) {
        if (languageCode == "zh_TW") {
          languageString = "Chinese (traditional)";
        } else {
          languageString = "Chinese (simplified)";
        }
      }
      languages.push_back(std::make_pair(QString("%1").arg(languageString), exp.cap(1)));
    }
  }
  if (!ui->languageBox->findText("English")) {
    languages.push_back(std::make_pair(QString("English"), QString("en_US")));
  }
  std::sort(languages.begin(), languages.end());
  for (const auto &lang : languages) {
    ui->languageBox->addItem(lang.first, lang.second);
  }
}

void GeneralSettingsTab::selectLanguage()
{
  QString languageCode = settings().interface().language();
  int currentID        = ui->languageBox->findData(languageCode);
  // I made a mess. :( Most languages are stored with only the iso country
  // code (2 characters like "de") but chinese
  // with the exact language variant (zh_TW) so I have to search for both
  // variants
  if (currentID == -1) {
    currentID = ui->languageBox->findData(languageCode.mid(0, 2));
  }
  if (currentID != -1) {
    ui->languageBox->setCurrentIndex(currentID);
  }
}

void GeneralSettingsTab::addStyles()
{
  ui->styleBox->addItem("None", "");
  for (auto&& key : QStyleFactory::keys()) {
    ui->styleBox->addItem(key, key);
  }

  ui->styleBox->insertSeparator(ui->styleBox->count());

  QDirIterator iter(
    QCoreApplication::applicationDirPath() + "/" +
      QString::fromStdWString(AppConfig::stylesheetsPath()),
        QStringList("*.qss"),
    QDir::Files);

  while (iter.hasNext()) {
    iter.next();

    ui->styleBox->addItem(
      iter.fileInfo().completeBaseName(),
      iter.fileName());
  }
}

void GeneralSettingsTab::selectStyle()
{
  const int currentID = ui->styleBox->findData(
    settings().interface().styleName().value_or(""));

  if (currentID != -1) {
    ui->styleBox->setCurrentIndex(currentID);
  }
}

void GeneralSettingsTab::setColorTable()
{
  ui->colorTable->setColumnCount(2);
  ui->colorTable->setHorizontalHeaderLabels({
    QObject::tr("Item"), QObject::tr("Color")
    });

  ui->colorTable->setItemDelegate(new ColorDelegate(ui->colorTable));

  addColor(
    QObject::tr("Is overwritten (loose files)"),
    QColor(0, 255, 0, 64),
    [this]{ return settings().colors().modlistOverwrittenLoose(); },
    [this](auto&& v){ settings().colors().setModlistOverwrittenLoose(v); });

  addColor(
    QObject::tr("Is overwriting (loose files)"),
    QColor(255, 0, 0, 64),
    [this]{ return settings().colors().modlistOverwritingLoose(); },
    [this](auto&& v){ settings().colors().setModlistOverwritingLoose(v); });

  addColor(
    QObject::tr("Is overwritten (archives)"),
    QColor(0, 255, 255, 64),
    [this]{ return settings().colors().modlistOverwrittenArchive(); },
    [this](auto&& v){ settings().colors().setModlistOverwrittenArchive(v); });

  addColor(
    QObject::tr("Is overwriting (archives)"),
    QColor(255, 0, 255, 64),
    [this]{ return settings().colors().modlistOverwritingArchive(); },
    [this](auto&& v){ settings().colors().setModlistOverwritingArchive(v); });

  addColor(
    QObject::tr("Mod contains selected plugin"),
    QColor(0, 0, 255, 64),
    [this]{ return settings().colors().modlistContainsPlugin(); },
    [this](auto&& v){ settings().colors().setModlistContainsPlugin(v); });

  addColor(
    QObject::tr("Plugin is contained in selected mod"),
    QColor(0, 0, 255, 64),
    [this]{ return settings().colors().pluginListContained(); },
    [this](auto&& v){ settings().colors().setPluginListContained(v); });

  QObject::connect(
    ui->colorTable, &QTableWidget::cellActivated,
    [&]{ onColorActivated(); });
}

void GeneralSettingsTab::addColor(
  const QString& text, const QColor& defaultColor,
  std::function<QColor ()> get,
  std::function<void (const QColor&)> commit)
{
  const auto r = ui->colorTable->rowCount();
  ui->colorTable->setRowCount(r + 1);

  ui->colorTable->setItem(r, 0, new QTableWidgetItem(text));
  ui->colorTable->setItem(r, 1, new ColorItem(defaultColor, get, commit));

  ui->colorTable->resizeColumnsToContents();
}

void GeneralSettingsTab::resetDialogs()
{
  settings().widgets().resetQuestionButtons();
}

void GeneralSettingsTab::onColorActivated()
{
  const auto rows = ui->colorTable->selectionModel()->selectedRows();
  if (rows.isEmpty()) {
    return;
  }

  const auto row = rows[0].row();

  const auto text = ui->colorTable->item(row, 0)->text();
  auto* item = dynamic_cast<ColorItem*>(ui->colorTable->item(row, 1));

  if (!item) {
    return;
  }

  const QColor result = QColorDialog::getColor(
    item->get(), &dialog(), text, QColorDialog::ShowAlphaChannel);

  if (result.isValid()) {
    item->set(result);
    ui->colorTable->update(ui->colorTable->model()->index(row, 1));
  }
}

void GeneralSettingsTab::on_resetColorsBtn_clicked()
{
  bool changed = false;

  forEachColorItem(ui->colorTable, [&](auto* item) {
    if (item->reset()) {
      changed = true;
    }
  });

  if (changed) {
    ui->colorTable->update();
  }
}

void GeneralSettingsTab::on_resetDialogsButton_clicked()
{
  const auto r = QMessageBox::question(
    parentWidget(),
    QObject::tr("Confirm?"),
    QObject::tr(
      "This will reset all the choices you made to dialogs and make them all "
      "visible again. Continue?"),
    QMessageBox::Yes | QMessageBox::No);

  if (r == QMessageBox::Yes) {
    resetDialogs();
  }
}

void GeneralSettingsTab::on_categoriesBtn_clicked()
{
  CategoriesDialog dialog(&dialog());

  if (dialog.exec() == QDialog::Accepted) {
    dialog.commitChanges();
  }
}

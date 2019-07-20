#include "settingsdialogpaths.h"
#include "ui_settingsdialog.h"
#include "appconfig.h"
#include <iplugingame.h>

PathsTab::PathsTab(Settings *parent, SettingsDialog &dialog)
  : SettingsTab(parent, dialog)
{
  ui->baseDirEdit->setText(m_parent->getBaseDirectory());
  ui->managedGameDirEdit->setText(m_parent->gamePlugin()->gameDirectory().absoluteFilePath(m_parent->gamePlugin()->binaryName()));
  QString basePath = parent->getBaseDirectory();
  QDir baseDir(basePath);
  for (const auto &dir : {
    std::make_pair(ui->downloadDirEdit, m_parent->getDownloadDirectory(false)),
    std::make_pair(ui->modDirEdit, m_parent->getModDirectory(false)),
    std::make_pair(ui->cacheDirEdit, m_parent->getCacheDirectory(false)),
    std::make_pair(ui->profilesDirEdit, m_parent->getProfileDirectory(false)),
    std::make_pair(ui->overwriteDirEdit, m_parent->getOverwriteDirectory(false))
    }) {
    QString storePath = baseDir.relativeFilePath(dir.second);
    storePath = dir.second;
    dir.first->setText(storePath);
  }

  QObject::connect(ui->browseBaseDirBtn, &QPushButton::clicked, [&]{ on_browseBaseDirBtn_clicked(); });
  QObject::connect(ui->browseCacheDirBtn, &QPushButton::clicked, [&]{ on_browseCacheDirBtn_clicked(); });
  QObject::connect(ui->browseDownloadDirBtn, &QPushButton::clicked, [&]{ on_browseDownloadDirBtn_clicked(); });
  QObject::connect(ui->browseGameDirBtn, &QPushButton::clicked, [&]{ on_browseGameDirBtn_clicked(); });
  QObject::connect(ui->browseModDirBtn, &QPushButton::clicked, [&]{ on_browseModDirBtn_clicked(); });
  QObject::connect(ui->browseOverwriteDirBtn, &QPushButton::clicked, [&]{ on_browseOverwriteDirBtn_clicked(); });
  QObject::connect(ui->browseProfilesDirBtn, &QPushButton::clicked, [&]{ on_browseProfilesDirBtn_clicked(); });

  QObject::connect(ui->baseDirEdit, &QLineEdit::editingFinished, [&]{ on_baseDirEdit_editingFinished(); });
  QObject::connect(ui->cacheDirEdit, &QLineEdit::editingFinished, [&]{ on_cacheDirEdit_editingFinished(); });
  QObject::connect(ui->downloadDirEdit, &QLineEdit::editingFinished, [&]{ on_downloadDirEdit_editingFinished(); });
  QObject::connect(ui->modDirEdit, &QLineEdit::editingFinished, [&]{ on_modDirEdit_editingFinished(); });
  QObject::connect(ui->overwriteDirEdit, &QLineEdit::editingFinished, [&]{ on_overwriteDirEdit_editingFinished(); });
  QObject::connect(ui->profilesDirEdit, &QLineEdit::editingFinished, [&]{ on_profilesDirEdit_editingFinished(); });
}

void PathsTab::update()
{
  typedef std::tuple<QString, QString, std::wstring> Directory;

  QString basePath = m_parent->getBaseDirectory();

  for (const Directory &dir :{
    Directory{ui->downloadDirEdit->text(), "download_directory", AppConfig::downloadPath()},
    Directory{ui->cacheDirEdit->text(), "cache_directory", AppConfig::cachePath()},
    Directory{ui->modDirEdit->text(), "mod_directory", AppConfig::modsPath()},
    Directory{ui->overwriteDirEdit->text(), "overwrite_directory", AppConfig::overwritePath()},
    Directory{ui->profilesDirEdit->text(), "profiles_directory", AppConfig::profilesPath()}
    }) {
    QString path, settingsKey;
    std::wstring defaultName;
    std::tie(path, settingsKey, defaultName) = dir;

    settingsKey = QString("Settings/%1").arg(settingsKey);

    QString realPath = path;
    realPath.replace("%BASE_DIR%", ui->baseDirEdit->text());

    if (!QDir(realPath).exists()) {
      if (!QDir().mkpath(realPath)) {
        QMessageBox::warning(qApp->activeWindow(), QObject::tr("Error"),
          QObject::tr("Failed to create \"%1\", you may not have the "
            "necessary permission. path remains unchanged.")
          .arg(realPath));
      }
    }

    if (QFileInfo(realPath)
      != QFileInfo(basePath + "/" + QString::fromStdWString(defaultName))) {
      m_Settings.setValue(settingsKey, path);
    } else {
      m_Settings.remove(settingsKey);
    }
  }

  if (QFileInfo(ui->baseDirEdit->text()) !=
    QFileInfo(qApp->property("dataPath").toString())) {
    m_Settings.setValue("Settings/base_directory", ui->baseDirEdit->text());
  } else {
    m_Settings.remove("Settings/base_directory");
  }

  QFileInfo oldGameExe(m_parent->gamePlugin()->gameDirectory().absoluteFilePath(m_parent->gamePlugin()->binaryName()));
  QFileInfo newGameExe(ui->managedGameDirEdit->text());
  if (oldGameExe != newGameExe) {
    m_Settings.setValue("gamePath", newGameExe.absolutePath());
  }
}

void PathsTab::on_browseBaseDirBtn_clicked()
{
  QString temp = QFileDialog::getExistingDirectory(
    parentWidget(), QObject::tr("Select base directory"), ui->baseDirEdit->text());
  if (!temp.isEmpty()) {
    ui->baseDirEdit->setText(temp);
  }
}

void PathsTab::on_browseDownloadDirBtn_clicked()
{
  QString searchPath = ui->downloadDirEdit->text();
  searchPath.replace("%BASE_DIR%", ui->baseDirEdit->text());

  QString temp = QFileDialog::getExistingDirectory(parentWidget(), QObject::tr("Select download directory"), searchPath);
  if (!temp.isEmpty()) {
    ui->downloadDirEdit->setText(temp);
  }
}

void PathsTab::on_browseModDirBtn_clicked()
{
  QString searchPath = ui->modDirEdit->text();
  searchPath.replace("%BASE_DIR%", ui->baseDirEdit->text());

  QString temp = QFileDialog::getExistingDirectory(parentWidget(), QObject::tr("Select mod directory"), searchPath);
  if (!temp.isEmpty()) {
    ui->modDirEdit->setText(temp);
  }
}

void PathsTab::on_browseCacheDirBtn_clicked()
{
  QString searchPath = ui->cacheDirEdit->text();
  searchPath.replace("%BASE_DIR%", ui->baseDirEdit->text());

  QString temp = QFileDialog::getExistingDirectory(parentWidget(), QObject::tr("Select cache directory"), searchPath);
  if (!temp.isEmpty()) {
    ui->cacheDirEdit->setText(temp);
  }
}

void PathsTab::on_browseProfilesDirBtn_clicked()
{
  QString searchPath = ui->profilesDirEdit->text();
  searchPath.replace("%BASE_DIR%", ui->baseDirEdit->text());

  QString temp = QFileDialog::getExistingDirectory(parentWidget(), QObject::tr("Select profiles directory"), searchPath);
  if (!temp.isEmpty()) {
    ui->profilesDirEdit->setText(temp);
  }
}

void PathsTab::on_browseOverwriteDirBtn_clicked()
{
  QString searchPath = ui->overwriteDirEdit->text();
  searchPath.replace("%BASE_DIR%", ui->baseDirEdit->text());

  QString temp = QFileDialog::getExistingDirectory(parentWidget(), QObject::tr("Select overwrite directory"), searchPath);
  if (!temp.isEmpty()) {
    ui->overwriteDirEdit->setText(temp);
  }
}

void PathsTab::on_browseGameDirBtn_clicked()
{
  QFileInfo oldGameExe(ui->managedGameDirEdit->text());

  QString temp = QFileDialog::getOpenFileName(parentWidget(), QObject::tr("Select game executable"), oldGameExe.absolutePath(), oldGameExe.fileName());
  if (!temp.isEmpty()) {
    ui->managedGameDirEdit->setText(temp);
  }
}

void PathsTab::on_baseDirEdit_editingFinished()
{
  normalizePath(ui->baseDirEdit);
}

void PathsTab::on_downloadDirEdit_editingFinished()
{
  normalizePath(ui->downloadDirEdit);
}

void PathsTab::on_modDirEdit_editingFinished()
{
  normalizePath(ui->modDirEdit);
}

void PathsTab::on_cacheDirEdit_editingFinished()
{
  normalizePath(ui->cacheDirEdit);
}

void PathsTab::on_profilesDirEdit_editingFinished()
{
  normalizePath(ui->profilesDirEdit);
}

void PathsTab::on_overwriteDirEdit_editingFinished()
{
  normalizePath(ui->overwriteDirEdit);
}

void PathsTab::normalizePath(QLineEdit *lineEdit)
{
  QString text = lineEdit->text();
  while (text.endsWith('/') || text.endsWith('\\')) {
    text.chop(1);
  }
  lineEdit->setText(text);
}

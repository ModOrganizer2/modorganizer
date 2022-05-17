#include "settingsdialogpaths.h"
#include "shared/appconfig.h"
#include "ui_settingsdialog.h"
#include <iplugingame.h>

PathsSettingsTab::PathsSettingsTab(Settings& s, SettingsDialog& d)
    : SettingsTab(s, d), m_gameDir(settings().game().plugin()->gameDirectory())
{
  ui->baseDirEdit->setText(settings().paths().base());

  ui->managedGameDirEdit->setText(QDir::toNativeSeparators(
      m_gameDir.absoluteFilePath(settings().game().plugin()->binaryName())));

  QString basePath = settings().paths().base();
  QDir baseDir(basePath);

  for (const auto& dir :
       {std::make_pair(ui->downloadDirEdit, settings().paths().downloads(false)),
        std::make_pair(ui->modDirEdit, settings().paths().mods(false)),
        std::make_pair(ui->cacheDirEdit, settings().paths().cache(false)),
        std::make_pair(ui->profilesDirEdit, settings().paths().profiles(false)),
        std::make_pair(ui->overwriteDirEdit, settings().paths().overwrite(false))}) {

    QString storePath = baseDir.relativeFilePath(dir.second);
    storePath         = dir.second;
    dir.first->setText(storePath);
  }

  QObject::connect(ui->browseBaseDirBtn, &QPushButton::clicked, [&] {
    on_browseBaseDirBtn_clicked();
  });
  QObject::connect(ui->browseCacheDirBtn, &QPushButton::clicked, [&] {
    on_browseCacheDirBtn_clicked();
  });
  QObject::connect(ui->browseDownloadDirBtn, &QPushButton::clicked, [&] {
    on_browseDownloadDirBtn_clicked();
  });
  QObject::connect(ui->browseGameDirBtn, &QPushButton::clicked, [&] {
    on_browseGameDirBtn_clicked();
  });
  QObject::connect(ui->browseModDirBtn, &QPushButton::clicked, [&] {
    on_browseModDirBtn_clicked();
  });
  QObject::connect(ui->browseOverwriteDirBtn, &QPushButton::clicked, [&] {
    on_browseOverwriteDirBtn_clicked();
  });
  QObject::connect(ui->browseProfilesDirBtn, &QPushButton::clicked, [&] {
    on_browseProfilesDirBtn_clicked();
  });

  QObject::connect(ui->baseDirEdit, &QLineEdit::editingFinished, [&] {
    on_baseDirEdit_editingFinished();
  });
  QObject::connect(ui->cacheDirEdit, &QLineEdit::editingFinished, [&] {
    on_cacheDirEdit_editingFinished();
  });
  QObject::connect(ui->downloadDirEdit, &QLineEdit::editingFinished, [&] {
    on_downloadDirEdit_editingFinished();
  });
  QObject::connect(ui->modDirEdit, &QLineEdit::editingFinished, [&] {
    on_modDirEdit_editingFinished();
  });
  QObject::connect(ui->overwriteDirEdit, &QLineEdit::editingFinished, [&] {
    on_overwriteDirEdit_editingFinished();
  });
  QObject::connect(ui->profilesDirEdit, &QLineEdit::editingFinished, [&] {
    on_profilesDirEdit_editingFinished();
  });
}

void PathsSettingsTab::update()
{
  using Setter    = void (PathSettings::*)(const QString&);
  using Directory = std::tuple<QString, Setter, std::wstring>;

  QString basePath = settings().paths().base();

  for (const Directory& dir :
       {Directory{ui->downloadDirEdit->text(), &PathSettings::setDownloads,
                  AppConfig::downloadPath()},
        Directory{ui->cacheDirEdit->text(), &PathSettings::setCache,
                  AppConfig::cachePath()},
        Directory{ui->modDirEdit->text(), &PathSettings::setMods,
                  AppConfig::modsPath()},
        Directory{ui->overwriteDirEdit->text(), &PathSettings::setOverwrite,
                  AppConfig::overwritePath()},
        Directory{ui->profilesDirEdit->text(), &PathSettings::setProfiles,
                  AppConfig::profilesPath()}}) {
    QString path;
    Setter setter;
    std::wstring defaultName;
    std::tie(path, setter, defaultName) = dir;

    QString realPath = path;
    realPath         = PathSettings::resolve(realPath, ui->baseDirEdit->text());

    if (!QDir(realPath).exists()) {
      if (!QDir().mkpath(realPath)) {
        QMessageBox::warning(
            parentWidget(), QObject::tr("Error"),
            QObject::tr("Failed to create \"%1\", you may not have the "
                        "necessary permissions. Path remains unchanged.")
                .arg(realPath));

        continue;
      }
    }

    if (QFileInfo(realPath) !=
        QFileInfo(basePath + "/" + QString::fromStdWString(defaultName))) {
      (settings().paths().*setter)(path);
    } else {
      (settings().paths().*setter)("");
    }
  }

  if (QFileInfo(ui->baseDirEdit->text()) !=
      QFileInfo(qApp->property("dataPath").toString())) {
    settings().paths().setBase(ui->baseDirEdit->text());
  } else {
    settings().paths().setBase("");
  }

  if (m_gameDir != settings().game().plugin()->gameDirectory()) {
    settings().game().setDirectory(m_gameDir.absolutePath());
  }
}

void PathsSettingsTab::on_browseBaseDirBtn_clicked()
{
  QString temp = QFileDialog::getExistingDirectory(
      &dialog(), QObject::tr("Select base directory"), ui->baseDirEdit->text());
  if (!temp.isEmpty()) {
    ui->baseDirEdit->setText(temp);
  }
}

void PathsSettingsTab::on_browseDownloadDirBtn_clicked()
{
  QString searchPath = ui->downloadDirEdit->text();
  searchPath         = PathSettings::resolve(searchPath, ui->baseDirEdit->text());

  QString temp = QFileDialog::getExistingDirectory(
      &dialog(), QObject::tr("Select download directory"), searchPath);
  if (!temp.isEmpty()) {
    ui->downloadDirEdit->setText(temp);
  }
}

void PathsSettingsTab::on_browseModDirBtn_clicked()
{
  QString searchPath = ui->modDirEdit->text();
  searchPath         = PathSettings::resolve(searchPath, ui->baseDirEdit->text());

  QString temp = QFileDialog::getExistingDirectory(
      &dialog(), QObject::tr("Select mod directory"), searchPath);
  if (!temp.isEmpty()) {
    ui->modDirEdit->setText(temp);
  }
}

void PathsSettingsTab::on_browseCacheDirBtn_clicked()
{
  QString searchPath = ui->cacheDirEdit->text();
  searchPath         = PathSettings::resolve(searchPath, ui->baseDirEdit->text());

  QString temp = QFileDialog::getExistingDirectory(
      &dialog(), QObject::tr("Select cache directory"), searchPath);
  if (!temp.isEmpty()) {
    ui->cacheDirEdit->setText(temp);
  }
}

void PathsSettingsTab::on_browseProfilesDirBtn_clicked()
{
  QString searchPath = ui->profilesDirEdit->text();
  searchPath         = PathSettings::resolve(searchPath, ui->baseDirEdit->text());

  QString temp = QFileDialog::getExistingDirectory(
      &dialog(), QObject::tr("Select profiles directory"), searchPath);
  if (!temp.isEmpty()) {
    ui->profilesDirEdit->setText(temp);
  }
}

void PathsSettingsTab::on_browseOverwriteDirBtn_clicked()
{
  QString searchPath = ui->overwriteDirEdit->text();
  searchPath         = PathSettings::resolve(searchPath, ui->baseDirEdit->text());

  QString temp = QFileDialog::getExistingDirectory(
      &dialog(), QObject::tr("Select overwrite directory"), searchPath);
  if (!temp.isEmpty()) {
    ui->overwriteDirEdit->setText(temp);
  }
}

void PathsSettingsTab::on_browseGameDirBtn_clicked()
{
  QFileInfo oldGameExe(ui->managedGameDirEdit->text());

  QString temp =
      QFileDialog::getOpenFileName(&dialog(), QObject::tr("Select game executable"),
                                   oldGameExe.absolutePath(), oldGameExe.fileName());

  if (temp.isEmpty()) {
    return;
  }

  // we need to find the game folder corresponding to the executable
  //
  // some game plugins have executable in subfolder, e.g. bin/game.exe,
  // so we need to go up the parent folders until the concatenation of
  // the folder and the binary path equals the game executable specified
  // by the user
  //
  QFileInfo newExe(temp);
  const auto binaryPath = settings().game().plugin()->binaryName();
  QDir folder           = newExe.absoluteDir();
  while (folder.exists() && !folder.isRoot() &&
         QFileInfo(folder.filePath(binaryPath)) != newExe) {
    folder.cdUp();
  }

  if (folder.exists(binaryPath)) {
    m_gameDir = folder;
    ui->managedGameDirEdit->setText(QDir::toNativeSeparators(
        m_gameDir.absoluteFilePath(settings().game().plugin()->binaryName())));
  } else {
    QMessageBox::warning(
        parentWidget(), QObject::tr("Error"),
        QObject::tr("The given path was not recognized as a valid game installation. "
                    "The current game plugin requires the executable to be in a \"%1\" "
                    "subfolder of the game directory.")
            .arg(QFileInfo(binaryPath).path()));
  }
}

void PathsSettingsTab::on_baseDirEdit_editingFinished()
{
  normalizePath(ui->baseDirEdit);
}

void PathsSettingsTab::on_downloadDirEdit_editingFinished()
{
  normalizePath(ui->downloadDirEdit);
}

void PathsSettingsTab::on_modDirEdit_editingFinished()
{
  normalizePath(ui->modDirEdit);
}

void PathsSettingsTab::on_cacheDirEdit_editingFinished()
{
  normalizePath(ui->cacheDirEdit);
}

void PathsSettingsTab::on_profilesDirEdit_editingFinished()
{
  normalizePath(ui->profilesDirEdit);
}

void PathsSettingsTab::on_overwriteDirEdit_editingFinished()
{
  normalizePath(ui->overwriteDirEdit);
}

void PathsSettingsTab::normalizePath(QLineEdit* lineEdit)
{
  QString text = lineEdit->text();
  while (text.endsWith('/') || text.endsWith('\\')) {
    text.chop(1);
  }
  lineEdit->setText(text);
}

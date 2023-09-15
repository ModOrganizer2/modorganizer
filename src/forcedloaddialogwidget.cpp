#include "forcedloaddialogwidget.h"
#include "executableinfo.h"
#include "ui_forcedloaddialogwidget.h"
#include <QFileDialog>
#include <log.h>

using namespace MOBase;

ForcedLoadDialogWidget::ForcedLoadDialogWidget(const IPluginGame* game, QWidget* parent)
    : QWidget(parent), ui(new Ui::ForcedLoadDialogWidget), m_GamePlugin(game)
{
  ui->setupUi(this);
}

ForcedLoadDialogWidget::~ForcedLoadDialogWidget()
{
  delete ui;
}

bool ForcedLoadDialogWidget::getEnabled()
{
  return ui->enabledBox->isChecked();
}

bool ForcedLoadDialogWidget::getForced()
{
  return m_Forced;
}

QString ForcedLoadDialogWidget::getLibraryPath()
{
  return ui->libraryPathEdit->text();
}

QString ForcedLoadDialogWidget::getProcess()
{
  return ui->processEdit->text();
}

void ForcedLoadDialogWidget::setEnabled(bool enabled)
{
  ui->enabledBox->setChecked(enabled);
}

void ForcedLoadDialogWidget::setForced(bool forced)
{
  m_Forced = forced;
  ui->libraryPathBrowseButton->setEnabled(!forced);
  ui->libraryPathEdit->setEnabled(!forced);
  ui->processBrowseButton->setEnabled(!forced);
  ui->processEdit->setEnabled(!forced);
}

void ForcedLoadDialogWidget::setLibraryPath(const QString& path)
{
  ui->libraryPathEdit->setText(path);
}

void ForcedLoadDialogWidget::setProcess(const QString& name)
{
  ui->processEdit->setText(name);
}

void ForcedLoadDialogWidget::on_enabledBox_toggled()
{
  // anything to do?
}

void ForcedLoadDialogWidget::on_libraryPathBrowseButton_clicked()
{
  QDir gameDir(m_GamePlugin->gameDirectory());
  QString startPath = gameDir.absolutePath();
  QString result    = QFileDialog::getOpenFileName(
      nullptr, "Select a library...", startPath, "Dynamic link library (*.dll)",
      nullptr, QFileDialog::ReadOnly);
  if (!result.isEmpty()) {
    QFileInfo fileInfo(result);
    QString relativePath = gameDir.relativeFilePath(fileInfo.filePath());
    QString filePath     = fileInfo.filePath();
    if (!relativePath.startsWith("..")) {
      filePath = relativePath;
    }

    if (fileInfo.exists()) {
      ui->libraryPathEdit->setText(filePath);
    } else {
      log::error("{} does not exist", filePath);
    }
  }
}

void ForcedLoadDialogWidget::on_processBrowseButton_clicked()
{
  QDir gameDir(m_GamePlugin->gameDirectory());
  QString startPath = gameDir.absolutePath();
  QString result    = QFileDialog::getOpenFileName(nullptr, "Select a process...",
                                                   startPath, "Executable (*.exe)",
                                                   nullptr, QFileDialog::ReadOnly);
  if (!result.isEmpty()) {
    QFileInfo fileInfo(result);
    QString fileName = fileInfo.fileName();

    if (fileInfo.exists()) {
      ui->processEdit->setText(fileName);
    } else {
      log::error("{} does not exist", fileInfo.filePath());
    }
  }
}

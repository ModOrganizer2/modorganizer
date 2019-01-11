#include "forcedloaddialogwidget.h"
#include "ui_forcedloaddialogwidget.h"

#include <QFileDialog>

#include "executableinfo.h"

ForcedLoadDialogWidget::ForcedLoadDialogWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ForcedLoadDialogWidget)
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

void ForcedLoadDialogWidget::setLibraryPath(QString &path)
{
  ui->libraryPathEdit->setText(path);
}

void ForcedLoadDialogWidget::setProcess(QString &name)
{
  ui->processEdit->setText(name);
}

void ForcedLoadDialogWidget::on_enabledBox_toggled()
{
  // anything to do?
}

void ForcedLoadDialogWidget::on_libraryPathBrowseButton_clicked()
{
  QDir gameDir("D:/Games/Steam Library/steamapps/common/Fallout 4");
  QString startPath = gameDir.absolutePath();
  QString result = QFileDialog::getOpenFileName(nullptr, "Select a library...", startPath, "Dynamic link library (*.dll)", nullptr, QFileDialog::ReadOnly);
  if (!result.isEmpty()) {
    ui->libraryPathEdit->setText(result);
  }
}

void ForcedLoadDialogWidget::on_processBrowseButton_clicked()
{
  QDir gameDir("D:/Games/Steam Library/steamapps/common/Fallout 4");
  QString startPath = gameDir.absolutePath();
  QString result = QFileDialog::getOpenFileName(nullptr, "Select a process...", startPath, "Executable (*.exe)", nullptr, QFileDialog::ReadOnly);
  if (!result.isEmpty()) {
    ui->processEdit->setText(QFile(result).fileName());
  }
}

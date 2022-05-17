#include "forcedloaddialog.h"
#include "ui_forcedloaddialog.h"

#include "forcedloaddialogwidget.h"

#include <QComboBox>
#include <QLabel>

using namespace MOBase;

ForcedLoadDialog::ForcedLoadDialog(const IPluginGame* game, QWidget* parent)
    : QDialog(parent), ui(new Ui::ForcedLoadDialog), m_GamePlugin(game)
{
  ui->setupUi(this);
}

ForcedLoadDialog::~ForcedLoadDialog()
{
  delete ui;
}

void ForcedLoadDialog::setValues(QList<MOBase::ExecutableForcedLoadSetting>& values)
{
  ui->tableWidget->clearContents();

  for (int i = 0; i < values.count(); i++) {
    ForcedLoadDialogWidget* item = new ForcedLoadDialogWidget(m_GamePlugin, this);
    item->setEnabled(values[i].enabled());
    item->setProcess(values[i].process());
    item->setLibraryPath(values[i].library());
    item->setForced(values[i].forced());

    ui->tableWidget->insertRow(i);
    ui->tableWidget->setCellWidget(i, 0, item);
  }

  ui->tableWidget->resizeRowsToContents();
}

QList<MOBase::ExecutableForcedLoadSetting> ForcedLoadDialog::values()
{
  QList<MOBase::ExecutableForcedLoadSetting> results;
  for (int row = 0; row < ui->tableWidget->rowCount(); row++) {
    auto widget = (ForcedLoadDialogWidget*)ui->tableWidget->cellWidget(row, 0);
    results.append(
        ExecutableForcedLoadSetting(widget->getProcess(), widget->getLibraryPath())
            .withEnabled(widget->getEnabled())
            .withForced(widget->getForced()));
  }
  return results;
}

void ForcedLoadDialog::on_addRowButton_clicked()
{
  int row = ui->tableWidget->rowCount();
  ui->tableWidget->insertRow(row);
  ForcedLoadDialogWidget* item = new ForcedLoadDialogWidget(m_GamePlugin, this);
  ui->tableWidget->setCellWidget(row, 0, item);
  ui->tableWidget->resizeRowsToContents();
}

void ForcedLoadDialog::on_deleteRowButton_clicked()
{
  for (auto rowIndex : ui->tableWidget->selectionModel()->selectedRows()) {
    int row     = rowIndex.row();
    auto widget = (ForcedLoadDialogWidget*)ui->tableWidget->cellWidget(row, 0);
    if (!widget->getForced()) {
      ui->tableWidget->removeRow(row);
    }
  }
}

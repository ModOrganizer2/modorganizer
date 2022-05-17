#include "savetextasdialog.h"
#include "ui_savetextasdialog.h"
#include <report.h>
#include <QClipboard>
#include <QFileDialog>


using MOBase::reportError;


SaveTextAsDialog::SaveTextAsDialog(QWidget *parent)
  : QDialog(parent), ui(new Ui::SaveTextAsDialog)
{
  ui->setupUi(this);
}

SaveTextAsDialog::~SaveTextAsDialog()
{
  delete ui;
}

void SaveTextAsDialog::setText(const QString &text)
{
  ui->textEdit->setPlainText(text);
}

void SaveTextAsDialog::on_closeBtn_clicked()
{
  this->close();
}

void SaveTextAsDialog::on_clipboardBtn_clicked()
{
  QClipboard *clipboard = QApplication::clipboard();
  clipboard->setText(ui->textEdit->toPlainText());
}

void SaveTextAsDialog::on_saveAsBtn_clicked()
{
  QString fileName = QFileDialog::getSaveFileName(this, tr("Save CSV"), QString(), tr("Text Files") + " (*.txt *.csv)");
  if (!fileName.isEmpty()) {
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
      reportError(tr("failed to open \"%1\" for writing").arg(fileName));
      return;
    }

    file.write(ui->textEdit->toPlainText().toUtf8());
  }
}

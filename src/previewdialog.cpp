#include "previewdialog.h"
#include "settings.h"
#include "ui_previewdialog.h"
#include <QFileInfo>

PreviewDialog::PreviewDialog(const QString& fileName, QWidget* parent)
    : QDialog(parent), ui(new Ui::PreviewDialog)
{
  ui->setupUi(this);
  ui->nameLabel->setText(QFileInfo(fileName).fileName());
  ui->nextButton->setEnabled(false);
  ui->previousButton->setEnabled(false);
}

PreviewDialog::~PreviewDialog()
{
  delete ui;
}

int PreviewDialog::exec()
{
  GeometrySaver gs(Settings::instance(), this);
  return QDialog::exec();
}

void PreviewDialog::addVariant(const QString& modName, QWidget* widget)
{
  widget->setProperty("modName", modName);
  ui->variantsStack->addWidget(widget);
  if (ui->variantsStack->count() > 1) {
    ui->nextButton->setEnabled(true);
    ui->previousButton->setEnabled(true);
  }
}

int PreviewDialog::numVariants() const
{
  return ui->variantsStack->count();
}

void PreviewDialog::on_variantsStack_currentChanged(int index)
{
  ui->modLabel->setText(
      ui->variantsStack->widget(index)->property("modName").toString());
}

void PreviewDialog::on_closeButton_clicked()
{
  this->accept();
}

void PreviewDialog::on_previousButton_clicked()
{
  int i = ui->variantsStack->currentIndex() - 1;
  if (i < 0) {
    i = ui->variantsStack->count() - 1;
  }
  ui->variantsStack->setCurrentIndex(i);
}

void PreviewDialog::on_nextButton_clicked()
{
  ui->variantsStack->setCurrentIndex((ui->variantsStack->currentIndex() + 1) %
                                     ui->variantsStack->count());
}

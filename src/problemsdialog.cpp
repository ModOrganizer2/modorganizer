#include "problemsdialog.h"
#include "ui_problemsdialog.h"
#include <utility.h>
#include <iplugindiagnose.h>
#include <QPushButton>
#include <Shellapi.h>


using namespace MOBase;


ProblemsDialog::ProblemsDialog(std::vector<MOBase::IPluginDiagnose *> diagnosePlugins, QWidget *parent)
  : QDialog(parent), ui(new Ui::ProblemsDialog)
{
  ui->setupUi(this);

  foreach (IPluginDiagnose *diagnose, diagnosePlugins) {
    std::vector<unsigned int> activeProblems = diagnose->activeProblems();
    foreach (unsigned int key, activeProblems) {
      QTreeWidgetItem *newItem = new QTreeWidgetItem();
      newItem->setText(0, diagnose->shortDescription(key));
      newItem->setData(0, Qt::UserRole, diagnose->fullDescription(key));
      newItem->setData(1, Qt::UserRole, qVariantFromValue(reinterpret_cast<void*>(diagnose)));
      newItem->setData(1, Qt::UserRole + 1, key);

      ui->problemsWidget->addTopLevelItem(newItem);

      if (diagnose->hasGuidedFix(key)) {
        newItem->setText(1, tr("fix"));
        QPushButton *fixButton = new QPushButton(tr("Fix"));
        connect(fixButton, SIGNAL(clicked()), this, SLOT(startFix()));
        ui->problemsWidget->setItemWidget(newItem, 1, fixButton);
      } else {
        newItem->setText(1, tr("No guided fix"));
      }
    }
  }
  connect(ui->problemsWidget, SIGNAL(itemSelectionChanged()), this, SLOT(selectionChanged()));
  connect(ui->descriptionText, SIGNAL(anchorClicked(QUrl)), this, SLOT(urlClicked(QUrl)));
}


ProblemsDialog::~ProblemsDialog()
{
  delete ui;
}


bool ProblemsDialog::hasProblems() const
{
  return ui->problemsWidget->topLevelItemCount() != 0;
}

void ProblemsDialog::selectionChanged()
{
  QString text = ui->problemsWidget->currentItem()->data(0, Qt::UserRole).toString();
  ui->descriptionText->setText(text);
  ui->descriptionText->setLineWrapMode(text.contains('\n') ? QTextEdit::NoWrap : QTextEdit::WidgetWidth);
}

void ProblemsDialog::startFix()
{
  IPluginDiagnose *plugin = reinterpret_cast<IPluginDiagnose*>(ui->problemsWidget->currentItem()->data(1, Qt::UserRole).value<void*>());
  plugin->startGuidedFix(ui->problemsWidget->currentItem()->data(1, Qt::UserRole + 1).toUInt());
}

void ProblemsDialog::urlClicked(const QUrl &url)
{
  ::ShellExecuteW(NULL, L"open", ToWString(url.toString()).c_str(), NULL, NULL, SW_SHOWNORMAL);
}

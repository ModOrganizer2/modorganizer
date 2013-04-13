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

      if (diagnose->hasGuidedFix(key)) {
        ui->problemsWidget->setItemWidget(newItem, 1, new QPushButton(tr("Fix")));
      } else {
        newItem->setText(1, tr("No guided fix :("));
      }
      ui->problemsWidget->addTopLevelItem(newItem);
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

void ProblemsDialog::urlClicked(const QUrl &url)
{
  ::ShellExecuteW(NULL, L"open", ToWString(url.toString()).c_str(), NULL, NULL, SW_SHOWNORMAL);
}

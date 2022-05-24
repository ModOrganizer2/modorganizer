#include "problemsdialog.h"
#include "organizercore.h"
#include "ui_problemsdialog.h"
#include <QPushButton>
#include <Shellapi.h>
#include <iplugin.h>
#include <iplugindiagnose.h>
#include <utility.h>

#include "plugincontainer.h"

using namespace MOBase;

ProblemsDialog::ProblemsDialog(const PluginManager& pluginManager, QWidget* parent)
    : QDialog(parent), ui(new Ui::ProblemsDialog), m_PluginManager(pluginManager),
      m_hasProblems(false)
{
  ui->setupUi(this);

  runDiagnosis();

  connect(ui->problemsWidget, SIGNAL(itemSelectionChanged()), this,
          SLOT(selectionChanged()));
  connect(ui->descriptionText, SIGNAL(anchorClicked(QUrl)), this,
          SLOT(urlClicked(QUrl)));
}

ProblemsDialog::~ProblemsDialog()
{
  delete ui;
}

int ProblemsDialog::exec()
{
  GeometrySaver gs(Settings::instance(), this);
  return QDialog::exec();
}

void ProblemsDialog::runDiagnosis()
{
  m_hasProblems = false;
  ui->problemsWidget->clear();

  for (IPluginDiagnose* diagnose : m_PluginManager.plugins<IPluginDiagnose>()) {
    if (!m_PluginManager.isEnabled(diagnose)) {
      continue;
    }

    std::vector<unsigned int> activeProblems = diagnose->activeProblems();
    for (const auto key : activeProblems) {
      QTreeWidgetItem* newItem = new QTreeWidgetItem();
      newItem->setText(0, diagnose->shortDescription(key));
      newItem->setData(0, Qt::UserRole, diagnose->fullDescription(key));

      ui->problemsWidget->addTopLevelItem(newItem);
      m_hasProblems = true;

      if (diagnose->hasGuidedFix(key)) {
        newItem->setText(1, tr("Fix"));
        QPushButton* fixButton = new QPushButton(tr("Fix"));
        fixButton->setProperty("fix",
                               QVariant::fromValue(reinterpret_cast<void*>(diagnose)));
        fixButton->setProperty("key", key);
        connect(fixButton, SIGNAL(clicked()), this, SLOT(startFix()));
        ui->problemsWidget->setItemWidget(newItem, 1, fixButton);
      } else {
        newItem->setText(1, tr("No guided fix"));
      }
    }
  }

  if (!m_hasProblems) {
    auto* item = new QTreeWidgetItem;

    item->setText(0, tr("(There are no notifications)"));
    item->setText(1, "");
    item->setData(0, Qt::UserRole, QString());

    QFont font = item->font(0);
    font.setItalic(true);
    item->setFont(0, font);

    ui->problemsWidget->addTopLevelItem(item);
  }
}

bool ProblemsDialog::hasProblems() const
{
  return m_hasProblems;
}

void ProblemsDialog::selectionChanged()
{
  QString text = ui->problemsWidget->currentItem()->data(0, Qt::UserRole).toString();
  ui->descriptionText->setText(text);
  ui->descriptionText->setLineWrapMode(text.contains('\n') ? QTextEdit::NoWrap
                                                           : QTextEdit::WidgetWidth);
}

void ProblemsDialog::startFix()
{
  QObject* fixButton = QObject::sender();
  if (fixButton == NULL) {
    log::warn("no button");
    return;
  }
  IPluginDiagnose* plugin =
      reinterpret_cast<IPluginDiagnose*>(fixButton->property("fix").value<void*>());
  plugin->startGuidedFix(fixButton->property("key").toUInt());
  runDiagnosis();
}

void ProblemsDialog::urlClicked(const QUrl& url)
{
  shell::Open(url);
}

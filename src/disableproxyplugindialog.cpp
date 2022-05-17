#include "disableproxyplugindialog.h"

#include "ui_disableproxyplugindialog.h"

using namespace MOBase;

DisableProxyPluginDialog::DisableProxyPluginDialog(
    MOBase::IPlugin* proxyPlugin, std::vector<MOBase::IPlugin*> const& required,
    QWidget* parent)
    : QDialog(parent), ui(new Ui::DisableProxyPluginDialog)
{
  ui->setupUi(this);

  ui->topLabel->setText(QObject::tr("Disabling the '%1' plugin will prevent the "
                                    "following %2 plugin(s) from working:",
                                    "", required.size())
                            .arg(proxyPlugin->localizedName())
                            .arg(required.size()));

  connect(ui->noBtn, &QPushButton::clicked, this, &QDialog::reject);
  connect(ui->yesBtn, &QPushButton::clicked, this, &QDialog::accept);

  ui->requiredPlugins->setSelectionMode(QAbstractItemView::NoSelection);
  ui->requiredPlugins->setRowCount(required.size());
  for (int i = 0; i < required.size(); ++i) {
    ui->requiredPlugins->setItem(i, 0,
                                 new QTableWidgetItem(required[i]->localizedName()));
    ui->requiredPlugins->setItem(i, 1,
                                 new QTableWidgetItem(required[i]->description()));
    ui->requiredPlugins->setRowHeight(i, 9);
  }
  ui->requiredPlugins->verticalHeader()->setVisible(false);
  ui->requiredPlugins->sortByColumn(0, Qt::AscendingOrder);
}

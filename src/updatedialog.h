#ifndef MODORGANIZER_UPDATEDIALOG_H
#define MODORGANIZER_UPDATEDIALOG_H

#include <QDialog>

#include "lootdialog.h"  // for MarkdownDocument
#include <expanderwidget.h>

namespace Ui
{
class UpdateDialog;
}

class UpdateDialog : public QDialog
{
  Q_OBJECT;

public:
  UpdateDialog(QWidget* parent);
  ~UpdateDialog();

  void setChangeLogs(const QString& text);
  void setVersions(const QString& oldVersion, const QString& newVersion);

private:
  std::unique_ptr<Ui::UpdateDialog> ui;
  MOBase::ExpanderWidget m_expander;
  MarkdownDocument m_changeLogs;
};

#endif  // MODORGANIZER_UPDATEDIALOG_H

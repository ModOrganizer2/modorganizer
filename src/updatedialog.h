#ifndef MODORGANIZER_UPDATEDIALOG_H
#define MODORGANIZER_UPDATEDIALOG_H

#include <QDialog>

#include <expanderwidget.h>
#include "lootdialog.h" // for MarkdownDocument

namespace Ui { class UpdateDialog; }

class UpdateDialog : public QDialog
{
  Q_OBJECT;

public:
  UpdateDialog(QWidget* parent, const QString& title);
  ~UpdateDialog();

  void setChangeLogs(const QString& text);

private:
  std::unique_ptr<Ui::UpdateDialog> ui;
  MOBase::ExpanderWidget m_expander;
  MarkdownDocument m_changeLogs;
};

#endif // MODORGANIZER_UPDATEDIALOG_H

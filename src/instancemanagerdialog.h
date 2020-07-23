#ifndef MODORGANIZER_INSTANCEMANAGERDIALOG_INCLUDED
#define MODORGANIZER_INSTANCEMANAGERDIALOG_INCLUDED

#include <QDialog>

namespace Ui { class InstanceManagerDialog; };

class InstanceManagerDialog : public QDialog
{
  Q_OBJECT

public:
  explicit InstanceManagerDialog(QWidget *parent = nullptr);
  ~InstanceManagerDialog();

private:
  std::unique_ptr<Ui::InstanceManagerDialog> ui;
};

#endif // MODORGANIZER_INSTANCEMANAGERDIALOG_INCLUDED

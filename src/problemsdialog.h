#ifndef PROBLEMSDIALOG_H
#define PROBLEMSDIALOG_H

#include <QDialog>
#include <QUrl>
#include <iplugindiagnose.h>

namespace Ui
{
class ProblemsDialog;
}

class PluginContainer;

class ProblemsDialog : public QDialog
{
  Q_OBJECT

public:
  explicit ProblemsDialog(PluginContainer const& pluginContainer, QWidget* parent = 0);
  ~ProblemsDialog();

  // also saves and restores geometry
  //
  int exec() override;

  bool hasProblems() const;

private:
  void runDiagnosis();

private slots:
  void selectionChanged();
  void urlClicked(const QUrl& url);

  void startFix();

private:
  Ui::ProblemsDialog* ui;
  const PluginContainer& m_PluginContainer;
  bool m_hasProblems;
};

#endif  // PROBLEMSDIALOG_H

#ifndef MODORGANIZER_LOOTDIALOG_H
#define MODORGANIZER_LOOTDIALOG_H

#include <lootcli/lootcli.h>
#include <log.h>

namespace Ui { class LootDialog; }

class OrganizerCore;
class Loot;

class LootDialog : public QDialog
{
public:
  LootDialog(QWidget* parent, OrganizerCore& core, Loot& loot);
  ~LootDialog();

  void setText(const QString& s);
  void setProgress(lootcli::Progress p);

  QString progressToString(lootcli::Progress p);

  void addOutput(const QString& s);

  bool result() const;

  void cancel();

  void openReport();

private:
  std::unique_ptr<Ui::LootDialog> ui;
  OrganizerCore& m_core;
  Loot& m_loot;
  bool m_finished;
  bool m_cancelling;

  void createUI();
  void closeEvent(QCloseEvent* e) override;
  void onButton(QAbstractButton* b);
  void addLineOutput(const QString& line);
  void onFinished();
  void log(MOBase::log::Levels lv, const QString& s);
  void handleReport();
};

#endif // MODORGANIZER_LOOTDIALOG_H

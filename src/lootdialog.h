#ifndef MODORGANIZER_LOOTDIALOG_H
#define MODORGANIZER_LOOTDIALOG_H

#include <expanderwidget.h>
#include <log.h>
#include <lootcli/lootcli.h>

namespace Ui
{
class LootDialog;
}

class OrganizerCore;
class Loot;

class MarkdownDocument : public QObject
{
  Q_OBJECT;
  Q_PROPERTY(QString text MEMBER m_text NOTIFY textChanged FINAL);

public:
  explicit MarkdownDocument(QObject* parent = nullptr);
  void setText(const QString& text);

signals:
  void textChanged(const QString& text);

private:
  QString m_text;
};

class MarkdownPage : public QWebEnginePage
{
  Q_OBJECT;

public:
  explicit MarkdownPage(QObject* parent = nullptr);

protected:
  bool acceptNavigationRequest(const QUrl& url, NavigationType, bool) override;
};

class LootDialog : public QDialog
{
  Q_OBJECT;

public:
  LootDialog(QWidget* parent, OrganizerCore& core, Loot& loot);
  ~LootDialog();

  void setText(const QString& s);
  void setProgress(lootcli::Progress p);

  void addOutput(const QString& s);
  bool result() const;
  void cancel();
  void openReport();

  int exec() override;
  void accept() override;
  void reject() override;

private:
  std::unique_ptr<Ui::LootDialog> ui;
  MOBase::ExpanderWidget m_expander;
  OrganizerCore& m_core;
  Loot& m_loot;
  bool m_finished;
  bool m_cancelling;
  MarkdownDocument m_report;

  void createUI();
  void closeEvent(QCloseEvent* e) override;
  void addLineOutput(const QString& line);
  void onFinished();
  void log(MOBase::log::Levels lv, const QString& s);
  void showReport();
};

#endif  // MODORGANIZER_LOOTDIALOG_H

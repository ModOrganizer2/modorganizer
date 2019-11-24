#ifndef MODORGANIZER_LOOT_H
#define MODORGANIZER_LOOT_H

#include "envmodule.h"
#include <log.h>
#include <lootcli/lootcli.h>
#include <windows.h>
#include <QWidget>

Q_DECLARE_METATYPE(lootcli::Progress);
Q_DECLARE_METATYPE(MOBase::log::Levels);

class OrganizerCore;

class Loot : public QObject
{
  Q_OBJECT;

public:
  Loot();
  ~Loot();

  bool start(QWidget* parent, OrganizerCore& core, bool didUpdateMasterList);
  void cancel();
  bool result() const;
  const QString& outPath() const;

signals:
  void output(const QString& s);
  void progress(const lootcli::Progress p);
  void log(MOBase::log::Levels level, const QString& s);
  void information(const QString& mod, const QString& info);
  void finished();

private:
  struct Report;
  struct Stats;
  struct Message;
  struct Plugin;
  struct Dirty;
  struct File;
  class BadReport {};

  std::unique_ptr<QThread> m_thread;
  std::atomic<bool> m_cancel;
  std::atomic<bool> m_result;
  QString m_outPath;
  env::HandlePtr m_lootProcess;
  env::HandlePtr m_stdout;
  std::string m_outputBuffer;

  std::string readFromPipe();

  void lootThread();
  bool waitForCompletion();

  void processStdout(const std::string &lootOut);
  void processMessage(const lootcli::Message& m);

  void processOutputFile();

  Report createReport(const QJsonDocument& doc) const;
  Message reportMessage(const QJsonObject& message) const;
  std::vector<Plugin> reportPlugins(const QJsonArray& plugins) const;
  Loot::Plugin reportPlugin(const QJsonObject& plugin) const;

  std::vector<Message> reportMessages(const QJsonArray& array) const;
  std::vector<Loot::File> reportFiles(const QJsonArray& array) const;
  std::vector<Loot::Dirty> reportDirty(const QJsonArray& array) const;
  std::vector<QString> reportStringArray(const QJsonArray& array) const;
};


bool runLoot(QWidget* parent, OrganizerCore& core, bool didUpdateMasterList);

#endif // MODORGANIZER_LOOT_H

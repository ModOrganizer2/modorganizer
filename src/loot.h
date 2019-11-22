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
  bool processMessages(const QJsonArray& messages);
  bool processMessage(const QJsonObject& message);
  bool processPlugins(const QJsonArray& plugins);
  bool processPlugin(const QJsonObject& plugin);

  bool processPluginDirty(const QString& name, const QJsonObject& plugin);

  template <class Format, class... Args>
  void logJsonError(Format&& f, Args&&... args)
  {
    MOBase::log::error(
      std::string("loot output file '{}': ") + f,
      m_outPath, std::forward<Args>(args)...);
  };
};


bool runLoot(QWidget* parent, OrganizerCore& core, bool didUpdateMasterList);

#endif // MODORGANIZER_LOOT_H

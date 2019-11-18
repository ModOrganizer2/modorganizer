#ifndef MODORGANIZER_LOOT_H
#define MODORGANIZER_LOOT_H

#include "envmodule.h"
#include <log.h>
#include <windows.h>
#include <QWidget>

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

signals:
  void output(const QString& s);
  void progress(const QString& s);
  void information(const QString& mod, const QString& info);
  void errorMessage(const QString& s);
  void error(const QString& s);
  void finished();

private:
  std::unique_ptr<QThread> m_thread;
  std::atomic<bool> m_cancel;
  std::atomic<bool> m_result;
  QString m_outPath;
  env::HandlePtr m_lootProcess;
  env::HandlePtr m_stdout;

  std::string readFromPipe();

  void lootThread();
  bool waitForCompletion();
  void processStdout(const std::string &lootOut);

  void processOutputFile();
  bool processOutputPlugin(const QJsonValue& pluginValue);

  bool processPluginMessages(
    const QString& pluginName, const QJsonObject& plugin);

  bool processPluginMessage(
    const QString& pluginName, const QJsonObject& message);

  bool processPluginDirty(
    const QString& pluginName, const QJsonObject& plugin);

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

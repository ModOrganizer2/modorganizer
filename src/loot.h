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
  struct Message
  {
    QString type;
    QString text;
  };

  struct File
  {
    QString name;
    QString displayName;
  };

  struct Dirty
  {
    qint64 crc=0;
    qint64 itm=0;
    qint64 deletedReferences=0;
    qint64 deletedNavmesh=0;
    QString cleaningUtility;
    QString info;

    QString toString(bool isClean) const;
    QString cleaningString() const;
  };

  struct Plugin
  {
    QString name;
    std::vector<File> incompatibilities;
    std::vector<Message> messages;
    std::vector<Dirty> dirty, clean;
    std::vector<QString> missingMasters;
    bool loadsArchive = false;
    bool isMaster = false;
    bool isLightMaster = false;
  };

  struct Stats
  {
    qint64 time = 0;
    QString version;
  };

  struct Report
  {
    std::vector<Message> messages;
    std::vector<Plugin> plugins;
    Stats stats;
  };


  Loot();
  ~Loot();

  bool start(QWidget* parent, OrganizerCore& core, bool didUpdateMasterList);
  void cancel();
  bool result() const;
  const QString& outPath() const;
  const Report& report() const;

signals:
  void output(const QString& s);
  void progress(const lootcli::Progress p);
  void log(MOBase::log::Levels level, const QString& s);
  void finished();

private:
  std::unique_ptr<QThread> m_thread;
  std::atomic<bool> m_cancel;
  std::atomic<bool> m_result;
  QString m_outPath;
  env::HandlePtr m_lootProcess;
  env::HandlePtr m_stdout;
  std::string m_outputBuffer;
  Report m_report;

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

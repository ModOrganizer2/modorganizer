#include "loot.h"
#include "lootdialog.h"
#include "spawn.h"
#include "organizercore.h"
#include "json.h"
#include <log.h>
#include <report.h>

using namespace MOBase;
using namespace json;

log::Levels levelFromLoot(lootcli::LogLevels level)
{
  using LC = lootcli::LogLevels;

  switch (level)
  {
    case LC::Trace:  // fall-through
    case LC::Debug:
      return log::Debug;

    case LC::Info:
      return log::Info;

    case LC::Warning:
      return log::Warning;

    case LC::Error:
      return log::Error;

    default:
      return log::Info;
  }
}


QString Loot::Dirty::toString(bool isClean) const
{
  if (isClean) {
    return QObject::tr("Verified clean by %1")
      .arg(cleaningUtility.isEmpty() ? "?" : cleaningUtility);
  }

  QString s = cleaningString();

  if (!info.isEmpty()) {
    s += " " + info;
  }

  return s;
}

QString Loot::Dirty::cleaningString() const
{
  return QObject::tr("%1 found %2 ITM record(s), %3 deleted reference(s) and %4 deleted navmesh(es).")
    .arg(cleaningUtility.isEmpty() ? "?" : cleaningUtility)
    .arg(itm)
    .arg(deletedReferences)
    .arg(deletedNavmesh);
}


Loot::Loot()
  : m_thread(nullptr), m_cancel(false), m_result(false)
{
}

Loot::~Loot()
{
  if (m_thread) {
    m_thread->wait();
  }

  if (!m_outPath.isEmpty() && QFile::exists(m_outPath)) {
    const auto r = shell::Delete(m_outPath);

    if (!r) {
      log::error(
        "failed to remove temporary loot json report '{}': {}",
        m_outPath, r.toString());
    }
  }
}

bool Loot::start(QWidget* parent, OrganizerCore& core, bool didUpdateMasterList)
{
  m_outPath = QDir::temp().absoluteFilePath("lootreport.json");

  const auto logLevel = core.settings().diagnostics().lootLogLevel();

  QStringList parameters;
  parameters
    << "--game" << core.managedGame()->gameShortName()
    << "--gamePath" << QString("\"%1\"").arg(core.managedGame()->gameDirectory().absolutePath())
    << "--pluginListPath" << QString("\"%1/loadorder.txt\"").arg(core.profilePath())
    << "--logLevel" << QString::fromStdString(lootcli::logLevelToString(logLevel))
    << "--out" << QString("\"%1\"").arg(m_outPath);

  if (didUpdateMasterList) {
    parameters << "--skipUpdateMasterlist";
  }

  SECURITY_ATTRIBUTES secAttributes;
  secAttributes.nLength = sizeof(SECURITY_ATTRIBUTES);
  secAttributes.bInheritHandle = TRUE;
  secAttributes.lpSecurityDescriptor = nullptr;

  env::HandlePtr readPipe, writePipe;

  {
    HANDLE read = INVALID_HANDLE_VALUE;
    HANDLE write = INVALID_HANDLE_VALUE;

    if (!::CreatePipe(&read, &write, &secAttributes, 0)) {
      log::error("failed to create stdout reroute");
    }

    readPipe.reset(read);
    writePipe.reset(write);

    if (!::SetHandleInformation(read, HANDLE_FLAG_INHERIT, 0)) {
      log::error("failed to correctly set up the stdout reroute");
    }
  }

  core.prepareVFS();

  spawn::SpawnParameters sp;
  sp.binary = QFileInfo(qApp->applicationDirPath() + "/loot/lootcli.exe");
  sp.arguments = parameters.join(" ");
  sp.currentDirectory.setPath(qApp->applicationDirPath() + "/loot");
  sp.hooked = true;
  sp.stdOut = writePipe.get();

  m_stdout = std::move(readPipe);

  HANDLE lootHandle = spawn::startBinary(parent, sp);
  if (lootHandle == INVALID_HANDLE_VALUE) {
    emit log(log::Levels::Error, tr("failed to start loot"));
    return false;
  }

  m_lootProcess.reset(lootHandle);

  core.pluginList()->clearAdditionalInformation();

  m_thread.reset(QThread::create([&]{
    lootThread();
    emit finished();
  }));

  m_thread->start();

  return true;
}

void Loot::cancel()
{
  m_cancel = true;
}

bool Loot::result() const
{
  return m_result;
}

const QString& Loot::outPath() const
{
  return m_outPath;
}

const Loot::Report& Loot::report() const
{
  return m_report;
}

void Loot::lootThread()
{
  try {
    m_result = false;

    if (!waitForCompletion()) {
      return;
    }

    m_result = true;
    processOutputFile();
  } catch (const std::exception &e) {
    emit log(log::Levels::Error, tr("failed to run loot: %1").arg(e.what()));
  }
}

bool Loot::waitForCompletion()
{
  bool terminating = false;

  for (;;) {
    DWORD res = WaitForSingleObject(m_lootProcess.get(), 100);

    if (res == WAIT_OBJECT_0) {
      // done
      break;
    }

    if (res == WAIT_FAILED) {
      const auto e = GetLastError();
      log::error("failed to wait on loot process, {}", formatSystemMessage(e));
      return false;
    }

    if (m_cancel) {
      // terminate and wait to finish
      ::TerminateProcess(m_lootProcess.get(), 1);
      WaitForSingleObject(m_lootProcess.get(), INFINITE);
      return false;
    }

    processStdout(readFromPipe());
  }

  if (m_cancel) {
    return false;
  }

  processStdout(readFromPipe());

  // checking exit code
  DWORD exitCode = 0;

  if (!::GetExitCodeProcess(m_lootProcess.get(), &exitCode)) {
    const auto e = GetLastError();
    log::error("failed to get exit code for loot, {}", formatSystemMessage(e));
    return false;
  }

  if (exitCode != 0UL) {
    emit log(log::Levels::Error, tr("Loot failed. Exit code was: %1").arg(exitCode));
    return false;
  }

  return true;
}

std::string Loot::readFromPipe()
{
  static const int chunkSize = 128;
  std::string result;

  char buffer[chunkSize + 1];
  buffer[chunkSize] = '\0';

  DWORD read = 1;
  while (read > 0) {
    if (!::ReadFile(m_stdout.get(), buffer, chunkSize, &read, nullptr)) {
      break;
    }
    if (read > 0) {
      result.append(buffer, read);
      if (read < chunkSize) {
        break;
      }
    }
  }
  return result;
}

void Loot::processStdout(const std::string &lootOut)
{
  emit output(QString::fromStdString(lootOut));

  m_outputBuffer += lootOut;
  std::size_t start = 0;

  for (;;) {
    const auto newline = m_outputBuffer.find("\n", start);
    if (newline == std::string::npos) {
      break;
    }

    const std::string_view line(m_outputBuffer.c_str() + start, newline - start);
    const auto m = lootcli::parseMessage(line);

    if (m.type == lootcli::MessageType::None) {
      log::error("unrecognised loot output: '{}'", line);
      continue;
    }

    processMessage(m);

    start = newline + 1;
  }

  m_outputBuffer.erase(0, start);
}

void Loot::processMessage(const lootcli::Message& m)
{
  switch (m.type)
  {
    case lootcli::MessageType::Log:
    {
      emit log(levelFromLoot(m.logLevel), QString::fromStdString(m.log));
      break;
    }

    case lootcli::MessageType::Progress:
    {
      emit progress(m.progress);
      break;
    }
  }
}

void Loot::processOutputFile()
{
  log::info("parsing json output file at '{}'", m_outPath);

  QFile outFile(m_outPath);
  if (!outFile.open(QIODevice::ReadOnly)) {
    emit log(
      MOBase::log::Error,
      QString("failed to open file, %1 (error %2)")
        .arg(outFile.errorString()).arg(outFile.error()));

    return;
  }

  QJsonParseError e;
  const QJsonDocument doc = QJsonDocument::fromJson(outFile.readAll(), &e);
  if (doc.isNull()) {
    emit log(
      MOBase::log::Error,
      QString("invalid json, %1 (error %2)")
        .arg(e.errorString()).arg(e.error));

    return;
  }

  m_report = createReport(doc);
}

Loot::Report Loot::createReport(const QJsonDocument& doc) const
{
  requireObject(doc, "root");

  Report r;
  const QJsonObject object = doc.object();

  r.messages = reportMessages(getOpt<QJsonArray>(object, "messages"));
  r.plugins = reportPlugins(getOpt<QJsonArray>(object, "plugins"));

  return r;
}

std::vector<Loot::Plugin> Loot::reportPlugins(const QJsonArray& plugins) const
{
  std::vector<Loot::Plugin> v;

  for (auto pluginValue : plugins) {
    const auto o = convertWarn<QJsonObject>(pluginValue, "plugin");
    if (o.isEmpty()) {
      continue;
    }

    auto p = reportPlugin(o);
    if (!p.name.isEmpty()) {
      v.emplace_back(std::move(p));
    }
  }

  return v;
}

Loot::Plugin Loot::reportPlugin(const QJsonObject& plugin) const
{
  Plugin p;

  p.name = getWarn<QString>(plugin, "name");
  if (p.name.isEmpty()) {
    return {};
  }

  if (plugin.contains("incompatibilities")) {
    p.incompatibilities = reportFiles(getOpt<QJsonArray>(plugin, "incompatibilities"));
  }

  if (plugin.contains("messages")) {
    p.messages = reportMessages(getOpt<QJsonArray>(plugin, "messages"));
  }

  if (plugin.contains("dirty")) {
    p.dirty = reportDirty(getOpt<QJsonArray>(plugin, "dirty"));
  }

  if (plugin.contains("clean")) {
    p.clean = reportDirty(getOpt<QJsonArray>(plugin, "clean"));
  }

  if (plugin.contains("missingMasters")) {
    p.missingMasters = reportStringArray(getOpt<QJsonArray>(plugin, "missingMasters"));
  }

  p.loadsArchive = getOpt(plugin, "loadsArchive", false);
  p.isMaster = getOpt(plugin, "isMaster", false);
  p.isLightMaster = getOpt(plugin, "isLightMaster", false);

  return p;
}

std::vector<Loot::Message> Loot::reportMessages(const QJsonArray& array) const
{
  std::vector<Loot::Message> v;

  for (auto messageValue : array) {
    const auto o = convertWarn<QJsonObject>(messageValue, "message");
    if (o.isEmpty()) {
      continue;
    }

    Message m;

    const auto type = getWarn<QString>(o, "type");

    if (type == "info") {
      m.type = log::Info;
    } else if (type == "warn") {
      m.type = log::Warning;
    } else if (type == "error") {
      m.type = log::Error;
    } else {
      log::error("unknown message type '{}'", type);
      m.type = log::Info;
    }

    m.text = getWarn<QString>(o, "text");

    if (!m.text.isEmpty()) {
      v.emplace_back(std::move(m));
    }
  }

  return v;
}

std::vector<Loot::File> Loot::reportFiles(const QJsonArray& array) const
{
  std::vector<Loot::File> v;

  for (auto&& fileValue : array) {
    const auto o = convertWarn<QJsonObject>(fileValue, "file");
    if (o.isEmpty()) {
      continue;
    }

    File f;

    f.name = getWarn<QString>(o, "name");
    f.displayName = getOpt<QString>(o, "displayName");

    if (!f.name.isEmpty()) {
      v.emplace_back(std::move(f));
    }
  }

  return v;
}

std::vector<Loot::Dirty> Loot::reportDirty(const QJsonArray& array) const
{
  std::vector<Loot::Dirty> v;

  for (auto&& dirtyValue : array) {
    const auto o = convertWarn<QJsonObject>(dirtyValue, "dirty");

    Dirty d;

    d.crc = getWarn<qint64>(o, "crc");
    d.itm = getOpt<qint64>(o, "itm");
    d.deletedReferences = getOpt<qint64>(o, "deletedReferences");
    d.deletedNavmesh = getOpt<qint64>(o, "deletedNavmesh");
    d.cleaningUtility = getOpt<QString>(o, "cleaningUtility");
    d.info = getOpt<QString>(o, "info");

    v.emplace_back(std::move(d));
  }

  return v;
}

std::vector<QString> Loot::reportStringArray(const QJsonArray& array) const
{
  std::vector<QString> v;

  for (auto&& sv : array) {
    auto s = convertWarn<QString>(sv, "string");
    if (s.isEmpty()) {
      continue;
    }

    v.emplace_back(std::move(s));
  }

  return v;
}


bool runLoot(QWidget* parent, OrganizerCore& core, bool didUpdateMasterList)
{
  //m_OrganizerCore.currentProfile()->writeModlistNow();
  core.savePluginList();

  //Create a backup of the load orders w/ LOOT in name
  //to make sure that any sorting is easily undo-able.
  //Need to figure out how I want to do that.

  try {
    Loot loot;
    LootDialog dialog(parent, core, loot);

    loot.start(parent, core, didUpdateMasterList);

    dialog.setText(QObject::tr("Please wait while LOOT is running"));
    dialog.exec();

    return dialog.result();
  } catch (const UsvfsConnectorException &e) {
    log::debug("{}", e.what());
    return false;
  } catch (const std::exception &e) {
    reportError(QObject::tr("failed to run loot: %1").arg(e.what()));
    return false;
  }
}

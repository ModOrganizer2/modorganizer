#include "loot.h"
#include "lootdialog.h"
#include "spawn.h"
#include "organizercore.h"
#include "json.h"
#include <log.h>
#include <report.h>

using namespace MOBase;
using namespace json;

static QString LootReportPath = QDir::temp().absoluteFilePath("lootreport.json");

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


QString Loot::Report::toMarkdown() const
{
  QString s;

  if (!messages.empty()) {
    s += "### " + QObject::tr("General messages") + "\n";

    for (auto&& m : messages) {
      s += " - " + m.toMarkdown() + "\n";
    }
  }

  if (!plugins.empty()) {
    if (!s.isEmpty()) {
      s += "\n";
    }

    s += "### " + QObject::tr("Plugins") + "\n";

    for (auto&& p : plugins) {
      const auto ps = p.toMarkdown();
      if (!ps.isEmpty()) {
        s += ps + "\n";
      }
    }
  }

  if (s.isEmpty()) {
    s += "**" + QObject::tr("No messages.") + "**";
  }

  s += stats.toMarkdown();

  return s;
}

QString Loot::Stats::toMarkdown() const
{
  return QString("`stats: %1s, lootcli %2, loot %3`")
    .arg(QString::number(time / 1000.0, 'f', 2))
    .arg(lootcliVersion)
    .arg(lootVersion);
}

QString Loot::Plugin::toMarkdown() const
{
  QString s;

  if (!incompatibilities.empty()) {
    s += " - **" + QObject::tr("Incompatibilities") + ": ";

    QString fs;
    for (auto&& f : incompatibilities) {
      if (!fs.isEmpty()) {
        fs += ", ";
      }

      fs += f.displayName.isEmpty() ? f.name : f.displayName;
    }

    s += fs + "**\n";
  }

  if (!missingMasters.empty()) {
    s += " - **" + QObject::tr("Missing masters") + ": ";

    QString ms;
    for (auto&& m : missingMasters) {
      if (!ms.isEmpty()) {
        ms += ", ";
      }

      ms += m;
    }

    s += ms + "**\n";
  }

  for (auto&& m : messages) {
    s += " - " + m.toMarkdown() + "\n";
  }

  for (auto&& d : dirty) {
    s += " - " + d.toMarkdown(false) + "\n";
  }

  if (!s.isEmpty()) {
    s = "#### " + name + "\n" + s;
  }

  return s;
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

QString Loot::Dirty::toMarkdown(bool isClean) const
{
  return toString(isClean);
}

QString Loot::Dirty::cleaningString() const
{
  return QObject::tr("%1 found %2 ITM record(s), %3 deleted reference(s) and %4 deleted navmesh(es).")
    .arg(cleaningUtility.isEmpty() ? "?" : cleaningUtility)
    .arg(itm)
    .arg(deletedReferences)
    .arg(deletedNavmesh);
}

QString Loot::Message::toMarkdown() const
{
  QString s;

  switch (type)
  {
    case log::Error:
    {
      s += "**" + QObject::tr("Error") + "**: ";
      break;
    }

    case log::Warning:
    {
      s += "**" + QObject::tr("Warning") + "**: ";
      break;
    }

    default:
    {
      break;
    }
  }

  s += text;

  return s;
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

  if (QFile::exists(LootReportPath)) {
    log::debug("deleting temporary loot report '{}'", LootReportPath);
    const auto r = shell::Delete(LootReportPath);

    if (!r) {
      log::error(
        "failed to remove temporary loot json report '{}': {}",
        LootReportPath, r.toString());
    }
  }
}

bool Loot::start(QWidget* parent, OrganizerCore& core, bool didUpdateMasterList)
{
  log::debug("starting loot");

  // creating pipe
  env::HandlePtr out(createPipe());
  if (out.get() == INVALID_HANDLE_VALUE) {
    return false;
  }

  // vfs
  core.prepareVFS();

  // spawning
  if (!spawnLootcli(parent, core, didUpdateMasterList, out.get())) {
    return false;
  }

  // starting thread
  log::debug("starting loot thread");
  m_thread.reset(QThread::create([&]{ lootThread(); }));
  m_thread->start();

  return true;
}

bool Loot::spawnLootcli(
  QWidget* parent, OrganizerCore& core, bool didUpdateMasterList,
  HANDLE stdoutHandle)
{
  const auto logLevel = core.settings().diagnostics().lootLogLevel();

  QStringList parameters;
  parameters
    << "--game" << core.managedGame()->gameShortName()
    << "--gamePath" << QString("\"%1\"").arg(core.managedGame()->gameDirectory().absolutePath())
    << "--pluginListPath" << QString("\"%1/loadorder.txt\"").arg(core.profilePath())
    << "--logLevel" << QString::fromStdString(lootcli::logLevelToString(logLevel))
    << "--out" << QString("\"%1\"").arg(LootReportPath);

  if (didUpdateMasterList) {
    parameters << "--skipUpdateMasterlist";
  }

  spawn::SpawnParameters sp;
  sp.binary = QFileInfo(qApp->applicationDirPath() + "/loot/lootcli.exe");
  sp.arguments = parameters.join(" ");
  sp.currentDirectory.setPath(qApp->applicationDirPath() + "/loot");
  sp.hooked = true;
  sp.stdOut = stdoutHandle;

  HANDLE lootHandle = spawn::startBinary(parent, sp);
  if (lootHandle == INVALID_HANDLE_VALUE) {
    emit log(log::Levels::Error, tr("failed to start loot"));
    return false;
  }

  m_lootProcess.reset(lootHandle);

  return true;
}

HANDLE Loot::createPipe()
{
  static const wchar_t* PipeName = L"\\\\.\\pipe\\lootcli_pipe";

  SECURITY_ATTRIBUTES sa = {};
  sa.nLength = sizeof(SECURITY_ATTRIBUTES);
  sa.bInheritHandle = TRUE;

  env::HandlePtr pipe;

  // creating pipe
  {
    HANDLE pipeHandle = ::CreateNamedPipe(
      PipeName, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE|PIPE_READMODE_BYTE|PIPE_WAIT,
      1, 50'000, 50'000, 0, &sa);

    if (pipeHandle == INVALID_HANDLE_VALUE) {
      const auto e = GetLastError();
      log::error("CreateNamedPipe failed, {}", formatSystemMessage(e));
      return INVALID_HANDLE_VALUE;
    }

    pipe.reset(pipeHandle);
  }

  {
    // duplicating the handle to read from it
    HANDLE outputRead = INVALID_HANDLE_VALUE;

    const auto r = DuplicateHandle(
      GetCurrentProcess(), pipe.get(), GetCurrentProcess(), &outputRead,
      0, TRUE, DUPLICATE_SAME_ACCESS);

    if (!r) {
      const auto e = GetLastError();
      log::error("DuplicateHandle for pipe failed, {}", formatSystemMessage(e));
      return INVALID_HANDLE_VALUE;
    }

    m_stdout.reset(outputRead);
  }


  // creating handle to pipe which is passed to CreateProcess()
  HANDLE outputWrite = ::CreateFileW(
    PipeName, FILE_WRITE_DATA|SYNCHRONIZE, 0,
    &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

  if (outputWrite == INVALID_HANDLE_VALUE) {
    const auto e = GetLastError();
    log::error("CreateFileW for pipe failed, {}", formatSystemMessage(e));
    return INVALID_HANDLE_VALUE;
  }

  return outputWrite;
}

void Loot::cancel()
{
  if (!m_cancel) {
    log::debug("loot received cancel request");
    m_cancel = true;
  }
}

bool Loot::result() const
{
  return m_result;
}

const QString& Loot::outPath() const
{
  return LootReportPath;
}

const Loot::Report& Loot::report() const
{
  return m_report;
}

void Loot::lootThread()
{
  ::SetThreadDescription(GetCurrentThread(), L"loot");

  try
  {
    m_result = false;

    if (!waitForCompletion()) {
      return;
    }

    m_result = true;
    processOutputFile();
  }
  catch(...)
  {
    log::error("unhandled exception in loot thread");
  }

  log::debug("finishing loot thread");
  emit finished();
}

bool Loot::waitForCompletion()
{
  bool terminating = false;

  log::debug("loot thread waiting for completion on lootcli");

  for (;;) {
    DWORD res = WaitForSingleObject(m_lootProcess.get(), 100);

    if (res == WAIT_OBJECT_0) {
      log::debug("lootcli has completed");
      // done
      break;
    }

    if (res == WAIT_FAILED) {
      const auto e = GetLastError();
      log::error("failed to wait on loot process, {}", formatSystemMessage(e));
      return false;
    }

    if (m_cancel) {
      log::debug("terminating lootcli process");
      ::TerminateProcess(m_lootProcess.get(), 1);

      log::debug("waiting for loocli process to terminate");
      WaitForSingleObject(m_lootProcess.get(), INFINITE);

      log::debug("lootcli terminated");
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
  static const std::size_t bufferSize = 50'000;

  char buffer[bufferSize] = {};

  DWORD bytesRead = 0;
  if (::ReadFile(m_stdout.get(), buffer, bufferSize, &bytesRead, nullptr)) {
    return {buffer, buffer + bytesRead};
  } else {
    const auto e = GetLastError();

    // broken pipe probably means lootcli is finished
    if (e != ERROR_BROKEN_PIPE) {
      log::error("{}", formatSystemMessage(e));
    }

    return {};
  }
}

void Loot::processStdout(const std::string &lootOut)
{
  emit output(QString::fromStdString(lootOut));

  m_outputBuffer += lootOut;
  if (m_outputBuffer.empty()) {
    return;
  }

  log::debug("loot: processing stdout ({} bytes)", m_outputBuffer.size());

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
  log::debug("parsing json output file at '{}'", LootReportPath);

  QFile outFile(LootReportPath);
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
  r.stats = reportStats(getWarn<QJsonObject>(object, "stats"));

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

Loot::Stats Loot::reportStats(const QJsonObject& stats) const
{
  Stats s;

  s.time = getWarn<qint64>(stats, "time");
  s.lootcliVersion = getWarn<QString>(stats, "lootcliVersion");
  s.lootVersion = getWarn<QString>(stats, "lootVersion");

  return s;
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
  core.savePluginList();

  try {
    Loot loot;
    LootDialog dialog(parent, core, loot);

    loot.start(parent, core, didUpdateMasterList);
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

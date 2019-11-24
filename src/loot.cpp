#include "loot.h"
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


class LootDialog : public QDialog
{
public:
  LootDialog(QWidget* parent, OrganizerCore& core, Loot& loot) :
    QDialog(parent), m_core(core), m_loot(loot),
    m_label(nullptr), m_progress(nullptr), m_buttons(nullptr),
    m_report(nullptr), m_output(nullptr),
    m_finished(false), m_cancelling(false)
  {
    createUI();
    m_progress->setMaximum(0);

    QObject::connect(
      &m_loot, &Loot::output, this,
      [&](auto&& s){ addOutput(s); }, Qt::QueuedConnection);

    QObject::connect(
      &m_loot, &Loot::progress,
      this, [&](auto&& p){ setProgress(p); }, Qt::QueuedConnection);

    QObject::connect(
      &m_loot, &Loot::log, this,
      [&](auto&& lv, auto&& s){ log(lv, s); }, Qt::QueuedConnection);

    QObject::connect(
      &m_loot, &Loot::finished, this,
      [&]{ onFinished(); }, Qt::QueuedConnection);
  }

  void setText(const QString& s)
  {
    m_label->setText(s);
  }

  void setProgress(lootcli::Progress p)
  {
    setText(progressToString(p));

    if (p == lootcli::Progress::Done) {
      m_progress->setRange(0, 1);
      m_progress->setValue(1);
    }
  }

  QString progressToString(lootcli::Progress p)
  {
    using P = lootcli::Progress;

    switch (p)
    {
      case P::CheckingMasterlistExistence: return tr("Checking masterlist existence");
      case P::UpdatingMasterlist: return tr("Updating masterlist");
      case P::LoadingLists: return tr("Loading lists");
      case P::ReadingPlugins: return tr("Reading plugins");
      case P::SortingPlugins: return tr("Sorting plugins");
      case P::WritingLoadorder: return tr("Writing loadorder.txt");
      case P::ParsingLootMessages: return tr("Parsing loot messages");
      case P::Done: return tr("Done");
      default: return QString("unknown progress %1").arg(static_cast<int>(p));
    }
  }

  void addOutput(const QString& s)
  {
    if (m_core.settings().diagnostics().lootLogLevel() > lootcli::LogLevels::Debug) {
      return;
    }

    const auto lines = s.split(QRegExp("[\\r\\n]"), QString::SkipEmptyParts);

    for (auto&& line : lines) {
      if (line.isEmpty()) {
        continue;
      }

      addLineOutput(line);
    }
  }

  bool result() const
  {
    return m_loot.result();
  }

  void cancel()
  {
    if (!m_finished && !m_cancelling) {
      addLineOutput(tr("Stopping LOOT..."));
      m_loot.cancel();
      m_buttons->setEnabled(false);
      m_cancelling = true;
    }
  }

  int exec() override
  {
    return QDialog::exec();
  }

  void openReport()
  {
    const auto path = m_loot.outPath();
    shell::Open(path);
  }

private:
  OrganizerCore& m_core;
  Loot& m_loot;
  QLabel* m_label;
  QProgressBar* m_progress;
  QDialogButtonBox* m_buttons;
  QPushButton* m_report;
  QPlainTextEdit* m_output;
  bool m_finished;
  bool m_cancelling;

  void createUI()
  {
    auto* root = new QWidget(this);
    auto* ly = new QVBoxLayout(root);

    setLayout(new QVBoxLayout);
    layout()->setContentsMargins(0, 0, 0, 0);
    layout()->addWidget(root);

    m_label = new QLabel;
    ly->addWidget(m_label);

    m_progress = new QProgressBar;
    ly->addWidget(m_progress);

    auto* more = createMoreUI();
    ly->addWidget(more);

    resize(700, 400);
  }

  QWidget* createMoreUI()
  {
    auto* more = new QWidget;
    auto* ly = new QVBoxLayout(more);
    ly->setContentsMargins(0, 0, 0, 0);

    auto* buttons = new QHBoxLayout;
    buttons->setContentsMargins(0, 0, 0, 0);
    m_report = new QPushButton(tr("Open JSON report"));
    m_report->setEnabled(false);
    connect(m_report, &QPushButton::clicked, [&]{ openReport(); });
    buttons->addWidget(m_report);
    buttons->addStretch(1);
    ly->addLayout(buttons);

    m_output = new QPlainTextEdit;
    ly->addWidget(m_output);

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Cancel);
    connect(m_buttons, &QDialogButtonBox::clicked, [&](auto* b){ onButton(b); });
    ly->addWidget(m_buttons);

    return more;
  }

  void closeEvent(QCloseEvent* e) override
  {
    if (m_finished) {
      QDialog::closeEvent(e);
    } else {
      cancel();
      e->ignore();
    }
  }

  void onButton(QAbstractButton* b)
  {
    if (m_buttons->buttonRole(b) == QDialogButtonBox::RejectRole) {
      if (m_finished) {
        close();
      } else {
        cancel();
      }
    }
  }

  void addLineOutput(const QString& line)
  {
    m_output->appendPlainText(line);
  }

  void onFinished()
  {
    m_finished = true;

    if (m_cancelling) {
      close();
    } else {
      handleReport();
      m_report->setEnabled(true);
      m_buttons->setStandardButtons(QDialogButtonBox::Close);
    }
  }

  void log(log::Levels lv, const QString& s)
  {
    if (lv >= log::Levels::Warning) {
      log::log(lv, "{}", s);
    }

    if (m_core.settings().diagnostics().lootLogLevel() > lootcli::LogLevels::Debug) {
      addLineOutput(QString("[%1] %2").arg(log::levelToString(lv)).arg(s));
    }
  }

  void handleReport()
  {
    const auto& report = m_loot.report();

    if (!report.messages.empty()) {
      addLineOutput("");
    }

    for (auto&& m : report.messages) {
      log(levelFromLoot(
        lootcli::logLevelFromString(m.type.toStdString())),
        m.text);
    }

    for (auto&& p : report.plugins) {
      for (auto&& d : p.dirty) {
        m_core.pluginList()->addInformation(p.name, d.toString(false));
      }
    }
  }
};


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
  m_thread->wait();

  if (!m_outPath.isEmpty()) {
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
  /*static const std::regex exRequires("\"([^\"]*)\" requires \"([^\"]*)\", but it is missing\\.");
  static const std::regex exIncompatible("\"([^\"]*)\" is incompatible with \"([^\"]*)\", but both are present\\.");

  switch (m.type)
  {
    case lootcli::MessageType::Log:
    {
      if (m.logLevel == lootcli::LogLevels::Error) {
        std::smatch match;

        if (std::regex_match(m.log, match, exRequires)) {
          std::string modName(match[1].first, match[1].second);
          std::string dependency(match[2].first, match[2].second);
          emit information(
            QString::fromStdString(modName),
            tr("depends on missing \"%1\"").arg(dependency.c_str()));
        } else if (std::regex_match(m.log, match, exIncompatible)) {
          std::string modName(match[1].first, match[1].second);
          std::string dependency(match[2].first, match[2].second);
          emit information(
            QString::fromStdString(modName),
            tr("incompatible with \"%1\"").arg(dependency.c_str()));
        } else {
          emit log(levelFromLoot(m.logLevel), QString::fromStdString(m.log));
        }
      } else {
        emit log(levelFromLoot(m.logLevel), QString::fromStdString(m.log));
      }

      break;
    }

    case lootcli::MessageType::Progress:
    {
      emit progress(m.progress);
      break;
    }
  }*/
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
    m.type = getWarn<QString>(o, "type");
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

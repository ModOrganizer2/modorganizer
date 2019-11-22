#include "loot.h"
#include "spawn.h"
#include "organizercore.h"
#include <log.h>
#include <report.h>

using namespace MOBase;

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
      &m_loot, &Loot::information, this,
      [&](auto&& mod, auto&& i){ setInfo(mod, i); }, Qt::QueuedConnection);

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

  void setInfo(const QString& mod, const QString& info)
  {
    m_core.pluginList()->addInformation(mod.toStdString().c_str(), info);
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
    m_output->setWordWrapMode(QTextOption::NoWrap);
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
};


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
  static const std::regex exRequires("\"([^\"]*)\" requires \"([^\"]*)\", but it is missing\\.");
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
  }
}

QString jsonType(const QJsonValue& v)
{
  if (v.isUndefined()) {
    return "undefined";
  } else if (v.isNull()) {
    return "null";
  } else if (v.isArray()) {
    return "an array";
  } else if (v.isBool()) {
    return "a bool";
  } else if (v.isDouble()) {
    return "a double";
  } else if (v.isObject()) {
    return "an object";
  } else if (v.isString()) {
    return "a string";
  } else {
    return "an unknown type";
  }
}

QString jsonType(const QJsonDocument& doc)
{
  if (doc.isEmpty()) {
    return "empty";
  } else if (doc.isNull()) {
    return "null";
  } else if (doc.isArray()) {
    return "an array";
  } else if (doc.isObject()) {
    return "an object";
  } else {
    return "an unknown type";
  }
}

void Loot::processOutputFile()
{
  QFile outFile(m_outPath);
  if (!outFile.open(QIODevice::ReadOnly)) {
    logJsonError(
      "failed to open file, {} (error {})",
      outFile.errorString(), outFile.error());

    return;
  }

  QJsonParseError e;
  const QJsonDocument doc = QJsonDocument::fromJson(outFile.readAll(), &e);
  if (doc.isNull()) {
    logJsonError("invalid json, {} (error {})", e.errorString(), e.error);
    return;
  }

  if (!doc.isObject()) {
    logJsonError("root is {}, not an object", jsonType(doc));
    return;
  }

  const QJsonObject object = doc.object();

  if (object.contains("messages")) {
    const auto messagesValue = object["messages"];

    if (messagesValue.isArray()) {
      processMessages(messagesValue.toArray());
    } else {
      logJsonError(
        "'messages' property is {}, not an array", jsonType(messagesValue));
    }

  }

  if (object.contains("plugins")) {
    const auto pluginsValue = object["plugins"];

    if (pluginsValue.isArray()) {
      processPlugins(pluginsValue.toArray());
    } else {
      logJsonError(
        "'plugins' property is {}, not an array", jsonType(pluginsValue));
    }
  }
}

bool Loot::processMessages(const QJsonArray& messages)
{
  for (auto messageValue : messages) {
    if (messageValue.isObject()) {
      processMessage(messageValue.toObject());
    } else {
      logJsonError("a message is {}, not an object", jsonType(messageValue));
    }
  }

  return true;
}

bool Loot::processMessage(const QJsonObject& message)
{
  const auto messageType = message["type"].toString();
  const auto messageString = message["message"].toString();

  if (messageType.isEmpty()) {
    logJsonError("there's a message with no 'type' property");
    return false;
  }

  if (messageString.isEmpty()) {
    logJsonError("there's a message with no 'message' property");
    return false;
  }

  emit log(levelFromLoot(
    lootcli::logLevelFromString(messageType.toStdString())),
    messageString);

  return true;
}

bool Loot::processPlugins(const QJsonArray& plugins)
{
  for (auto pluginValue : plugins) {
    if (pluginValue.isObject()) {
      processPlugin(pluginValue.toObject());
    } else {
      logJsonError("a plugin is {}, not an object", jsonType(pluginValue));
    }
  }

  return true;
}

bool Loot::processPlugin(const QJsonObject& plugin)
{
  if (!plugin.contains("name")) {
    logJsonError("plugin missing 'name' property");
    return false;
  }

  const auto nameValue = plugin["name"];
  if (!nameValue.isString()) {
    logJsonError("plugin property 'name' is {}, not a string", jsonType(nameValue));
    return false;
  }

  const auto name = nameValue.toString();

  processPluginDirty(name, plugin);

  return true;
}


bool Loot::processPluginDirty(const QString& name, const QJsonObject& plugin)
{
  if (!plugin.contains("dirty")) {
    return true;
  }

  const auto dirtyValue = plugin["dirty"];

  if (!dirtyValue.isArray()) {
    logJsonError(
      "'dirty' value for plugin '{}' is {}, not an array",
      name, jsonType(dirtyValue));

    return false;
  }

  const auto dirty = dirtyValue.toArray();


  for (auto stringValue : dirty) {
    if (!stringValue.isString()) {
      logJsonError(
        "'dirty' value for plugin '{}' is {}, not a string",
        name, jsonType(stringValue));

      continue;
    }

    const auto string = stringValue.toString();

    if (string.isEmpty()) {
      logJsonError("'dirty' string for plugin '{}' is empty", name);
      continue;
    }

    emit information(name, string);
  }

  return true;
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

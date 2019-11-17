#include "loot.h"
#include "spawn.h"
#include "organizercore.h"
#include <log.h>
#include <report.h>

using namespace MOBase;


class LootDialog : public QDialog
{
public:
  LootDialog(QWidget* parent, OrganizerCore& core, Loot& loot) :
    QDialog(parent), m_core(core), m_loot(loot),
    m_label(nullptr), m_progress(nullptr), m_buttons(nullptr), m_finished(false)
  {
    createUI();

    QObject::connect(
      &m_loot, &Loot::output, this,
      [&](auto&& s){ addOutput(s); }, Qt::QueuedConnection);

    QObject::connect(
      &m_loot, &Loot::progress,
      this, [&](auto&& s){ setText(s); }, Qt::QueuedConnection);

    QObject::connect(
      &m_loot, &Loot::information, this,
      [&](auto&& mod, auto&& i){ setInfo(mod, i); }, Qt::QueuedConnection);

    QObject::connect(
      &m_loot, &Loot::errorMessage, this,
      [&](auto&& s){ onErrorMessage(s); }, Qt::QueuedConnection);

    QObject::connect(
      &m_loot, &Loot::error, this,
      [&](auto&& s){ onError(s); }, Qt::QueuedConnection);

    QObject::connect(
      &m_loot, &Loot::finished, this,
      [&]{ onFinished(); }, Qt::QueuedConnection);
  }

  void setText(const QString& s)
  {
    m_label->setText(s);
  }

  void setIndeterminate()
  {
    m_progress->setMaximum(0);
  }

  void addOutput(const QString& s)
  {
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
    m_loot.cancel();
  }

  int exec() override
  {
    m_loot.start();
    QDialog::exec();

    if (m_errorMessages.length() > 0) {
      QMessageBox *warn = new QMessageBox(
        QMessageBox::Warning, QObject::tr("Errors occurred"),
        m_errorMessages, QMessageBox::Ok, parentWidget());

      warn->exec();
    }

    return 0;
  }

  void onError(const QString& s)
  {
    reportError(s);
  }

private:
  OrganizerCore& m_core;
  Loot& m_loot;
  QLabel* m_label;
  QProgressBar* m_progress;
  QDialogButtonBox* m_buttons;
  QPlainTextEdit* m_output;
  QString m_lastLine;
  QString m_errorMessages;
  bool m_finished;

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

    m_output = new QPlainTextEdit;
    ly->addWidget(m_output);

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Cancel);
    connect(m_buttons, &QDialogButtonBox::clicked, [&](auto* b){ onButton(b); });
    ly->addWidget(m_buttons);
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
      cancel();
    }
  }

  void addLineOutput(const QString& line)
  {
    if (line == m_lastLine) {
      return;
    }

    m_output->appendPlainText(line);
    m_lastLine = line;
  }

  void onFinished()
  {
    m_finished = true;
    close();
  }

  void onErrorMessage(const QString& s)
  {
    m_errorMessages += s;
  }
};


Loot::Loot(QWidget* parent, OrganizerCore& core, bool didUpdateMasterList) :
  m_thread(nullptr), m_cancel(false), m_result(false),
  m_lootProcess(INVALID_HANDLE_VALUE), m_stdOutRead(INVALID_HANDLE_VALUE)
{
  m_outPath = QDir::temp().absoluteFilePath("lootreport.json");

  QStringList parameters;
  parameters
    << "--game" << core.managedGame()->gameShortName()
    << "--gamePath" << QString("\"%1\"").arg(core.managedGame()->gameDirectory().absolutePath())
    << "--pluginListPath" << QString("\"%1/loadorder.txt\"").arg(core.profilePath())
    << "--out" << QString("\"%1\"").arg(m_outPath);

  if (didUpdateMasterList) {
    parameters << "--skipUpdateMasterlist";
  }

  HANDLE stdOutWrite = INVALID_HANDLE_VALUE;
  createStdoutPipe(&m_stdOutRead, &stdOutWrite);

  core.prepareVFS();

  spawn::SpawnParameters sp;
  sp.binary = QFileInfo(qApp->applicationDirPath() + "/loot/lootcli.exe");
  sp.arguments = parameters.join(" ");
  sp.currentDirectory.setPath(qApp->applicationDirPath() + "/loot");
  sp.hooked = true;
  sp.stdOut = stdOutWrite;

  m_lootProcess = spawn::startBinary(parent, sp);

  // we don't use the write end
  ::CloseHandle(stdOutWrite);

  core.pluginList()->clearAdditionalInformation();

  m_thread.reset(QThread::create([&]{
    lootThread();
    emit finished();
  }));
}

Loot::~Loot()
{
  m_thread->wait();
}

void Loot::start()
{
  m_thread->start();
}

void Loot::cancel()
{
  m_cancel = true;
}

bool Loot::result() const
{
  return m_result;
}

void Loot::createStdoutPipe(HANDLE *stdOutRead, HANDLE *stdOutWrite)
{
  SECURITY_ATTRIBUTES secAttributes;
  secAttributes.nLength = sizeof(SECURITY_ATTRIBUTES);
  secAttributes.bInheritHandle = TRUE;
  secAttributes.lpSecurityDescriptor = nullptr;

  if (!::CreatePipe(stdOutRead, stdOutWrite, &secAttributes, 0)) {
    log::error("failed to create stdout reroute");
  }

  if (!::SetHandleInformation(*stdOutRead, HANDLE_FLAG_INHERIT, 0)) {
    log::error("failed to correctly set up the stdout reroute");
    *stdOutWrite = *stdOutRead = INVALID_HANDLE_VALUE;
  }
}

std::string Loot::readFromPipe(HANDLE stdOutRead)
{
  static const int chunkSize = 128;
  std::string result;

  char buffer[chunkSize + 1];
  buffer[chunkSize] = '\0';

  DWORD read = 1;
  while (read > 0) {
    if (!::ReadFile(stdOutRead, buffer, chunkSize, &read, nullptr)) {
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

void Loot::processLOOTOut(const std::string &lootOut)
{
  emit output(QString::fromStdString(lootOut));

  std::vector<std::string> lines;
  boost::split(lines, lootOut, boost::is_any_of("\r\n"));

  std::regex exRequires("\"([^\"]*)\" requires \"([^\"]*)\", but it is missing\\.");
  std::regex exIncompatible("\"([^\"]*)\" is incompatible with \"([^\"]*)\", but both are present\\.");

  for (const std::string &line : lines) {
    if (line.length() > 0) {
      size_t progidx    = line.find("[progress]");
      size_t erroridx   = line.find("[error]");
      if (progidx != std::string::npos) {
        emit progress(line.substr(progidx + 11).c_str());
      } else if (erroridx != std::string::npos) {
        log::warn("{}", line);
        emit errorMessage(QString::fromStdString(
          boost::algorithm::trim_copy(line.substr(erroridx + 8)) + "\n"));
      } else {
        std::smatch match;
        if (std::regex_match(line, match, exRequires)) {
          std::string modName(match[1].first, match[1].second);
          std::string dependency(match[2].first, match[2].second);
          emit information(
            QString::fromStdString(modName),
            QObject::tr("depends on missing \"%1\"").arg(dependency.c_str()));
        } else if (std::regex_match(line, match, exIncompatible)) {
          std::string modName(match[1].first, match[1].second);
          std::string dependency(match[2].first, match[2].second);
          emit information(
            QString::fromStdString(modName),
            QObject::tr("incompatible with \"%1\"").arg(dependency.c_str()));
        } else {
          log::debug("[loot] {}", line);
        }
      }
    }
  }
}

void Loot::lootThread()
{
  try {
    m_result = false;

    DWORD retLen;
    JOBOBJECT_BASIC_PROCESS_ID_LIST info;
    HANDLE processHandle = m_lootProcess;

    if (m_lootProcess != INVALID_HANDLE_VALUE) {
      bool isJobHandle = true;
      ULONG lastProcessID;
      DWORD res = ::MsgWaitForMultipleObjects(1, &m_lootProcess, false, 100, QS_KEY | QS_MOUSE);
      while ((res != WAIT_FAILED) && (res != WAIT_OBJECT_0)) {
        if (isJobHandle) {
          if (::QueryInformationJobObject(m_lootProcess, JobObjectBasicProcessIdList, &info, sizeof(info), &retLen) > 0) {
            if (info.NumberOfProcessIdsInList == 0) {
              log::debug("no more processes in job");
              break;
            } else {
              if (lastProcessID != info.ProcessIdList[0]) {
                lastProcessID = info.ProcessIdList[0];
                if (processHandle != m_lootProcess) {
                  ::CloseHandle(processHandle);
                }
                processHandle = ::OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, lastProcessID);
              }
            }
          } else {
            // the info-object I passed only provides space for 1 process id. but since this code only cares about whether there
            // is more than one that's good enough. ERROR_MORE_DATA simply signals there are at least two processes running.
            // any other error probably means the handle is a regular process handle, probably caused by running MO in a job without
            // the right to break out.
            if (::GetLastError() != ERROR_MORE_DATA) {
              isJobHandle = false;
            }
          }
        }

        if (m_cancel) {
          if (isJobHandle) {
            ::TerminateJobObject(m_lootProcess, 1);
          } else {
            ::TerminateProcess(m_lootProcess, 1);
          }
        }

        // keep processing events so the app doesn't appear dead
        QCoreApplication::processEvents();
        std::string lootOut = readFromPipe(m_stdOutRead);
        processLOOTOut(lootOut);

        res = ::MsgWaitForMultipleObjects(1, &m_lootProcess, false, 100, QS_KEY | QS_MOUSE);
      }

      std::string remainder = readFromPipe(m_stdOutRead).c_str();
      if (remainder.length() > 0) {
        processLOOTOut(remainder);
      }

      DWORD exitCode = 0UL;
      ::GetExitCodeProcess(processHandle, &exitCode);
      ::CloseHandle(processHandle);
      if (exitCode != 0UL) {
        emit error(QObject::tr("loot failed. Exit code was: %1").arg(exitCode));
        m_result = false;
        return;
      } else {
        m_result = true;
        QFile outFile(m_outPath);
        outFile.open(QIODevice::ReadOnly);
        QJsonDocument doc = QJsonDocument::fromJson(outFile.readAll());
        QJsonArray array = doc.array();
        for (auto iter = array.begin();  iter != array.end(); ++iter) {
          QJsonObject pluginObj = (*iter).toObject();
          QJsonArray pluginMessages = pluginObj["messages"].toArray();
          for (auto msgIter = pluginMessages.begin(); msgIter != pluginMessages.end(); ++msgIter) {
            QJsonObject msg = (*msgIter).toObject();
            emit information(
              pluginObj["name"].toString(),
              QString("%1: %2").arg(msg["type"].toString(), msg["message"].toString()));
          }
          if (pluginObj["dirty"].toString() == "yes") {
            emit information(pluginObj["name"].toString(), "dirty");
          }
        }
      }
    } else {
      emit error(QObject::tr("failed to start loot"));
    }
  } catch (const std::exception &e) {
    emit error(QObject::tr("failed to run loot: %1").arg(e.what()));
  }
}


bool runLoot(QWidget* parent, OrganizerCore& core, bool didUpdateMasterList)
{
  //m_OrganizerCore.currentProfile()->writeModlistNow();
  core.savePluginList();

  //Create a backup of the load orders w/ LOOT in name
  //to make sure that any sorting is easily undo-able.
  //Need to figure out how I want to do that.

  try {
    Loot loot(parent, core, didUpdateMasterList);
    LootDialog dialog(parent, core, loot);

    dialog.setText(QObject::tr("Please wait while LOOT is running"));
    dialog.setIndeterminate();
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

#include "spawn.h"
#include "organizercore.h"
#include <log.h>
#include <report.h>

using namespace MOBase;

void createStdoutPipe(HANDLE *stdOutRead, HANDLE *stdOutWrite)
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

std::string readFromPipe(HANDLE stdOutRead)
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

void processLOOTOut(
  OrganizerCore& core,
  const std::string &lootOut, std::string &errorMessages,
  QProgressDialog &dialog)
{
  std::vector<std::string> lines;
  boost::split(lines, lootOut, boost::is_any_of("\r\n"));

  std::regex exRequires("\"([^\"]*)\" requires \"([^\"]*)\", but it is missing\\.");
  std::regex exIncompatible("\"([^\"]*)\" is incompatible with \"([^\"]*)\", but both are present\\.");

  for (const std::string &line : lines) {
    if (line.length() > 0) {
      size_t progidx    = line.find("[progress]");
      size_t erroridx   = line.find("[error]");
      if (progidx != std::string::npos) {
        dialog.setLabelText(line.substr(progidx + 11).c_str());
      } else if (erroridx != std::string::npos) {
        log::warn("{}", line);
        errorMessages.append(boost::algorithm::trim_copy(line.substr(erroridx + 8)) + "\n");
      } else {
        std::smatch match;
        if (std::regex_match(line, match, exRequires)) {
          std::string modName(match[1].first, match[1].second);
          std::string dependency(match[2].first, match[2].second);
          core.pluginList()->addInformation(modName.c_str(), QObject::tr("depends on missing \"%1\"").arg(dependency.c_str()));
        } else if (std::regex_match(line, match, exIncompatible)) {
          std::string modName(match[1].first, match[1].second);
          std::string dependency(match[2].first, match[2].second);
          core.pluginList()->addInformation(modName.c_str(), QObject::tr("incompatible with \"%1\"").arg(dependency.c_str()));
        } else {
          log::debug("[loot] {}", line);
        }
      }
    }
  }
}

bool runLoot(QWidget* parent, OrganizerCore& core, bool didUpdateMasterList)
{
  std::string errorMessages;

  //m_OrganizerCore.currentProfile()->writeModlistNow();
  core.savePluginList();

  //Create a backup of the load orders w/ LOOT in name
  //to make sure that any sorting is easily undo-able.
  //Need to figure out how I want to do that.

  bool success = false;

  try {
    QProgressDialog dialog(parent);

    dialog.setLabelText(QObject::tr("Please wait while LOOT is running"));
    dialog.setMaximum(0);
    dialog.show();

    QString outPath = QDir::temp().absoluteFilePath("lootreport.json");

    QStringList parameters;
    parameters
      << "--game" << core.managedGame()->gameShortName()
      << "--gamePath" << QString("\"%1\"").arg(core.managedGame()->gameDirectory().absolutePath())
      << "--pluginListPath" << QString("\"%1/loadorder.txt\"").arg(core.profilePath())
      << "--out" << QString("\"%1\"").arg(outPath);

    if (didUpdateMasterList) {
      parameters << "--skipUpdateMasterlist";
    }

    HANDLE stdOutWrite = INVALID_HANDLE_VALUE;
    HANDLE stdOutRead = INVALID_HANDLE_VALUE;
    createStdoutPipe(&stdOutRead, &stdOutWrite);

    try {
      core.prepareVFS();
    } catch (const UsvfsConnectorException &e) {
      log::debug("{}", e.what());
      return false;
    } catch (const std::exception &e) {
      QMessageBox::warning(parent, QObject::tr("Error"), e.what());
      return false;
    }

    spawn::SpawnParameters sp;
    sp.binary = QFileInfo(qApp->applicationDirPath() + "/loot/lootcli.exe");
    sp.arguments = parameters.join(" ");
    sp.currentDirectory.setPath(qApp->applicationDirPath() + "/loot");
    sp.hooked = true;
    sp.stdOut = stdOutWrite;

    HANDLE loot = spawn::startBinary(parent, sp);

    // we don't use the write end
    ::CloseHandle(stdOutWrite);

    core.pluginList()->clearAdditionalInformation();

    DWORD retLen;
    JOBOBJECT_BASIC_PROCESS_ID_LIST info;
    HANDLE processHandle = loot;

    if (loot != INVALID_HANDLE_VALUE) {
      bool isJobHandle = true;
      ULONG lastProcessID;
      DWORD res = ::MsgWaitForMultipleObjects(1, &loot, false, 100, QS_KEY | QS_MOUSE);
      while ((res != WAIT_FAILED) && (res != WAIT_OBJECT_0)) {
        if (isJobHandle) {
          if (::QueryInformationJobObject(loot, JobObjectBasicProcessIdList, &info, sizeof(info), &retLen) > 0) {
            if (info.NumberOfProcessIdsInList == 0) {
              log::debug("no more processes in job");
              break;
            } else {
              if (lastProcessID != info.ProcessIdList[0]) {
                lastProcessID = info.ProcessIdList[0];
                if (processHandle != loot) {
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

        if (dialog.wasCanceled()) {
          if (isJobHandle) {
            ::TerminateJobObject(loot, 1);
          } else {
            ::TerminateProcess(loot, 1);
          }
        }

        // keep processing events so the app doesn't appear dead
        QCoreApplication::processEvents();
        std::string lootOut = readFromPipe(stdOutRead);
        processLOOTOut(core, lootOut, errorMessages, dialog);

        res = ::MsgWaitForMultipleObjects(1, &loot, false, 100, QS_KEY | QS_MOUSE);
      }

      std::string remainder = readFromPipe(stdOutRead).c_str();
      if (remainder.length() > 0) {
        processLOOTOut(core, remainder, errorMessages, dialog);
      }

      DWORD exitCode = 0UL;
      ::GetExitCodeProcess(processHandle, &exitCode);
      ::CloseHandle(processHandle);
      if (exitCode != 0UL) {
        reportError(QObject::tr("loot failed. Exit code was: %1").arg(exitCode));
        return false;
      } else {
        success = true;
        QFile outFile(outPath);
        outFile.open(QIODevice::ReadOnly);
        QJsonDocument doc = QJsonDocument::fromJson(outFile.readAll());
        QJsonArray array = doc.array();
        for (auto iter = array.begin();  iter != array.end(); ++iter) {
          QJsonObject pluginObj = (*iter).toObject();
          QJsonArray pluginMessages = pluginObj["messages"].toArray();
          for (auto msgIter = pluginMessages.begin(); msgIter != pluginMessages.end(); ++msgIter) {
            QJsonObject msg = (*msgIter).toObject();
            core.pluginList()->addInformation(pluginObj["name"].toString(),
              QString("%1: %2").arg(msg["type"].toString(), msg["message"].toString()));
          }
          if (pluginObj["dirty"].toString() == "yes")
            core.pluginList()->addInformation(pluginObj["name"].toString(), "dirty");
        }

      }
    } else {
      reportError(QObject::tr("failed to start loot"));
    }
  } catch (const std::exception &e) {
    reportError(QObject::tr("failed to run loot: %1").arg(e.what()));
  }

  if (errorMessages.length() > 0) {
    QMessageBox *warn = new QMessageBox(
      QMessageBox::Warning, QObject::tr("Errors occurred"),
      errorMessages.c_str(), QMessageBox::Ok, parent);

    warn->setModal(false);
    warn->show();
  }

  return success;
}

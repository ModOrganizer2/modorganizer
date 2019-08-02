#include "filerenamer.h"
#include <log.h>
#include <QMessageBox>
#include <QFileInfo>

using namespace MOBase;

FileRenamer::FileRenamer(QWidget* parent, QFlags<RenameFlags> flags)
  : m_parent(parent), m_flags(flags)
{
  // sanity check for flags
  if ((m_flags & (HIDE|UNHIDE)) == 0) {
    log::error("renameFile() missing hide flag");
    // doesn't really matter, it's just for text
    m_flags = HIDE;
  }
}

FileRenamer::RenameResults FileRenamer::rename(const QString& oldName, const QString& newName)
{
  log::debug("renaming {} to {}", oldName, newName);

  if (QFileInfo(newName).exists()) {
    log::debug("{} already exists", newName);

    // target file already exists, confirm replacement
    auto answer = confirmReplace(newName);

    switch (answer) {
      case DECISION_SKIP: {
        // user wants to skip this file
        log::debug("skipping {}", oldName);
        return RESULT_SKIP;
      }

      case DECISION_REPLACE: {
        log::debug("removing {}", newName);

        // user wants to replace the file, so remove it
        if (!QFile(newName).remove()) {
          log::warn("failed to remove '{}'", newName);
          // removal failed, warn the user and allow canceling
          if (!removeFailed(newName)) {
            log::debug("canceling {}", oldName);
            // user wants to cancel
            return RESULT_CANCEL;
          }

          // ignore this file and continue on
          log::debug("skipping {}", oldName);
          return RESULT_SKIP;
        }

        break;
      }

      case DECISION_CANCEL:  // fall-through
      default: {
        // user wants to stop
        log::debug("canceling");
        return RESULT_CANCEL;
      }
    }
  }

  // target either didn't exist or was removed correctly

  if (!QFile::rename(oldName, newName)) {
    log::warn("failed to rename '{}' to '{}'", oldName, newName);

    // renaming failed, warn the user and allow canceling
    if (!renameFailed(oldName, newName)) {
      // user wants to cancel
      log::debug("canceling");
      return RESULT_CANCEL;
    }

    // ignore this file and continue on
    log::debug("skipping {}", oldName);
    return RESULT_SKIP;
  }

  // everything worked
  log::debug("successfully renamed {} to {}", oldName, newName);
  return RESULT_OK;
}

FileRenamer::RenameDecision FileRenamer::confirmReplace(const QString& newName)
{
  if (m_flags & REPLACE_ALL) {
    // user wants to silently replace all
    log::debug("user has selected replace all");
    return DECISION_REPLACE;
  }
  else if (m_flags & REPLACE_NONE) {
    // user wants to silently skip all
    log::debug("user has selected replace none");
    return DECISION_SKIP;
  }

  QString text;

  if (m_flags & HIDE) {
    text = QObject::tr("The hidden file \"%1\" already exists. Replace it?").arg(newName);
  }
  else if (m_flags & UNHIDE) {
    text = QObject::tr("The visible file \"%1\" already exists. Replace it?").arg(newName);
  }

  auto buttons = QMessageBox::Yes | QMessageBox::No;
  if (m_flags & MULTIPLE) {
    // only show these buttons when there are multiple files to replace
    buttons |= QMessageBox::YesToAll | QMessageBox::NoToAll | QMessageBox::Cancel;
  }

  const auto answer = QMessageBox::question(
    m_parent, QObject::tr("Replace file?"), text, buttons);

  switch (answer) {
    case QMessageBox::Yes:
      log::debug("user wants to replace");
      return DECISION_REPLACE;

    case QMessageBox::No:
      log::debug("user wants to skip");
      return DECISION_SKIP;

    case QMessageBox::YesToAll:
      log::debug("user wants to replace all");
      // remember the answer
      m_flags |= REPLACE_ALL;
      return DECISION_REPLACE;

    case QMessageBox::NoToAll:
      log::debug("user wants to replace none");
      // remember the answer
      m_flags |= REPLACE_NONE;
      return DECISION_SKIP;

    case QMessageBox::Cancel:  // fall-through
    default:
      log::debug("user wants to cancel");
      return DECISION_CANCEL;
  }
}

bool FileRenamer::removeFailed(const QString& name)
{
  QMessageBox::StandardButtons buttons = QMessageBox::Ok;
  if (m_flags & MULTIPLE) {
    // only show cancel for multiple files
    buttons |= QMessageBox::Cancel;
  }

  const auto answer = QMessageBox::critical(
    m_parent, QObject::tr("File operation failed"),
    QObject::tr("Failed to remove \"%1\". Maybe you lack the required file permissions?").arg(name),
    buttons);

  if (answer == QMessageBox::Cancel) {
    // user wants to stop
    log::debug("user wants to cancel");
    return false;
  }

  // skip this one and continue
  log::debug("user wants to skip");
  return true;
}

bool FileRenamer::renameFailed(const QString& oldName, const QString& newName)
{
  QMessageBox::StandardButtons buttons = QMessageBox::Ok;
  if (m_flags & MULTIPLE) {
    // only show cancel for multiple files
    buttons |= QMessageBox::Cancel;
  }

  const auto answer = QMessageBox::critical(
    m_parent, QObject::tr("File operation failed"),
    QObject::tr("failed to rename %1 to %2").arg(oldName).arg(QDir::toNativeSeparators(newName)),
    buttons);

  if (answer == QMessageBox::Cancel) {
    // user wants to stop
    log::debug("user wants to cancel");
    return false;
  }

  // skip this one and continue
  log::debug("user wants to skip");
  return true;
}

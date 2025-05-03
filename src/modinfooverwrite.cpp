#include "modinfooverwrite.h"

#include "settings.h"
#include "shared/appconfig.h"

#include <QApplication>
#include <QDirIterator>
#include "organizercore.h"

ModInfoOverwrite::ModInfoOverwrite(OrganizerCore& core) : ModInfoWithConflictInfo(core)
{}

bool ModInfoOverwrite::isEmpty() const
{
  QDirIterator iter(absolutePath(), QDir::NoDotAndDotDot | QDir::Files | QDir::Dirs);
  if (!iter.hasNext())
    return true;
  while (iter.hasNext()) {
    iter.next();
    if (iter.fileInfo().isDir() &&
        !m_Core.managedGame()->getModMappings().keys().contains(iter.fileName(),
                                                                Qt::CaseInsensitive))
      return false;
    if (iter.fileInfo().isDir() &&
        m_Core.managedGame()->getModMappings().keys().contains(iter.fileName(),
                                                               Qt::CaseInsensitive)) {
      if (QDir(iter.filePath()).count() > 2) {
        return false;
      }
    }
    if (iter.fileInfo().isFile() && iter.fileName() != "meta.ini")
      return false;
  }
  
  return true;
}

QString ModInfoOverwrite::absolutePath() const
{
  return Settings::instance().paths().overwrite();
}

std::vector<ModInfo::EFlag> ModInfoOverwrite::getFlags() const
{
  std::vector<ModInfo::EFlag> result;
  result.push_back(FLAG_OVERWRITE);
  if (m_PluginSelected)
    result.push_back(FLAG_PLUGIN_SELECTED);
  for (auto flag : ModInfoWithConflictInfo::getFlags())
    result.push_back(flag);
  return result;
}

std::vector<ModInfo::EConflictFlag> ModInfoOverwrite::getConflictFlags() const
{
  std::vector<ModInfo::EConflictFlag> result;
  result.push_back(FLAG_OVERWRITE_CONFLICT);
  for (auto flag : ModInfoWithConflictInfo::getConflictFlags())
    result.push_back(flag);
  return result;
}

int ModInfoOverwrite::getHighlight() const
{
  int highlight =
      (isValid() ? HIGHLIGHT_IMPORTANT : HIGHLIGHT_INVALID) | HIGHLIGHT_CENTER;
  auto flags = getFlags();
  if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_PLUGIN_SELECTED) !=
      flags.end())
    highlight |= HIGHLIGHT_PLUGIN;
  return highlight;
}

QString ModInfoOverwrite::getDescription() const
{
  return tr("This pseudo mod contains files from the virtual data tree that got "
            "modified (i.e. by the construction kit)");
}

QStringList ModInfoOverwrite::archives(bool checkOnDisk)
{
  QStringList result;
  QDir dir(this->absolutePath());
  for (const QString& archive : dir.entryList(QStringList({"*.bsa", "*.ba2"}))) {
    result.append(this->absolutePath() + "/" + archive);
  }
  return result;
}

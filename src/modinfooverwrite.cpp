#include "modinfooverwrite.h"

#include "appconfig.h"
#include "settings.h"

#include <QApplication>
#include <QDirIterator>

ModInfoOverwrite::ModInfoOverwrite()
{
  testValid();
}

bool ModInfoOverwrite::isEmpty() const
{
  QDirIterator iter(absolutePath(), QDir::NoDotAndDotDot | QDir::Files | QDir::Dirs);
  if (!iter.hasNext()) return true;
  iter.next();
  if ((iter.fileName() == "meta.ini") && !iter.hasNext()) return true;
  return false;
}

QString ModInfoOverwrite::absolutePath() const
{
  return Settings::instance().getOverwriteDirectory();
}

std::vector<ModInfo::EFlag> ModInfoOverwrite::getFlags() const
{
  std::vector<ModInfo::EFlag> result;
  result.push_back(FLAG_OVERWRITE);
  if (m_PluginSelected)
    result.push_back(FLAG_PLUGIN_SELECTED);
  return result;
}

int ModInfoOverwrite::getHighlight() const
{
  int highlight = (isValid() ? HIGHLIGHT_IMPORTANT : HIGHLIGHT_INVALID) | HIGHLIGHT_CENTER;
  auto flags = getFlags();
  if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_PLUGIN_SELECTED) != flags.end())
    highlight |= HIGHLIGHT_PLUGIN;
  return highlight;
}

QString ModInfoOverwrite::getDescription() const
{
  return tr("This pseudo mod contains files from the virtual data tree that got "
            "modified (i.e. by the construction kit)");
}

QStringList ModInfoOverwrite::archives() const
{
  QStringList result;
  QDir dir(this->absolutePath());
  for (const QString &archive : dir.entryList(QStringList({ "*.bsa", "*.ba2" }))) {
    result.append(this->absolutePath() + "/" + archive);
  }
  return result;
}

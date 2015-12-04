#include "modinfooverwrite.h"

#include "appconfig.h"

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
  return QDir::fromNativeSeparators(qApp->property("dataPath").toString() + "/" + QString::fromStdWString(AppConfig::overwritePath()));
}

std::vector<ModInfo::EFlag> ModInfoOverwrite::getFlags() const
{
  std::vector<ModInfo::EFlag> result;
  result.push_back(FLAG_OVERWRITE);
  return result;
}

int ModInfoOverwrite::getHighlight() const
{
  return (isValid() ? HIGHLIGHT_IMPORTANT : HIGHLIGHT_INVALID) | HIGHLIGHT_CENTER;
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
  for (const QString &archive : dir.entryList(QStringList("*.bsa"))) {
    result.append(this->absolutePath() + "/" + archive);
  }
  return result;
}

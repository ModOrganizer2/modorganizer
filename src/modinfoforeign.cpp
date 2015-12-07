#include "modinfoforeign.h"

#include "iplugingame.h"
#include "utility.h"

#include <QApplication>

using namespace MOBase;
using namespace MOShared;

QString ModInfoForeign::name() const
{
  return m_Name;
}

QDateTime ModInfoForeign::creationTime() const
{
  return m_CreationTime;
}

QString ModInfoForeign::absolutePath() const
{
  //I ought to store this, it's used elsewhere
  IPluginGame const *game = qApp->property("managed_game").value<IPluginGame const *>();
  return game->dataDirectory().absolutePath();
}

std::vector<ModInfo::EFlag> ModInfoForeign::getFlags() const
{
  std::vector<ModInfo::EFlag> result = ModInfoWithConflictInfo::getFlags();
  result.push_back(FLAG_FOREIGN);

  return result;
}

int ModInfoForeign::getHighlight() const
{
  return 0;
}

QString ModInfoForeign::getDescription() const
{
  return tr("This pseudo mod represents content managed outside MO. It isn't modified by MO.");
}

ModInfoForeign::ModInfoForeign(const QString &referenceFile, const QStringList &archives,
                               DirectoryEntry **directoryStructure)
  : ModInfoWithConflictInfo(directoryStructure)
  , m_ReferenceFile(referenceFile)
  , m_Archives(archives)
{
  m_CreationTime = QFileInfo(referenceFile).created();
  m_Name = "Unmanaged: " + QFileInfo(m_ReferenceFile).baseName();
}

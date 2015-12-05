#include "modinfoforeign.h"

#include "gameinfo.h"
#include "utility.h"

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
  return QDir::fromNativeSeparators(ToQString(GameInfo::instance().getGameDirectory())) + "/data";
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

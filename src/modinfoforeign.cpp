#include "modinfoforeign.h"

#include "iplugingame.h"
#include "utility.h"

#include <QApplication>

using namespace MOBase;
using namespace MOShared;

QDateTime ModInfoForeign::creationTime() const
{
  return m_CreationTime;
}

QString ModInfoForeign::absolutePath() const
{
  // I ought to store this, it's used elsewhere
  IPluginGame const* game = qApp->property("managed_game").value<IPluginGame*>();
  return game->dataDirectory().absolutePath();
}

std::vector<ModInfo::EFlag> ModInfoForeign::getFlags() const
{
  std::vector<ModInfo::EFlag> result = ModInfoWithConflictInfo::getFlags();
  result.push_back(FLAG_FOREIGN);

  if (m_PluginSelected) {
    result.push_back(ModInfo::FLAG_PLUGIN_SELECTED);
  }

  return result;
}

int ModInfoForeign::getHighlight() const
{
  return m_PluginSelected ? ModInfo::HIGHLIGHT_PLUGIN : ModInfo::HIGHLIGHT_NONE;
}

QString ModInfoForeign::getDescription() const
{
  return tr("This pseudo mod represents content managed outside MO. It isn't modified "
            "by MO.");
}

ModInfoForeign::ModInfoForeign(const QString& modName, const QString& referenceFile,
                               const QStringList& archives, ModInfo::EModType modType,
                               OrganizerCore& core)
    : ModInfoWithConflictInfo(core), m_ReferenceFile(referenceFile),
      m_Archives(archives), m_ModType(modType)
{
  m_CreationTime = QFileInfo(referenceFile).birthTime();
  switch (modType) {
  case ModInfo::EModType::MOD_DLC:
    m_Name         = tr("DLC: ") + modName;
    m_InternalName = QString("DLC: ") + modName;
    break;
  case ModInfo::EModType::MOD_CC:
    m_Name         = tr("Creation Club: ") + modName;
    m_InternalName = QString("Creation Club: ") + modName;
    break;
  default:
    m_Name         = tr("Unmanaged: ") + modName;
    m_InternalName = QString("Unmanaged: ") + modName;
  }
}

#ifndef MODINFOWITHCONFLICTINFO_H
#define MODINFOWITHCONFLICTINFO_H

#include "modinfo.h"

#include <QTime>

class ModInfoWithConflictInfo : public ModInfo
{

public:

  ModInfoWithConflictInfo(MOShared::DirectoryEntry **directoryStructure);

  std::vector<ModInfo::EFlag> getFlags() const;

  /**
   * @brief clear all caches held for this mod
   */
  virtual void clearCaches();

  virtual std::set<unsigned int> getModOverwrite() { return m_OverwriteList; }

  virtual std::set<unsigned int> getModOverwritten() { return m_OverwrittenList; }

  virtual void doConflictCheck() const;

private:

  enum EConflictType {
    CONFLICT_NONE,
    CONFLICT_OVERWRITE,
    CONFLICT_OVERWRITTEN,
    CONFLICT_MIXED,
    CONFLICT_REDUNDANT
  };

private:

  /**
   * @return true if there is a conflict for files in this mod
   */
  EConflictType isConflicted() const;

  /**
   * @return true if this mod is completely replaced by others
   */
  bool isRedundant() const;

private:

  MOShared::DirectoryEntry **m_DirectoryStructure;

  mutable EConflictType m_CurrentConflictState;
  mutable QTime m_LastConflictCheck;

  mutable std::set<unsigned int> m_OverwriteList;   // indices of mods overritten by this mod
  mutable std::set<unsigned int> m_OverwrittenList; // indices of mods overwriting this mod

};



#endif // MODINFOWITHCONFLICTINFO_H

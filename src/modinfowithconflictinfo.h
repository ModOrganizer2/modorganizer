#ifndef MODINFOWITHCONFLICTINFO_H
#define MODINFOWITHCONFLICTINFO_H

#include <ifiletree.h>

#include "modinfo.h"

#include <QTime>

class ModInfoWithConflictInfo : public ModInfo
{

public:

  std::vector<ModInfo::EConflictFlag> getConflictFlags() const override;
  virtual std::vector<ModInfo::EFlag> getFlags() const override;

  /**
   * @return true if this mod is considered "valid", that is: it contains data used by the game
   **/
  virtual bool isValid() const override;

  /**
   * @brief clear all caches held for this mod
   */
  virtual void clearCaches() override;

  virtual std::set<unsigned int> getModOverwrite() const override { return m_OverwriteList; }

  virtual std::set<unsigned int> getModOverwritten() const override { return m_OverwrittenList; }

  virtual std::set<unsigned int> getModArchiveOverwrite() const override { return m_ArchiveOverwriteList; }

  virtual std::set<unsigned int> getModArchiveOverwritten() const override { return m_ArchiveOverwrittenList; }

  virtual std::set<unsigned int> getModArchiveLooseOverwrite() const override { return m_ArchiveLooseOverwriteList; }

  virtual std::set<unsigned int> getModArchiveLooseOverwritten() const override { return m_ArchiveLooseOverwrittenList; }

  virtual void doConflictCheck() const override;

public slots:

  /**
   * @brief Notify this mod that the content of the disk may have changed.
   */
  virtual void diskContentModified();

protected:

  /**
   * @brief check if the content of this mod is valid.
   *
   * @return true if the content is valid, false otherwize.
   **/
  virtual bool doTestValid() const;

  /**
   * @brief Retrieve a file tree corresponding to the underlying disk content
   *     of this mod.
   *
   * The file tree should not be cached since it is already cached and updated when
   * required.
   *
   * @return a file tree representing the content of this mod.
   */
  std::shared_ptr<const MOBase::IFileTree> contentFileTree() const;

  ModInfoWithConflictInfo(
    PluginContainer* pluginContainer,
    const MOBase::IPluginGame* gamePlugin,
    MOShared::DirectoryEntry** directoryStructure);

private:

  enum EConflictType {
    CONFLICT_NONE,
    CONFLICT_OVERWRITE,
    CONFLICT_OVERWRITTEN,
    CONFLICT_MIXED,
    CONFLICT_REDUNDANT,
    CONFLICT_CROSS
  };

private:

  /**
   * @return true if there is a conflict for files in this mod
   */
  EConflictType isConflicted() const;

  /**
   * @return true if there are archive conflicts for files in this mod
   */
  EConflictType isArchiveConflicted() const;

  /**
   * @return true if there are archive conflicts with loose files in this mod
   */
  EConflictType isLooseArchiveConflicted() const;

  /**
   * @return true if this mod is completely replaced by others
   */
  bool isRedundant() const;

  bool hasHiddenFiles() const;

protected:

  // Current game plugin running in MO2:
  MOBase::IPluginGame const * const m_GamePlugin;

private:

  // Mutex:
  mutable std::mutex m_Mutex;

  // File tree representing the content of the disk. A null pointer indicates
  // that the content needs to be loaded from the disk:
  mutable std::shared_ptr<const MOBase::IFileTree> m_FileTree = nullptr;

  MOShared::DirectoryEntry **m_DirectoryStructure;

  mutable EConflictType m_CurrentConflictState;
  mutable EConflictType m_ArchiveConflictState;
  mutable EConflictType m_ArchiveConflictLooseState;
  mutable bool m_HasLooseOverwrite;
  mutable bool m_HasHiddenFiles;
  mutable QTime m_LastConflictCheck;

  mutable std::set<unsigned int> m_OverwriteList;   // indices of mods overritten by this mod
  mutable std::set<unsigned int> m_OverwrittenList; // indices of mods overwriting this mod
  mutable std::set<unsigned int> m_ArchiveOverwriteList;   // indices of mods with archive files overritten by this mod
  mutable std::set<unsigned int> m_ArchiveOverwrittenList; // indices of mods with archive files overwriting this mod
  mutable std::set<unsigned int> m_ArchiveLooseOverwriteList; // indices of mods with archives being overwritten by this mod's loose files
  mutable std::set<unsigned int> m_ArchiveLooseOverwrittenList; // indices of mods with loose files overwriting this mod's archive files

};



#endif // MODINFOWITHCONFLICTINFO_H

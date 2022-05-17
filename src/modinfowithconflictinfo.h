#ifndef MODINFOWITHCONFLICTINFO_H
#define MODINFOWITHCONFLICTINFO_H

#include <ifiletree.h>

#include "memoizedlock.h"
#include "modinfo.h"

#include <QTime>
#include <set>

class ModInfoWithConflictInfo : public ModInfo
{

public:
  std::vector<ModInfo::EConflictFlag> getConflictFlags() const override;
  virtual std::vector<ModInfo::EFlag> getFlags() const override;

  /**
   * @return true if this mod is considered "valid", that is: it contains data used by
   *the game
   **/
  virtual bool isValid() const override;

  /**
   * @return a list of content types contained in a mod
   */
  virtual const std::set<int>& getContents() const override;

  /**
   * @brief Test if the mod contains the specified content.
   *
   * @param content ID of the content to test.
   *
   * @return true if the content is there, false otherwise.
   */
  virtual bool hasContent(int content) const override;

  /**
   * @brief Retrieve a file tree corresponding to the underlying disk content
   *     of this mod.
   *
   * The file tree should not be cached since it is already cached and updated when
   * required.
   *
   * @return a file tree representing the content of this mod.
   */
  std::shared_ptr<const MOBase::IFileTree> fileTree() const override;

public:
  /**
   * @brief clear all caches held for this mod
   */
  void clearCaches() override;

  const std::set<unsigned int>& getModOverwrite() const override
  {
    return m_Conflicts.value().m_OverwriteList;
  }
  const std::set<unsigned int>& getModOverwritten() const override
  {
    return m_Conflicts.value().m_OverwrittenList;
  }
  const std::set<unsigned int>& getModArchiveOverwrite() const override
  {
    return m_Conflicts.value().m_ArchiveOverwriteList;
  }
  const std::set<unsigned int>& getModArchiveOverwritten() const override
  {
    return m_Conflicts.value().m_ArchiveOverwrittenList;
  }
  const std::set<unsigned int>& getModArchiveLooseOverwrite() const override
  {
    return m_Conflicts.value().m_ArchiveLooseOverwriteList;
  }
  const std::set<unsigned int>& getModArchiveLooseOverwritten() const override
  {
    return m_Conflicts.value().m_ArchiveLooseOverwrittenList;
  }

public slots:

  /**
   * @brief Notify this mod that the content of the disk may have changed.
   */
  virtual void diskContentModified();

protected:
  // check if the content of this mod is valid
  //
  virtual bool doIsValid() const;

  /**
   * @brief Compute the contents for this mod.
   *
   * @return the contents for this mod.
   **/
  virtual std::set<int> doGetContents() const { return {}; }

  ModInfoWithConflictInfo(OrganizerCore& core);

private:
  enum EConflictType
  {
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
  /**
   * @brief Prefetch content for this mod.
   *
   * This method can be used to prefetch content from the mod, e.g., for isValid()
   * or getContents(). This method will only be called when first creating the mod
   * using multiple threads for all the mods.
   */
  virtual void prefetch() override;

private:
  struct Conflicts
  {
    EConflictType m_CurrentConflictState      = CONFLICT_NONE;
    EConflictType m_ArchiveConflictState      = CONFLICT_NONE;
    EConflictType m_ArchiveConflictLooseState = CONFLICT_NONE;
    bool m_HasLooseOverwrite                  = false;
    bool m_HasHiddenFiles                     = false;

    std::set<unsigned int> m_OverwriteList;    // indices of mods overritten by this mod
    std::set<unsigned int> m_OverwrittenList;  // indices of mods overwriting this mod
    std::set<unsigned int> m_ArchiveOverwriteList;    // indices of mods with archive
                                                      // files overritten by this mod
    std::set<unsigned int> m_ArchiveOverwrittenList;  // indices of mods with archive
                                                      // files overwriting this mod
    std::set<unsigned int>
        m_ArchiveLooseOverwriteList;  // indices of mods with archives being overwritten
                                      // by this mod's loose files
    std::set<unsigned int>
        m_ArchiveLooseOverwrittenList;  // indices of mods with loose files overwriting
                                        // this mod's archive files
  };

  Conflicts doConflictCheck() const;

  MOBase::MemoizedLocked<std::shared_ptr<const MOBase::IFileTree>> m_FileTree;
  MOBase::MemoizedLocked<bool> m_Valid;
  MOBase::MemoizedLocked<std::set<int>> m_Contents;
  MOBase::MemoizedLocked<Conflicts> m_Conflicts;
};

#endif  // MODINFOWITHCONFLICTINFO_H

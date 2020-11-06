#include "modinfowithconflictinfo.h"
#include "utility.h"
#include "shared/directoryentry.h"
#include "shared/filesorigin.h"
#include "shared/fileentry.h"
#include <filesystem>

#include "iplugingame.h"
#include "moddatachecker.h"
#include "qdirfiletree.h"

using namespace MOBase;
using namespace MOShared;
namespace fs = std::filesystem;

ModInfoWithConflictInfo::ModInfoWithConflictInfo(
  PluginContainer *pluginContainer, const MOBase::IPluginGame* gamePlugin, DirectoryEntry **directoryStructure)
  : ModInfo(pluginContainer), m_GamePlugin(gamePlugin), 
  m_FileTree([this]() { return QDirFileTree::makeTree(absolutePath()); }),
  m_Valid([this]() { return doIsValid(); }),
  m_Contents([this]() { return doGetContents(); }),
  m_DirectoryStructure(directoryStructure), m_HasLooseOverwrite(false), m_HasHiddenFiles(false) {}

void ModInfoWithConflictInfo::clearCaches()
{
  m_LastConflictCheck = QTime();
}

std::vector<ModInfo::EFlag> ModInfoWithConflictInfo::getFlags() const
{ 
  std::vector<ModInfo::EFlag> result = std::vector<ModInfo::EFlag>();
  if (hasHiddenFiles()) {
    result.push_back(ModInfo::FLAG_HIDDEN_FILES);
  }
  return result;
}

std::vector<ModInfo::EConflictFlag> ModInfoWithConflictInfo::getConflictFlags() const
{
  std::vector<ModInfo::EConflictFlag> result;
  switch (isConflicted()) {
    case CONFLICT_MIXED: {
      result.push_back(ModInfo::FLAG_CONFLICT_MIXED);
    } break;
    case CONFLICT_OVERWRITE: {
      result.push_back(ModInfo::FLAG_CONFLICT_OVERWRITE);
    } break;
    case CONFLICT_OVERWRITTEN: {
      result.push_back(ModInfo::FLAG_CONFLICT_OVERWRITTEN);
    } break;
    case CONFLICT_REDUNDANT: {
      result.push_back(ModInfo::FLAG_CONFLICT_REDUNDANT);
    } break;
    default: { /* NOP */ }
  }
  switch (isLooseArchiveConflicted()) {
    case CONFLICT_MIXED: {
      result.push_back(ModInfo::FLAG_ARCHIVE_LOOSE_CONFLICT_OVERWRITE);
      result.push_back(ModInfo::FLAG_ARCHIVE_LOOSE_CONFLICT_OVERWRITTEN);
    } break;
    case CONFLICT_OVERWRITE: {
      result.push_back(ModInfo::FLAG_ARCHIVE_LOOSE_CONFLICT_OVERWRITE);
    } break;
    case CONFLICT_OVERWRITTEN: {
      result.push_back(ModInfo::FLAG_ARCHIVE_LOOSE_CONFLICT_OVERWRITTEN);
    } break;
    default: { /* NOP */ }
  }
  switch (isArchiveConflicted()) {
    case CONFLICT_MIXED: {
      result.push_back(ModInfo::FLAG_ARCHIVE_CONFLICT_MIXED);
    } break;
    case CONFLICT_OVERWRITE: {
      result.push_back(ModInfo::FLAG_ARCHIVE_CONFLICT_OVERWRITE);
    } break;
    case CONFLICT_OVERWRITTEN: {
      result.push_back(ModInfo::FLAG_ARCHIVE_CONFLICT_OVERWRITTEN);
    } break;
    default: { /* NOP */ }
  }
  return result;
}


void ModInfoWithConflictInfo::doConflictCheck() const
{
  m_OverwriteList.clear();
  m_OverwrittenList.clear();
  m_ArchiveOverwriteList.clear();
  m_ArchiveOverwrittenList.clear();
  m_ArchiveLooseOverwriteList.clear();
  m_ArchiveLooseOverwrittenList.clear();

  bool providesAnything = false;
  bool hasHiddenFiles = false;

  int dataID = 0;
  if ((*m_DirectoryStructure)->originExists(L"data")) {
    dataID = (*m_DirectoryStructure)->getOriginByName(L"data").getID();
  }

  std::wstring name = ToWString(this->name());
  const std::wstring hideExt = ToWString(ModInfo::s_HiddenExt);

  m_CurrentConflictState = CONFLICT_NONE;
  m_ArchiveConflictState = CONFLICT_NONE;
  m_ArchiveConflictLooseState = CONFLICT_NONE;

  if ((*m_DirectoryStructure)->originExists(name)) {
    FilesOrigin &origin = (*m_DirectoryStructure)->getOriginByName(name);
    std::vector<FileEntryPtr> files = origin.getFiles();
    std::set<const DirectoryEntry*> checkedDirs;

    // for all files in this origin
    for (FileEntryPtr file : files) {

      // skip hiidden file check if already found one
      if (!hasHiddenFiles) {
        const fs::path nameAsPath(file->getName());

        if (nameAsPath.extension().wstring().compare(hideExt) == 0) {
          hasHiddenFiles = true;
        }
        else {
          const DirectoryEntry* parent = file->getParent();

          // iterate on all parent direEntries to check for .mohiddden
          while (parent != nullptr) {
            auto insertResult = checkedDirs.insert(parent);

            if (insertResult.second == false) {
              // if already present break as we can assume to have checked the parents as well
              break;
            }
            else {
              const fs::path dirPath(parent->getName());
              if (dirPath.extension().wstring().compare(hideExt) == 0) {
                hasHiddenFiles = true;
                break;
              }
              parent = parent->getParent();
            }
          }
        }
      }

      auto alternatives = file->getAlternatives();
      if ((alternatives.size() == 0) || (alternatives.back().originID() == dataID)) {
        // no alternatives -> no conflict
        providesAnything = true;
      } else {
        // Get the archive data for the current mod
        DataArchiveOrigin archiveData;
        if (file->getOrigin() == origin.getID())
          archiveData = file->getArchive();
        else {
          for (const auto& alt : alternatives) {
            if (alt.originID() == origin.getID()) {
              archiveData = alt.archive();
              break;
            }
          }
        }

        // If this is not the origin then determine the correct overwrite
        if (file->getOrigin() != origin.getID()) {
          FilesOrigin &altOrigin = (*m_DirectoryStructure)->getOriginByID(file->getOrigin());
          unsigned int altIndex = ModInfo::getIndex(ToQString(altOrigin.getName()));
          if (!file->isFromArchive()) {
            if (!archiveData.isValid())
              m_OverwrittenList.insert(altIndex);
            else
              m_ArchiveLooseOverwrittenList.insert(altIndex);
          }
          else {
            m_ArchiveOverwrittenList.insert(altIndex);
          }
        } else {
          providesAnything = true;
        }

        // Sort out the alternatives
        for (const auto& altInfo : alternatives) {
          if ((altInfo.originID() != dataID) && (altInfo.originID() != origin.getID())) {
            FilesOrigin &altOrigin = (*m_DirectoryStructure)->getOriginByID(altInfo.originID());
            QString altOriginName = ToQString(altOrigin.getName());
            unsigned int altIndex = ModInfo::getIndex(altOriginName);
            if (!altInfo.isFromArchive()) {
              if (!archiveData.isValid()) {
                if (origin.getPriority() > altOrigin.getPriority()) {
                  m_OverwriteList.insert(altIndex);
                } else {
                  m_OverwrittenList.insert(altIndex);
                }
              } else {
                m_ArchiveLooseOverwrittenList.insert(altIndex);
              }
            } else {
              if (!archiveData.isValid()) {
                m_ArchiveLooseOverwriteList.insert(altIndex);
              } else {
                if (archiveData.order() > altInfo.archive().order()) {
                  m_ArchiveOverwriteList.insert(altIndex);
                } else if (archiveData.order() < altInfo.archive().order()) {
                  m_ArchiveOverwrittenList.insert(altIndex);
                }
              }
            }
          }
        }
      }
    }
    m_LastConflictCheck = QTime::currentTime();

    if (files.size() != 0) {
      if (!providesAnything)
        m_CurrentConflictState = CONFLICT_REDUNDANT;
      else if (!m_OverwriteList.empty() && !m_OverwrittenList.empty())
        m_CurrentConflictState = CONFLICT_MIXED;
      else if (!m_OverwriteList.empty())
        m_CurrentConflictState = CONFLICT_OVERWRITE;
      else if (!m_OverwrittenList.empty())
        m_CurrentConflictState = CONFLICT_OVERWRITTEN;

      if (!m_ArchiveOverwriteList.empty() && !m_ArchiveOverwrittenList.empty())
        m_ArchiveConflictState = CONFLICT_MIXED;
      else if (!m_ArchiveOverwriteList.empty())
        m_ArchiveConflictState = CONFLICT_OVERWRITE;
      else if (!m_ArchiveOverwrittenList.empty())
        m_ArchiveConflictState = CONFLICT_OVERWRITTEN;

      if (!m_ArchiveLooseOverwrittenList.empty() && !m_ArchiveLooseOverwriteList.empty())
        m_ArchiveConflictLooseState = CONFLICT_MIXED;
      else if (!m_ArchiveLooseOverwrittenList.empty())
        m_ArchiveConflictLooseState = CONFLICT_OVERWRITTEN;
      else if (!m_ArchiveLooseOverwriteList.empty())
        m_ArchiveConflictLooseState = CONFLICT_OVERWRITE;

      m_HasHiddenFiles = hasHiddenFiles;
    }
  }
}

ModInfoWithConflictInfo::EConflictType ModInfoWithConflictInfo::isConflicted() const
{
  // this is costy so cache the result
  QTime now = QTime::currentTime();
  if (m_LastConflictCheck.isNull() || (m_LastConflictCheck.secsTo(now) > 10)) {
    doConflictCheck();
  }

  return m_CurrentConflictState;
}

ModInfoWithConflictInfo::EConflictType ModInfoWithConflictInfo::isArchiveConflicted() const
{
  QTime now = QTime::currentTime();
  if (m_LastConflictCheck.isNull() || (m_LastConflictCheck.secsTo(now) > 10)) {
    doConflictCheck();
  }

  return m_ArchiveConflictState;
}

ModInfoWithConflictInfo::EConflictType ModInfoWithConflictInfo::isLooseArchiveConflicted() const
{
  QTime now = QTime::currentTime();
  if (m_LastConflictCheck.isNull() || (m_LastConflictCheck.secsTo(now) > 10)) {
    doConflictCheck();
  }

  return m_ArchiveConflictLooseState;
}


bool ModInfoWithConflictInfo::isRedundant() const
{
  std::wstring name = ToWString(this->name());
  if ((*m_DirectoryStructure)->originExists(name)) {
    FilesOrigin &origin = (*m_DirectoryStructure)->getOriginByName(name);
    std::vector<FileEntryPtr> files = origin.getFiles();
    bool ignore = false;
    for (auto iter = files.begin(); iter != files.end(); ++iter) {
      if ((*iter)->getOrigin(ignore) == origin.getID()) {
        return false;
      }
    }
    return true;
  } else {
    return false;
  }
}


bool ModInfoWithConflictInfo::hasHiddenFiles() const
{
  QTime now = QTime::currentTime();
  if (m_LastConflictCheck.isNull() || (m_LastConflictCheck.secsTo(now) > 10)) {
    doConflictCheck();
  }

  return m_HasHiddenFiles;
}

void ModInfoWithConflictInfo::diskContentModified() {
  m_FileTree.invalidate();
  m_Valid.invalidate();
  m_Contents.invalidate();
}

void ModInfoWithConflictInfo::prefetch() {
  // Populating the tree to 1-depth (IFileTree is lazy, so size() forces the
  // tree to populate the first level):
  fileTree()->size();
}

bool ModInfoWithConflictInfo::doIsValid() const {
  auto mdc = m_GamePlugin->feature<ModDataChecker>();

  if (mdc) {
    auto qdirfiletree = fileTree();
    return mdc->dataLooksValid(qdirfiletree) == ModDataChecker::CheckReturn::VALID;
  }

  return true;
}

std::shared_ptr<const IFileTree> ModInfoWithConflictInfo::fileTree() const {
  return m_FileTree.value();
}

bool ModInfoWithConflictInfo::isValid() const {
  return m_Valid.value();
}

const std::set<int>& ModInfoWithConflictInfo::getContents() const {
  return m_Contents.value();
}

bool ModInfoWithConflictInfo::hasContent(int content) const {
  auto& contents = m_Contents.value();
  return std::find(std::begin(contents), std::end(contents), content) != std::end(contents);
}

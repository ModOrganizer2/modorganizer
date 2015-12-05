#include "modinfowithconflictinfo.h"

#include "directoryentry.h"
#include "utility.h"

using namespace MOBase;
using namespace MOShared;

ModInfoWithConflictInfo::ModInfoWithConflictInfo(DirectoryEntry **directoryStructure)
  : m_DirectoryStructure(directoryStructure) {}

void ModInfoWithConflictInfo::clearCaches()
{
  m_LastConflictCheck = QTime();
}

std::vector<ModInfo::EFlag> ModInfoWithConflictInfo::getFlags() const
{
  std::vector<ModInfo::EFlag> result;
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
  return result;
}


void ModInfoWithConflictInfo::doConflictCheck() const
{
  m_OverwriteList.clear();
  m_OverwrittenList.clear();

  bool providesAnything = false;

  int dataID = 0;
  if ((*m_DirectoryStructure)->originExists(L"data")) {
    dataID = (*m_DirectoryStructure)->getOriginByName(L"data").getID();
  }

  std::wstring name = ToWString(this->name());

  m_CurrentConflictState = CONFLICT_NONE;

  if ((*m_DirectoryStructure)->originExists(name)) {
    FilesOrigin &origin = (*m_DirectoryStructure)->getOriginByName(name);
    std::vector<FileEntry::Ptr> files = origin.getFiles();
    // for all files in this origin
    for (FileEntry::Ptr file : files) {
      const std::vector<int> &alternatives = file->getAlternatives();
      if ((alternatives.size() == 0) || (alternatives[0] == dataID)) {
        // no alternatives -> no conflict
        providesAnything = true;
      } else {
        if (file->getOrigin() != origin.getID()) {
          FilesOrigin &altOrigin = (*m_DirectoryStructure)->getOriginByID(file->getOrigin());
          unsigned int altIndex = ModInfo::getIndex(ToQString(altOrigin.getName()));
          m_OverwrittenList.insert(altIndex);
        } else {
          providesAnything = true;
        }

        // for all non-providing alternative origins
        for (int altId : alternatives) {
          if ((altId != dataID) && (altId != origin.getID())) {
            FilesOrigin &altOrigin = (*m_DirectoryStructure)->getOriginByID(altId);
            unsigned int altIndex = ModInfo::getIndex(ToQString(altOrigin.getName()));
            if (origin.getPriority() > altOrigin.getPriority()) {
              m_OverwriteList.insert(altIndex);
            } else {
              m_OverwrittenList.insert(altIndex);
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


bool ModInfoWithConflictInfo::isRedundant() const
{
  std::wstring name = ToWString(this->name());
  if ((*m_DirectoryStructure)->originExists(name)) {
    FilesOrigin &origin = (*m_DirectoryStructure)->getOriginByName(name);
    std::vector<FileEntry::Ptr> files = origin.getFiles();
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

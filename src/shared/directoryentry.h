/*
Copyright (C) 2012 Sebastian Herbord. All rights reserved.

This file is part of Mod Organizer.

Mod Organizer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Mod Organizer is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Mod Organizer.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef DIRECTORYENTRY_H
#define DIRECTORYENTRY_H


#include <string>
#include <set>
#include <vector>
#include <map>
#include <cassert>
#define WIN32_MEAN_AND_LEAN
#include <Windows.h>
#include <bsatk.h>
#ifndef Q_MOC_RUN
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#endif
#include "util.h"


namespace MOShared {


class DirectoryEntry;
class OriginConnection;
class FileRegister;


class FileEntry {

public:

  typedef unsigned int Index;

  typedef boost::shared_ptr<FileEntry> Ptr;

public:

  FileEntry();

  FileEntry(Index index, const std::wstring &name, DirectoryEntry *parent);

  ~FileEntry();

  Index getIndex() const { return m_Index; }

  time_t lastAccessed() const { return m_LastAccessed; }

  void addOrigin(int origin, FILETIME fileTime, const std::wstring &archive);
  // remove the specified origin from the list of origins that contain this file. if no origin is left,
  // the file is effectively deleted and true is returned. otherwise, false is returned
  bool removeOrigin(int origin);
  void sortOrigins();

  // gets the list of alternative origins (origins with lower priority than the primary one).
  // if sortOrigins has been called, it is sorted by priority (ascending)
  const std::vector<std::pair<int, std::wstring>> &getAlternatives() const { return m_Alternatives; }

  const std::wstring &getName() const { return m_Name; }
  int getOrigin() const { return m_Origin; }
  int getOrigin(bool &archive) const { archive = (m_Archive.length() != 0); return m_Origin; }
  const std::wstring &getArchive() const { return m_Archive; }
  bool isFromArchive() const { return m_Archive.length() != 0; }
  std::wstring getFullPath() const;
  std::wstring getRelativePath() const;
  DirectoryEntry *getParent() { return m_Parent; }

  void setFileTime(FILETIME fileTime) const { m_FileTime = fileTime; }
  FILETIME getFileTime() const { return m_FileTime; }

private:

  bool recurseParents(std::wstring &path, const DirectoryEntry *parent) const;

  void determineTime();

private:

  Index m_Index;
  std::wstring m_Name;
  int m_Origin = -1;
  std::wstring m_Archive;
  std::vector<std::pair<int, std::wstring>> m_Alternatives;
  DirectoryEntry *m_Parent;
  mutable FILETIME m_FileTime;

  time_t m_LastAccessed;

  friend bool operator<(const FileEntry &lhs, const FileEntry &rhs) {
    return _wcsicmp(lhs.m_Name.c_str(), rhs.m_Name.c_str()) < 0;
  }
  friend bool operator==(const FileEntry &lhs, const FileEntry &rhs) {
    return _wcsicmp(lhs.m_Name.c_str(), rhs.m_Name.c_str()) == 0;
  }
};


// represents a mod or the data directory, providing files to the tree
class FilesOrigin {
  friend class OriginConnection;
public:

  FilesOrigin();
  FilesOrigin(const FilesOrigin &reference);
  ~FilesOrigin();

  // sets priority for this origin, but it will overwrite the exisiting mapping for this priority,
  // the previous origin will no longer be referenced
  void setPriority(int priority);

  int getPriority() const { return m_Priority; }

  void setName(const std::wstring &name);
  const std::wstring &getName() const { return m_Name; }

  int getID() const { return m_ID; }
  const std::wstring &getPath() const { return m_Path; }

  std::vector<FileEntry::Ptr> getFiles() const;

  void enable(bool enabled, time_t notAfter = LONG_MAX);
  bool isDisabled() const { return m_Disabled; }

  void addFile(FileEntry::Index index) { m_Files.insert(index); }
  void removeFile(FileEntry::Index index);

private:

  FilesOrigin(int ID, const std::wstring &name, const std::wstring &path, int priority,
              boost::shared_ptr<FileRegister> fileRegister, boost::shared_ptr<OriginConnection> originConnection);


private:

  int m_ID;

  bool m_Disabled;

  std::set<FileEntry::Index> m_Files;
  std::wstring m_Name;
  std::wstring m_Path;
  int m_Priority;
  boost::weak_ptr<FileRegister> m_FileRegister;
  boost::weak_ptr<OriginConnection> m_OriginConnection;

};


class FileRegister
{

public:

  FileRegister(boost::shared_ptr<OriginConnection> originConnection);
  ~FileRegister();

  bool indexValid(FileEntry::Index index) const;

  FileEntry::Ptr createFile(const std::wstring &name, DirectoryEntry *parent);
  FileEntry::Ptr getFile(FileEntry::Index index) const;

  size_t size() const { return m_Files.size(); }

  bool removeFile(FileEntry::Index index);
  void removeOrigin(FileEntry::Index index, int originID);
  void removeOriginMulti(std::set<FileEntry::Index> indices, int originID, time_t notAfter);

  void sortOrigins();

private:

  FileEntry::Index generateIndex();

  void unregisterFile(FileEntry::Ptr file);

private:

  std::map<FileEntry::Index, FileEntry::Ptr> m_Files;

  boost::shared_ptr<OriginConnection> m_OriginConnection;

};


class DirectoryEntry
{
public:

  DirectoryEntry(const std::wstring &name, DirectoryEntry *parent, int originID);

  DirectoryEntry(const std::wstring &name, DirectoryEntry *parent, int originID,
                 boost::shared_ptr<FileRegister> fileRegister,
                 boost::shared_ptr<OriginConnection> originConnection);

  ~DirectoryEntry();

  void clear();
  bool isPopulated() const { return m_Populated; }

  bool isEmpty() const { return m_Files.empty() && m_SubDirectories.empty(); }

  const DirectoryEntry *getParent() const { return m_Parent; }

  // add files to this directory (and subdirectories) from the specified origin. That origin may exist or not
  void addFromOrigin(const std::wstring &originName, const std::wstring &directory, int priority);
  void addFromBSA(const std::wstring &originName, std::wstring &directory, const std::wstring &fileName, int priority);

  void propagateOrigin(int origin);

  const std::wstring &getName() const;

  boost::shared_ptr<FileRegister> getFileRegister() { return m_FileRegister; }

  bool originExists(const std::wstring &name) const;
  FilesOrigin &getOriginByID(int ID) const;
  FilesOrigin &getOriginByName(const std::wstring &name) const;

  int anyOrigin() const;

  //int getOrigin(const std::wstring &path, bool &archive);

  std::vector<FileEntry::Ptr> getFiles() const;

  void getSubDirectories(std::vector<DirectoryEntry*>::const_iterator &begin
                         , std::vector<DirectoryEntry*>::const_iterator &end) const {
    begin = m_SubDirectories.begin(); end = m_SubDirectories.end();
  }

  DirectoryEntry *findSubDirectory(const std::wstring &name) const;
  DirectoryEntry *findSubDirectoryRecursive(const std::wstring &path);

  /** retrieve a file in this directory by name.
    * @param name name of the file
    * @return fileentry object for the file or nullptr if no file matches
    */
  const FileEntry::Ptr findFile(const std::wstring &name) const;

  /** search through this directory and all subdirectories for a file by the specified name (relative path).
      if directory is not nullptr, the referenced variable will be set to the path containing the file */
  const FileEntry::Ptr searchFile(const std::wstring &path, const DirectoryEntry **directory) const;

  void insertFile(const std::wstring &filePath, FilesOrigin &origin, FILETIME fileTime);

  void removeFile(FileEntry::Index index);

  // remove the specified file from the tree. This can be a path leading to a file in a subdirectory
  bool removeFile(const std::wstring &filePath, int *origin = nullptr);

  /**
   * @brief remove the specified directory
   * @param path directory to remove
   */
  void removeDir(const std::wstring &path);

  bool remove(const std::wstring &fileName, int *origin) {
    auto iter = m_Files.find(ToLower(fileName));
    if (iter != m_Files.end()) {
      if (origin != nullptr) {
        FileEntry::Ptr entry = m_FileRegister->getFile(iter->second);
        if (entry.get() != nullptr) {
          bool ignore;
          *origin = entry->getOrigin(ignore);
        }
      }
      return m_FileRegister->removeFile(iter->second);
    } else {
      return false;
    }
  }

  bool hasContentsFromOrigin(int originID) const;

  FilesOrigin &createOrigin(const std::wstring &originName, const std::wstring &directory, int priority);

  void removeFiles(const std::set<FileEntry::Index> &indices);

private:

  DirectoryEntry(const DirectoryEntry &reference);
  DirectoryEntry &operator=(const DirectoryEntry &reference);

  void insert(const std::wstring &fileName, FilesOrigin &origin, FILETIME fileTime, const std::wstring &archive) {
    std::wstring fileNameLower = ToLower(fileName);
    auto iter = m_Files.find(fileNameLower);
    FileEntry::Ptr file;
    if (iter != m_Files.end()) {
      file = m_FileRegister->getFile(iter->second);
    } else {
      file = m_FileRegister->createFile(fileName, this);
      // TODO this has been observed to cause a crash, no clue why
      m_Files[fileNameLower] = file->getIndex();
    }
    file->addOrigin(origin.getID(), fileTime, archive);
    origin.addFile(file->getIndex());
  }

  void addFiles(FilesOrigin &origin, wchar_t *buffer, int bufferOffset);
  void addFiles(FilesOrigin &origin, BSA::Folder::Ptr archiveFolder, FILETIME &fileTime, const std::wstring &archiveName);

  DirectoryEntry *getSubDirectory(const std::wstring &name, bool create, int originID = -1);

  DirectoryEntry *getSubDirectoryRecursive(const std::wstring &path, bool create, int originID = -1);

  void removeDirRecursive();

private:

  boost::shared_ptr<FileRegister> m_FileRegister;
  boost::shared_ptr<OriginConnection> m_OriginConnection;

  std::wstring m_Name;
  std::map<std::wstring, FileEntry::Index> m_Files;
  std::vector<DirectoryEntry*> m_SubDirectories;

  DirectoryEntry *m_Parent;
  std::set<int> m_Origins;

  bool m_Populated;

  bool m_TopLevel;

};


} // namespace MOShared

#endif // DIRECTORYENTRY_H

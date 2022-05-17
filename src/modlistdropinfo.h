#ifndef MODLISTDROPINFO_H
#define MODLISTDROPINFO_H

#include <optional>
#include <vector>

#include <QMimeData>
#include <QString>
#include <QUrl>

class OrganizerCore;

// small class that extract information from mimeData
//
class ModListDropInfo
{
public:
  // text value for the mime-data for the various possible
  // origin (not for external drops)
  //
  static constexpr const char* ModText      = "mod";
  static constexpr const char* DownloadText = "download";

public:
  struct RelativeUrl
  {
    const QUrl url;
    const QString relativePath;
    const QString originName;
  };

public:
  ModListDropInfo(const QMimeData* mimeData, OrganizerCore& core);

  // returns true if this drop is valid
  //
  bool isValid() const;

  // returns true if these data corresponds to drag&drop
  // of local files (e.g. from overwrite)
  //
  bool isLocalFileDrop() const;

  // returns true if these data corresponds to drag&drop
  // of mod in the list
  //
  bool isModDrop() const;

  // returns true if these data corresponds to drag&drop
  // from the download list
  //
  bool isDownloadDrop() const;

  // returns true if these data corresponds to dropping
  // an archive for installation
  //
  bool isExternalArchiveDrop() const;

  // returns true if these data corresponds to dropping
  // a folder for copy
  //
  bool isExternalFolderDrop() const;

  const auto& rows() const { return m_rows; }
  const auto& download() const { return m_download; }
  const auto& localUrls() const { return m_localUrls; }
  const auto& externalUrl() const { return m_url; }

private:
  friend class ModList;

  // retrieve the relative path of file and its origin given a URL from Mime data
  // returns an empty optional if the URL is not a valid file for dropping
  //
  std::optional<RelativeUrl> relativeUrl(const QUrl&) const;

private:
  // rows for drag&drop between views
  std::vector<int> m_rows;
  int m_download;  // -1 if invalid

  // local URLs from the data (relative path + origin name)
  std::vector<RelativeUrl> m_localUrls;

  // external URL
  QUrl m_url;
};

#endif

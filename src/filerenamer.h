#ifndef FILERENAMER_H
#define FILERENAMER_H

#include <QWidget>

namespace MOBase::shell
{
class Result;
}

/**
 * Renames individual files and handles dialog boxes to confirm replacements and
 * failures with the user
 **/
class FileRenamer
{
public:
  /**
   * controls appearance and replacement behaviour; if RENAME_REPLACE_ALL and
   * RENAME_REPLACE_NONE are not provided, the user will have the option to
   * choose on the first replacement
   **/
  enum RenameFlags
  {
    /**
     * this renamer will be used on multiple files, so display additional
     * buttons to replace all and for canceling
     **/
    MULTIPLE = 0x01,

    /**
     * customizes some of the text shown on dialog to mention that files are
     * being hidden
     **/
    HIDE = 0x02,

    /**
     * customizes some of the text shown on dialog to mention that files are
     * being unhidden
     **/
    UNHIDE = 0x04,

    /**
     * silently replaces all existing files
     **/
    REPLACE_ALL = 0x08,

    /**
     * silently skips all existing files
     **/
    REPLACE_NONE = 0x10,
  };

  /** result of a single rename
   *
   **/
  enum RenameResults
  {
    /**
     * the user skipped this file
     */
    RESULT_SKIP,

    /**
     * the file was successfully renamed
     */
    RESULT_OK,

    /**
     * the user wants to cancel
     */
    RESULT_CANCEL
  };

  /**
   * @param parent Parent widget for dialog boxes
   **/
  FileRenamer(QWidget* parent, QFlags<RenameFlags> flags);

  /**
   * renames the given file
   * @param oldName current filename
   * @param newName new filename
   * @return whether the file was renamed, skipped or the user wants to cancel
   **/
  RenameResults rename(const QString& oldName, const QString& newName);

private:
  /**
   *user's decision when replacing
   **/
  enum RenameDecision
  {
    /**
     * replace the file
     **/
    DECISION_REPLACE,

    /**
     * skip the file
     **/
    DECISION_SKIP,

    /**
     * cancel the whole thing
     **/
    DECISION_CANCEL
  };

  /**
   * parent widget for dialog boxes
   **/
  QWidget* m_parent;

  /**
   * flags
   **/
  QFlags<RenameFlags> m_flags;

  /**
   * asks the user to replace an existing file, may return early if the user
   * has already selected to replace all/none
   * @return whether to replace, skip or cancel
   **/
  RenameDecision confirmReplace(const QString& newName);

  /**
   * removal of a file failed, ask the user to continue or cancel
   * @param name The name of the file that failed to be removed
   * @return true to continue, false to stop
   **/
  bool removeFailed(const QString& name, const MOBase::shell::Result& r);

  /**
   * renaming a file failed, ask the user to continue or cancel
   * @param oldName current filename
   * @param newName new filename
   * @return true to continue, false to stop
   **/
  bool renameFailed(const QString& oldName, const QString& newName,
                    const MOBase::shell::Result& r);
};

#endif  // FILERENAMER_H

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

#ifndef PLUGINLIST_H
#define PLUGINLIST_H

#include <directoryentry.h>
#include <ipluginlist.h>
#include <QString>
#include <QListWidget>
#include <QTimer>
#include <QTemporaryFile>
#include <boost/signals2.hpp>
#include <boost/ptr_container/ptr_vector.hpp>
#include <vector>
#include <pdll.h>
#include <BOSS-API.h>



template <class C>
class ChangeBracket {
public:
  ChangeBracket(C *model)
    : m_Model(nullptr)
  {
    QVariant var = model->property("__aboutToChange");
    bool aboutToChange = var.isValid() && var.toBool();
    if (!aboutToChange) {
      model->layoutAboutToBeChanged();
      model->setProperty("__aboutToChange", true);
      m_Model = model;
    }
  }
  ~ChangeBracket() {
    finish();
  }

  void finish() {
    if (m_Model != nullptr) {
      m_Model->layoutChanged();
      m_Model->setProperty("__aboutToChange", false);
      m_Model = nullptr;
    }
  }

private:
  C *m_Model;
};



/**
 * @brief model representing the plugins (.esp/.esm) in the current virtual data folder
 **/
class PluginList : public QAbstractTableModel, public MOBase::IPluginList
{
  Q_OBJECT
  friend class ChangeBracket<PluginList>;
public:

  enum EColumn {
    COL_NAME,
    COL_FLAGS,
    COL_PRIORITY,
    COL_MODINDEX,

    COL_LASTCOLUMN = COL_MODINDEX
  };

  typedef boost::signals2::signal<void ()> SignalRefreshed;

public:

  /**
   * @brief constructor
   *
   * @param parent parent object
   **/
  PluginList(QObject *parent = NULL);

  ~PluginList();

  /**
   * @brief does a complete refresh of the list
   *
   * @param profileName name of the current profile
   * @param baseDirectory the root directory structure representing the virtual data directory
   * @todo the profile is not used? If it was, we should pass the Profile-object instead
   **/
  void refresh(const QString &profileName, const MOShared::DirectoryEntry &baseDirectory, const QString &pluginsFile, const QString &loadOrderFile, const QString &lockedOrderFile);

  /**
   * @brief enable a plugin based on its name
   *
   * @param name name of the plugin to enable
   **/
  void enableESP(const QString &name);

  /**
   * @brief test if a plugin is enabled
   *
   * @param name name of the plugin to look up
   * @return true if the plugin is enabled, false otherwise
   **/
  bool isEnabled(const QString &name);

  /**
   * @brief test if a plugin is enabled
   *
   * @param index index of the plugin to look up
   * @return true if the plugin is enabled, false otherwise
   * @throws std::out_of_range exception is thrown if index is invalid
   **/
  bool isEnabled(int index);

  /**
   * @brief update the plugin status (enabled/disabled) from the specified file
   *
   * @param fileName path of the file to load. the filename should be "plugin.txt"
   * @todo it would make sense to move this into the Profile-class
   **/
  void readEnabledFrom(const QString &fileName);

  /**
   * @brief save the plugin status to the specified file
   *
   * @param pluginFileName path of the plugin.txt to write to
   * @param loadOrderFileName path of the loadorder.txt to write to
   * @param lockedOrderFileName path of the lockedorder.txt to write to
   * @param deleterFileName file to receive a list of files to hide from the virtual data tree. This is used to hide unchecked plugins if "hideUnchecked" is true
   * @param hideUnchecked if true, plugins that aren't enabled will be hidden from the virtual data directory
   **/
  void saveTo(const QString &pluginFileName, const QString &loadOrderFileName, const QString &lockedOrderFileName,
              const QString &deleterFileName, bool hideUnchecked) const;

  /**
   * @brief save the current load order
   *
   * the load order used by the game is defined by the last modification time which this
   * function sets. An exception is newer version of skyrim where the load order is defined
   * by the order of files in plugins.txt
   * @param directoryStructure the root directory structure representing the virtual data directory
   * @return true on success or if there was nothing to save, false if the load order can't be saved, i.e. because files are locked
   * @todo since this works on actual files the load order can't be configured per-profile. Files of the same name
   *       in different mods can also have different load orders which makes this very intransparent
   * @note also stores to disk the list of locked esps
   **/
  bool saveLoadOrder(MOShared::DirectoryEntry &directoryStructure);

  /**
   * @return number of enabled plugins in the list
   */
  int enabledCount() const;

  QString getName(int index) const { return m_ESPs.at(index).m_Name; }
  int getPriority(int index) const { return m_ESPs.at(index).m_Priority; }
  bool isESPLocked(int index) const;
  void lockESPIndex(int index, bool lock);

  bool eventFilter(QObject *obj, QEvent *event);

  static QString getColumnName(int column);
  static QString getColumnToolTip(int column);

  void refreshLoadOrder();

  void bossSort();

public:
  virtual PluginState state(const QString &name) const;
  virtual int priority(const QString &name) const;
  virtual int loadOrder(const QString &name) const;
  virtual bool isMaster(const QString &name) const;
  virtual QString origin(const QString &name) const;
  virtual bool onRefreshed(const std::function<void()> &callback);

public: // implementation of the QAbstractTableModel interface

  virtual int rowCount(const QModelIndex &parent = QModelIndex()) const;
  virtual int columnCount(const QModelIndex &parent = QModelIndex()) const;
  virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
  virtual bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole);
  virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
  virtual Qt::ItemFlags flags(const QModelIndex &index) const;
  virtual Qt::DropActions supportedDropActions() const { return Qt::MoveAction; }
  virtual bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent);

  void applyBOSSSorting(boss_db db, std::map<int, QString> &lockedLoadOrder, uint8_t **pluginList, size_t size, int &priority, int &loadOrder, bool recognized, const char *extension);
public slots:

  /**
   * @brief enables ALL plugins
   **/
  void enableAll();

  /**
   * @brief disables ALL plugins
   **/
  void disableAll();

signals:

 /**
  * @brief emitted when the plugin list changed, i.e. the load order was modified or a plugin was checked/unchecked
  * @note this is currently only used to signal that there are changes that can be saved, it does
  *       not immediately cause anything to be written to disc
  **/
 void esplist_changed();

 /**
  * @brief emitted when the plugin list should be saved
  */
 void saveTimer();

private:

  struct ESPInfo {

    ESPInfo(const QString &name, bool enabled, FILETIME time, const QString &originName, const QString &fullPath, bool hasIni);
    QString m_Name;
    bool m_Enabled;
    bool m_ForceEnabled;
    bool m_Removed;
    int m_Priority;
    int m_LoadOrder;
    FILETIME m_Time;
    QString m_OriginName;
    bool m_IsMaster;
    bool m_IsDummy;
    bool m_HasIni;
    std::set<QString> m_Masters;
    mutable std::set<QString> m_MasterUnset;
  };

  struct BossInfo {
    QStringList m_BOSSMessages;
    bool m_BOSSUnrecognized;
  };

  friend bool ByName(const ESPInfo& LHS, const ESPInfo& RHS);
  friend bool ByDate(const ESPInfo& LHS, const ESPInfo& RHS);
  friend bool ByPriority(const ESPInfo& LHS, const ESPInfo& RHS);

  class BossDLL : public PDLL {
    DECLARE_CLASS(BossDLL)

    DECLARE_FUNCTION3(__cdecl, uint32_t, CreateBossDb, boss_db*, const uint32_t, const uint8_t*)
    DECLARE_FUNCTION1(__cdecl, void, DestroyBossDb, boss_db)
    DECLARE_FUNCTION0(__cdecl, void, CleanUpAPI)

    DECLARE_FUNCTION7(__cdecl, uint32_t, SortCustomMods, boss_db, uint8_t**, size_t, uint8_t***, size_t*, uint8_t***, size_t*)
    DECLARE_FUNCTION3(__cdecl, uint32_t, SetActivePluginsDumb, boss_db, uint8_t**, const size_t)

    DECLARE_FUNCTION3(__cdecl, uint32_t, GetActivePluginsDumb , boss_db, uint8_t***, size_t*)

    DECLARE_FUNCTION2(__cdecl, uint32_t, UpdateMasterlist, boss_db, const uint8_t*)
    DECLARE_FUNCTION3(__cdecl, uint32_t, Load, boss_db, const uint8_t*, const uint8_t*)

    DECLARE_FUNCTION2(__cdecl, void, SetLoggerOutput, const char*, uint8_t)
    DECLARE_FUNCTION1(__cdecl, uint32_t, GetLastErrorDetails, uint8_t**)

    DECLARE_FUNCTION1(__cdecl, uint32_t, GetVersionString, uint8_t**)
    DECLARE_FUNCTION3(__cdecl, bool, IsCompatibleVersion, const uint32_t, const uint32_t, const uint32_t)

    DECLARE_FUNCTION4(__cdecl, uint32_t, GetPluginMessages, boss_db, const uint8_t*, BossMessage**, size_t*)

    enum ResultCode {
      RESULT_OK                                   = 0,
      RESULT_NO_MASTER_FILE                       = 1,
      RESULT_FILE_READ_FAIL                       = 2,
      RESULT_FILE_WRITE_FAIL                      = 3,
      RESULT_FILE_NOT_UTF8                        = 4,
      RESULT_FILE_NOT_FOUND                       = 5,
      RESULT_FILE_PARSE_FAIL                      = 6,
      RESULT_CONDITION_EVAL_FAIL                  = 7,
      RESULT_REGEX_EVAL_FAIL                      = 8,
      RESULT_NO_GAME_DETECTED                     = 9,
      RESULT_ENCODING_CONVERSION_FAIL             = 10,
      RESULT_FIND_ONLINE_MASTERLIST_REVISION_FAIL = 11,
      RESULT_FIND_ONLINE_MASTERLIST_DATE_FAIL     = 12,
      RESULT_READ_UPDATE_FILE_LIST_FAIL           = 13,
      RESULT_FILE_CRC_MISMATCH                    = 14,
      RESULT_FS_FILE_MOD_TIME_READ_FAIL           = 15,
      RESULT_FS_FILE_MOD_TIME_WRITE_FAIL          = 16,
      RESULT_FS_FILE_RENAME_FAIL                  = 17,
      RESULT_FS_FILE_DELETE_FAIL                  = 18,
      RESULT_FS_CREATE_DIRECTORY_FAIL             = 19,
      RESULT_FS_ITER_DIRECTORY_FAIL               = 20,
      RESULT_CURL_INIT_FAIL                       = 21,
      RESULT_CURL_SET_ERRBUFF_FAIL                = 22,
      RESULT_CURL_SET_OPTION_FAIL                 = 23,
      RESULT_CURL_SET_PROXY_FAIL                  = 24,
      RESULT_CURL_SET_PROXY_TYPE_FAIL             = 25,
      RESULT_CURL_SET_PROXY_AUTH_FAIL             = 26,
      RESULT_CURL_SET_PROXY_AUTH_TYPE_FAIL        = 27,
      RESULT_CURL_PERFORM_FAIL                    = 28,
      RESULT_CURL_USER_CANCEL                     = 29,
      RESULT_GUI_WINDOW_INIT_FAIL                 = 30,
      RESULT_NO_UPDATE_NECESSARY                  = 31,
      RESULT_LO_MISMATCH                          = 32,
      RESULT_NO_MEM                               = 33,
      RESULT_INVALID_ARGS                         = 34,
      RESULT_NETWORK_FAIL                         = 35,
      RESULT_NO_INTERNET_CONNECTION               = 36,
      RESULT_NO_TAG_MAP                           = 37,
      RESULT_PLUGINS_FULL                         = 38,
      RESULT_PLUGIN_BEFORE_MASTER                 = 39,
      RESULT_INVALID_SYNTAX                       = 40
    };

    enum GameIDs {
      AUTODETECT = 0,
      OBLIVION   = 1,
      SKYRIM     = 3,
      FALLOUT3   = 4,
      FALLOUTNV  = 5
    };
  };

private:

  void syncLoadOrder();
  void updateIndices();

  void writePlugins(const QString &fileName, bool writeUnchecked) const;
  void writeLockedOrder(const QString &fileName) const;

  bool readLoadOrder(const QString &fileName);
  void readLockedOrderFrom(const QString &fileName);
  void setPluginPriority(int row, int &newPriority);
  void changePluginPriority(std::vector<int> rows, int newPriority);

  void startSaveTime();

  void testMasters();

  boss_db initBoss();
  void convertPluginListForBoss(boss_db db, boost::ptr_vector<uint8_t> &inputPlugins, std::vector<uint8_t*> &activePlugins);

private:

  std::vector<ESPInfo> m_ESPs;

  std::map<QString, int> m_ESPsByName;
  std::vector<int> m_ESPsByPriority;

  // maps esp names to the priority specified in loadorder.txt. The esp names are
  // all lowercase!! This is to work around the fact that BOSS for some reason writes some file with
  // capitalization that doesn't match the actual name
  std::map<QString, int> m_ESPLoadOrder;
  std::map<QString, int> m_LockedOrder;

  std::map<QString, BossInfo> m_BossInfo; // maps esp names to boss information

  QString m_CurrentProfile;
  QFontMetrics m_FontMetrics;

  QTextCodec *m_Utf8Codec;
  QTextCodec *m_LocalCodec;

  mutable QTimer m_SaveTimer;

  SignalRefreshed m_Refreshed;

  BossDLL *m_BOSS;
  QTemporaryFile m_TempFile;

};



#endif // PLUGINLIST_H

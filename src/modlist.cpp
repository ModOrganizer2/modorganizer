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

#include "modlist.h"

#include "messagedialog.h"
#include "installationtester.h"
#include "qtgroupingproxy.h"
#include "viewmarkingscrollbar.h"
#include "modlistsortproxy.h"
#include <appconfig.h>
#include <utility.h>
#include <report.h>

#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QMimeData>
#include <QStandardItemModel>
#include <QMessageBox>
#include <QStringList>
#include <QEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QCheckBox>
#include <QWidgetAction>
#include <QAbstractItemView>
#include <QSortFilterProxyModel>
#include <QApplication>

#include <sstream>
#include <stdexcept>
#include <algorithm>


using namespace MOBase;


ModList::ModList(QObject *parent)
  : QAbstractItemModel(parent)
  , m_Profile(nullptr)
  , m_NexusInterface(nullptr)
  , m_Modified(false)
  , m_FontMetrics(QFont())
  , m_DropOnItems(false)
{
  m_ContentIcons[ModInfo::CONTENT_PLUGIN]    = std::make_tuple(":/MO/gui/content/plugin", tr("Game plugins (esp/esm)"));
  m_ContentIcons[ModInfo::CONTENT_INTERFACE] = std::make_tuple(":/MO/gui/content/interface", tr("Interface"));
  m_ContentIcons[ModInfo::CONTENT_MESH]      = std::make_tuple(":/MO/gui/content/mesh", tr("Meshes"));
  m_ContentIcons[ModInfo::CONTENT_BSA]       = std::make_tuple(":/MO/gui/content/bsa", tr("BSA"));
  m_ContentIcons[ModInfo::CONTENT_MUSIC]     = std::make_tuple(":/MO/gui/content/music", tr("Music"));
  m_ContentIcons[ModInfo::CONTENT_SCRIPT]    = std::make_tuple(":/MO/gui/content/script", tr("Scripts (Papyrus)"));
  m_ContentIcons[ModInfo::CONTENT_SKSE]      = std::make_tuple(":/MO/gui/content/skse", tr("Script Extender Plugin"));
  m_ContentIcons[ModInfo::CONTENT_SKYPROC]   = std::make_tuple(":/MO/gui/content/skyproc", tr("SkyProc Patcher"));
  m_ContentIcons[ModInfo::CONTENT_SOUND]     = std::make_tuple(":/MO/gui/content/sound", tr("Sound"));
  m_ContentIcons[ModInfo::CONTENT_STRING]    = std::make_tuple(":/MO/gui/content/string", tr("Strings"));
  m_ContentIcons[ModInfo::CONTENT_TEXTURE]   = std::make_tuple(":/MO/gui/content/texture", tr("Textures"));

  m_LastCheck.start();
}

ModList::~ModList()
{
  m_ModStateChanged.disconnect_all_slots();
  m_ModMoved.disconnect_all_slots();
}

void ModList::setProfile(Profile *profile)
{
  m_Profile = profile;
}

int ModList::rowCount(const QModelIndex &parent) const
{
  if (!parent.isValid()) {
    return ModInfo::getNumMods();
  } else {
    return 0;
  }
}

bool ModList::hasChildren(const QModelIndex &parent) const
{
  if (!parent.isValid()) {
    return ModInfo::getNumMods() > 0;
  } else {
    return false;
  }
}


int ModList::columnCount(const QModelIndex &) const
{
  return COL_LASTCOLUMN + 1;
}


QVariant ModList::getOverwriteData(int column, int role) const
{
  switch (role) {
    case Qt::DisplayRole: {
      if (column == 0) {
        return "Overwrite";
      } else {
        return QVariant();
      }
    } break;
    case Qt::CheckStateRole: {
      if (column == 0) {
        return Qt::Checked;
      } else {
        return QVariant();
      }
    } break;
    case Qt::TextAlignmentRole: return QVariant(Qt::AlignCenter | Qt::AlignVCenter);
    case Qt::UserRole: return -1;
    case Qt::ForegroundRole: return QBrush(Qt::red);
    case Qt::ToolTipRole: return tr("This entry contains files that have been created inside the virtual data tree (i.e. by the construction kit)");
    default: return QVariant();
  }
}


QString ModList::getFlagText(ModInfo::EFlag flag, ModInfo::Ptr modInfo) const
{
  switch (flag) {
    case ModInfo::FLAG_BACKUP: return tr("Backup");
    case ModInfo::FLAG_INVALID: return tr("No valid game data");
    case ModInfo::FLAG_NOTENDORSED: return tr("Not endorsed yet");
    case ModInfo::FLAG_NOTES: return QString("<i>%1</i>").arg(modInfo->notes().replace("\n", "<br>"));
    case ModInfo::FLAG_CONFLICT_OVERWRITE: return tr("Overwrites files");
    case ModInfo::FLAG_CONFLICT_OVERWRITTEN: return tr("Overwritten files");
    case ModInfo::FLAG_CONFLICT_MIXED: return tr("Overwrites & Overwritten");
    case ModInfo::FLAG_CONFLICT_REDUNDANT: return tr("Redundant");
    default: return "";
  }
}


QVariantList ModList::contentsToIcons(const std::vector<ModInfo::EContent> &contents) const
{
  QVariantList result;
  std::set<ModInfo::EContent> contentsSet(contents.begin(), contents.end());
  for (auto iter = m_ContentIcons.begin(); iter != m_ContentIcons.end(); ++iter) {
    if (contentsSet.find(iter->first) != contentsSet.end()) {
      result.append(std::get<0>(iter->second));
    } else {
      result.append(QString());
    }
  }
  return result;
}

QString ModList::contentsToToolTip(const std::vector<ModInfo::EContent> &contents) const
{
  QString result("<table cellspacing=7>");

  std::set<ModInfo::EContent> contentsSet(contents.begin(), contents.end());
  for (auto iter = m_ContentIcons.begin(); iter != m_ContentIcons.end(); ++iter) {
    if (contentsSet.find(iter->first) != contentsSet.end()) {
      result.append(QString("<tr><td><img src=\"%1\" width=32/></td>"
                            "<td valign=\"middle\">%2</td></tr>")
                    .arg(std::get<0>(iter->second)).arg(std::get<1>(iter->second)));
    }
  }
  result.append("</table>");
  return result;
}


QVariant ModList::data(const QModelIndex &modelIndex, int role) const
{
  if (m_Profile == nullptr) return QVariant();
  if (!modelIndex.isValid()) return QVariant();
  unsigned int modIndex = modelIndex.row();
  int column = modelIndex.column();

  ModInfo::Ptr modInfo = ModInfo::getByIndex(modIndex);
  if ((role == Qt::DisplayRole) ||
      (role == Qt::EditRole)) {
    if ((column == COL_FLAGS)
        || (column == COL_CONTENT)) {
      return QVariant();
    } else if (column == COL_NAME) {
      return modInfo->name();
    } else if (column == COL_VERSION) {
      VersionInfo verInfo = modInfo->getVersion();
      QString version = verInfo.displayString();
      if (role != Qt::EditRole) {
        if (version.isEmpty() && modInfo->canBeUpdated()) {
          version = "?";
        }
      }
      return version;
    } else if (column == COL_PRIORITY) {
      int priority = modInfo->getFixedPriority();
      if (priority != INT_MIN) {
        return QVariant(); // hide priority for mods where it's fixed
      } else {
        return m_Profile->getModPriority(modIndex);
      }
    } else if (column == COL_MODID) {
      int modID = modInfo->getNexusID();
      if (modID >= 0) {
        return modID;
      } else {
        return QVariant();
      }
    } else if (column == COL_CATEGORY) {
      if (modInfo->hasFlag(ModInfo::FLAG_FOREIGN)) {
        return tr("Non-MO");
      } else {
        int category = modInfo->getPrimaryCategory();
        if (category != -1) {
          CategoryFactory &categoryFactory = CategoryFactory::instance();
          if (categoryFactory.categoryExists(category)) {
            try {
              int categoryIdx = categoryFactory.getCategoryIndex(category);
              return categoryFactory.getCategoryName(categoryIdx);
            } catch (const std::exception &e) {
              qCritical("failed to retrieve category name: %s", e.what());
              return QString();
            }
          } else {
            qWarning("category %d doesn't exist (may have been removed)", category);
            modInfo->setCategory(category, false);
            return QString();
          }
        } else {
          return QVariant();
        }
      }
    } else if (column == COL_INSTALLTIME) {
      // display installation time for mods that can be updated
      if (modInfo->creationTime().isValid()) {
        return modInfo->creationTime();
      } else {
        return QVariant();
      }
    } else {
      return tr("invalid");
    }
  } else if ((role == Qt::CheckStateRole) && (column == 0)) {
    if (modInfo->canBeEnabled()) {
      return m_Profile->modEnabled(modIndex) ? Qt::Checked : Qt::Unchecked;
    } else {
      return QVariant();
    }
  } else if (role == Qt::TextAlignmentRole) {
    if (column == COL_NAME) {
      if (modInfo->getHighlight() & ModInfo::HIGHLIGHT_CENTER) {
        return QVariant(Qt::AlignCenter | Qt::AlignVCenter);
      } else {
        return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
      }
    } else if (column == COL_VERSION) {
      return QVariant(Qt::AlignRight | Qt::AlignVCenter);
    } else {
      return QVariant(Qt::AlignCenter | Qt::AlignVCenter);
    }
  } else if (role == Qt::UserRole) {
    if (column == COL_CATEGORY) {
      QVariantList categoryNames;
      std::set<int> categories = modInfo->getCategories();
      CategoryFactory &categoryFactory = CategoryFactory::instance();
      for (auto iter = categories.begin(); iter != categories.end(); ++iter) {
        categoryNames.append(categoryFactory.getCategoryName(categoryFactory.getCategoryIndex(*iter)));
      }
      if (categoryNames.count() != 0) {
        return categoryNames;
      } else {
        return QVariant();
      }
    } else if (column == COL_PRIORITY) {
      int priority = modInfo->getFixedPriority();
      if (priority != INT_MIN) {
        return priority;
      } else {
        return m_Profile->getModPriority(modIndex);
      }
    } else {
      return modInfo->getNexusID();
    }
  } else if (role == Qt::UserRole + 1) {
    return modIndex;
  } else if (role == Qt::UserRole + 2) {
    switch (column) {
      case COL_MODID:    return QtGroupingProxy::AGGR_FIRST;
      case COL_VERSION:  return QtGroupingProxy::AGGR_MAX;
      case COL_CATEGORY: return QtGroupingProxy::AGGR_FIRST;
      case COL_PRIORITY: return QtGroupingProxy::AGGR_MIN;
      default: return QtGroupingProxy::AGGR_NONE;
    }
  } else if (role == Qt::UserRole + 3) {
    return contentsToIcons(modInfo->getContents());
  } else if (role == Qt::FontRole) {
    QFont result;
    if (column == COL_NAME) {
      if (modInfo->getHighlight() & ModInfo::HIGHLIGHT_INVALID) {
        result.setItalic(true);
      }
    } else if ((column == COL_CATEGORY) && (modInfo->hasFlag(ModInfo::FLAG_FOREIGN))) {
      result.setItalic(true);
    } else if (column == COL_VERSION) {
      if (modInfo->updateAvailable() || modInfo->downgradeAvailable()) {
        result.setWeight(QFont::Bold);
      }
    }
    return result;
  } else if (role == Qt::DecorationRole) {
    if (column == COL_VERSION) {
      if (modInfo->updateAvailable()) {
        return QIcon(":/MO/gui/update_available");
      } else if (modInfo->downgradeAvailable()) {
        return QIcon(":/MO/gui/warning");
      } else if (modInfo->getVersion().scheme() == VersionInfo::SCHEME_DATE) {
        return QIcon(":/MO/gui/version_date");
      }
    }
    return QVariant();
  } else if (role == Qt::ForegroundRole) {
    if (column == COL_NAME) {
      int highlight = modInfo->getHighlight();
      if (highlight & ModInfo::HIGHLIGHT_IMPORTANT)    return QBrush(Qt::darkRed);
      else if (highlight & ModInfo::HIGHLIGHT_INVALID) return QBrush(Qt::darkGray);
    } else if (column == COL_VERSION) {
      if (!modInfo->getNewestVersion().isValid()) {
        return QVariant();
      } else if (modInfo->updateAvailable() || modInfo->downgradeAvailable()) {
        return QBrush(Qt::red);
      } else {
        return QBrush(Qt::darkGreen);
      }
    }
    return QVariant();
  } else if ((role == Qt::BackgroundRole)
             || (role == ViewMarkingScrollBar::DEFAULT_ROLE)) {
    if (m_Overwrite.find(modIndex) != m_Overwrite.end()) {
      return QColor(0, 255, 0, 32);
    } else if (m_Overwritten.find(modIndex) != m_Overwritten.end()) {
      return QColor(255, 0, 0, 32);
    } else {
      return QVariant();
    }
  } else if (role == Qt::ToolTipRole) {
    if (column == COL_FLAGS) {
      QString result;

      for (ModInfo::EFlag flag : modInfo->getFlags()) {
        if (result.length() != 0) result += "<br>";
        result += getFlagText(flag, modInfo);
      }

      return result;
    } else if (column == COL_CONTENT) {
      return contentsToToolTip(modInfo->getContents());
    } else if (column == COL_NAME) {
      try {
        return modInfo->getDescription();
      } catch (const std::exception &e) {
        qCritical("invalid mod description: %s", e.what());
        return QString();
      }
    } else if (column == COL_VERSION) {
      QString text = tr("installed version: \"%1\", newest version: \"%2\"").arg(modInfo->getVersion().displayString()).arg(modInfo->getNewestVersion().displayString());
      if (modInfo->downgradeAvailable()) {
        text += "<br>" + tr("The newest version on Nexus seems to be older than the one you have installed. This could either mean the version you have has been withdrawn "
                          "(i.e. due to a bug) or the author uses a non-standard versioning scheme and that newest version is actually newer. "
                          "Either way you may want to \"upgrade\".");
      }
      return text;
    } else if (column == COL_CATEGORY) {
      const std::set<int> &categories = modInfo->getCategories();
      std::wostringstream categoryString;
      categoryString << ToWString(tr("Categories: <br>"));
      CategoryFactory &categoryFactory = CategoryFactory::instance();
      for (std::set<int>::const_iterator catIter = categories.begin();
           catIter != categories.end(); ++catIter) {
        if (catIter != categories.begin()) {
          categoryString << " , ";
        }
        try {
          categoryString << "<span style=\"white-space: nowrap;\"><i>" << ToWString(categoryFactory.getCategoryName(categoryFactory.getCategoryIndex(*catIter))) << "</font></span>";
        } catch (const std::exception &e) {
          qCritical("failed to generate tooltip: %s", e.what());
          return QString();
        }
      }

      return ToQString(categoryString.str());
    } else {
      return QVariant();
    }
  } else {
    return QVariant();
  }
}


bool ModList::renameMod(int index, const QString &newName)
{
  QString nameFixed = newName;
  if (!fixDirectoryName(nameFixed) || nameFixed.isEmpty()) {
    MessageDialog::showMessage(tr("Invalid name"), nullptr);
    return false;
  }

  // before we rename, write back the current profile so we don't lose changes and to ensure
  // there is no scheduled asynchronous rewrite anytime soon
  m_Profile->writeModlistNow();

  ModInfo::Ptr modInfo = ModInfo::getByIndex(index);
  QString oldName = modInfo->name();
  if (modInfo->setName(nameFixed)) {
    // this just disabled the mod in all profiles. The recipient of modRenamed must fix that
    emit modRenamed(oldName, nameFixed);
  }

  // invalidate the currently displayed state of this list
  notifyChange(-1);
  return true;
}

bool ModList::setData(const QModelIndex &index, const QVariant &value, int role)
{
  if (m_Profile == nullptr) return false;

  if (static_cast<unsigned int>(index.row()) >= ModInfo::getNumMods()) {
    return false;
  }

  int modID = index.row();

  ModInfo::Ptr info = ModInfo::getByIndex(modID);
  IModList::ModStates oldState = state(modID);

  bool result = false;

  emit aboutToChangeData();

  if (role == Qt::CheckStateRole) {
    bool enabled = value.toInt() == Qt::Checked;
    if (m_Profile->modEnabled(modID) != enabled) {
      m_Profile->setModEnabled(modID, enabled);
      m_Modified = true;
      m_LastCheck.restart();
      emit modlist_changed(index, role);
    }
    result = true;
    emit dataChanged(index, index);
  } else if (role == Qt::EditRole) {
    switch (index.column()) {
      case COL_NAME: {
        result = renameMod(modID, value.toString());
      } break;
      case COL_PRIORITY: {
        bool ok = false;
        int newPriority = value.toInt(&ok);
        if (ok) {
          m_Profile->setModPriority(modID, newPriority);

          emit modorder_changed();
          result = true;
        } else {
          result = false;
        }
      } break;
      case COL_MODID: {
        bool ok = false;
        int newID = value.toInt(&ok);
        if (ok) {
          info->setNexusID(newID);
          emit modlist_changed(index, role);
          result = true;
        } else {
          result = false;
        }
      } break;
      case COL_VERSION: {
        VersionInfo::VersionScheme scheme = info->getVersion().scheme();
        VersionInfo version(value.toString(), scheme, true);
        if (version.isValid()) {
          info->setVersion(version);
          result = true;
        } else {
          result = false;
        }
      } break;
      default: {
        qWarning("edit on column \"%s\" not supported",
                 getColumnName(index.column()).toUtf8().constData());
        result = false;
      } break;
    }
    if (result) {
      emit dataChanged(index, index);
    }
  }

  emit postDataChanged();

  IModList::ModStates newState = state(modID);
  if (oldState != newState) {
    try {
      m_ModStateChanged(info->name(), newState);
    } catch (const std::exception &e) {
      qCritical("failed to invoke state changed notification: %s", e.what());
    } catch (...) {
      qCritical("failed to invoke state changed notification: unknown exception");
    }
  }

  return result;
}


QVariant ModList::headerData(int section, Qt::Orientation orientation,
                             int role) const
{
  if (orientation == Qt::Horizontal) {
    if (role == Qt::DisplayRole) {
      return getColumnName(section);
    } else if (role == Qt::ToolTipRole) {
      return getColumnToolTip(section);
    } else if (role == Qt::TextAlignmentRole) {
      return QVariant(Qt::AlignCenter);
    } else if (role == Qt::SizeHintRole) {
      QSize temp = m_FontMetrics.size(Qt::TextSingleLine, getColumnName(section));
      temp.rwidth() += 20;
      temp.rheight() += 12;
      return temp;
    }
  }
  return QAbstractItemModel::headerData(section, orientation, role);
}


Qt::ItemFlags ModList::flags(const QModelIndex &modelIndex) const
{
  Qt::ItemFlags result = QAbstractItemModel::flags(modelIndex);
  if (modelIndex.internalId() < 0) {
    return Qt::ItemIsEnabled;
  }
  if (modelIndex.isValid()) {
    ModInfo::Ptr modInfo = ModInfo::getByIndex(modelIndex.row());
    if (modInfo->getFixedPriority() == INT_MIN) {
      result |= Qt::ItemIsDragEnabled;
      result |= Qt::ItemIsUserCheckable;
      if ((modelIndex.column() == COL_NAME) ||
          (modelIndex.column() == COL_PRIORITY) ||
          (modelIndex.column() == COL_VERSION) ||
          (modelIndex.column() == COL_MODID)) {
        result |= Qt::ItemIsEditable;
      }
    }
    std::vector<ModInfo::EFlag> flags = modInfo->getFlags();
    if (m_DropOnItems
        && (std::find(flags.begin(), flags.end(), ModInfo::FLAG_OVERWRITE) == flags.end())) {
      result |= Qt::ItemIsDropEnabled;
    }
  } else {
    if (!m_DropOnItems) result |= Qt::ItemIsDropEnabled;
  }
  return result;
}


QStringList ModList::mimeTypes() const
{
  QStringList result = QAbstractItemModel::mimeTypes();
  result.append("text/uri-list");
  return result;
}

QMimeData *ModList::mimeData(const QModelIndexList &indexes) const
{
  QMimeData *result = QAbstractItemModel::mimeData(indexes);
  result->setData("text/plain", "mod");
  return result;
}

void ModList::changeModPriority(std::vector<int> sourceIndices, int newPriority)
{
  if (m_Profile == nullptr) return;

  emit layoutAboutToBeChanged();
  Profile *profile = m_Profile;
  // sort rows to insert by their old priority (ascending) and insert them move them in that order
  std::sort(sourceIndices.begin(), sourceIndices.end(),
            [profile](const int &LHS, const int &RHS) {
              return profile->getModPriority(LHS) < profile->getModPriority(RHS);
            });

  // odd stuff: if any of the dragged sources has priority lower than the destination then the
  // target idx is that of the row BELOW the dropped location, otherwise it's the one above. why?
  for (std::vector<int>::const_iterator iter = sourceIndices.begin();
       iter != sourceIndices.end(); ++iter) {
    if (profile->getModPriority(*iter) < newPriority) {
      --newPriority;
      break;
    }
  }
  for (std::vector<int>::const_iterator iter = sourceIndices.begin();
       iter != sourceIndices.end(); ++iter) {
    int oldPriority = m_Profile->getModPriority(*iter);
    m_Profile->setModPriority(*iter, newPriority);
    m_ModMoved(ModInfo::getByIndex(*iter)->name(), oldPriority, newPriority);
  }

  emit layoutChanged();

  emit modorder_changed();
}


void ModList::changeModPriority(int sourceIndex, int newPriority)
{
  if (m_Profile == nullptr) return;
  emit layoutAboutToBeChanged();

  m_Profile->setModPriority(sourceIndex, newPriority);

  emit layoutChanged();

  emit modorder_changed();
}

void ModList::setOverwriteMarkers(const std::set<unsigned int> &overwrite, const std::set<unsigned int> &overwritten)
{
  m_Overwrite = overwrite;
  m_Overwritten = overwritten;
  notifyChange(0, rowCount() - 1);
}

bool ModList::modInfoAboutToChange(ModInfo::Ptr info)
{
  if (m_ChangeInfo.name.isEmpty()) {
    m_ChangeInfo.name = info->name();
    m_ChangeInfo.state = state(info->name());
    return true;
  } else {
    return false;
  }
}

void ModList::modInfoChanged(ModInfo::Ptr info)
{
  if (info->name() == m_ChangeInfo.name) {
    IModList::ModStates newState = state(info->name());
    if (m_ChangeInfo.state != newState) {
      m_ModStateChanged(info->name(), newState);
    }

    int row = ModInfo::getIndex(info->name());
    info->testValid();
    emit dataChanged(index(row, 0), index(row, columnCount()));
  } else {
    qCritical("modInfoChanged not called after modInfoAboutToChange");
  }
  m_ChangeInfo.name = QString();
}

void ModList::disconnectSlots() {
  m_ModMoved.disconnect_all_slots();
  m_ModStateChanged.disconnect_all_slots();
}

int ModList::timeElapsedSinceLastChecked() const
{
  return m_LastCheck.elapsed();
}

IModList::ModStates ModList::state(unsigned int modIndex) const
{
  IModList::ModStates result;
  if (modIndex != UINT_MAX) {
    result |= IModList::STATE_EXISTS;
    ModInfo::Ptr modInfo = ModInfo::getByIndex(modIndex);
    if (modInfo->isEmpty()) {
      result |= IModList::STATE_EMPTY;
    }
    if (modInfo->endorsedState() == ModInfo::ENDORSED_TRUE) {
      result |= IModList::STATE_ENDORSED;
    }
    if (modInfo->isValid()) {
      result |= IModList::STATE_VALID;
    }
    if (modInfo->canBeEnabled()) {
      if (m_Profile->modEnabled(modIndex)) {
        result |= IModList::STATE_ACTIVE;
      }
    } else {
      result |= IModList::STATE_ESSENTIAL;
    }
  }
  return result;
}

QString ModList::displayName(const QString &internalName) const
{
  unsigned int modIndex = ModInfo::getIndex(internalName);
  if (modIndex == UINT_MAX) {
    // might be better to throw an exception?
    return internalName;
  } else {
    return ModInfo::getByIndex(modIndex)->name();
  }
}

QStringList ModList::allMods() const
{
  QStringList result;
  for (unsigned int i = 0; i < ModInfo::getNumMods(); ++i) {
    result.append(ModInfo::getByIndex(i)->internalName());
  }
  return result;
}

IModList::ModStates ModList::state(const QString &name) const
{
  unsigned int modIndex = ModInfo::getIndex(name);

  return state(modIndex);
}

bool ModList::setActive(const QString &name, bool active)
{
  unsigned int modIndex = ModInfo::getIndex(name);
  if (modIndex == UINT_MAX) {
    return false;
  } else {
    m_Profile->setModEnabled(modIndex, active);

    IModList::ModStates newState = state(modIndex);
    m_ModStateChanged(name, newState);
    return true;
  }
}

int ModList::priority(const QString &name) const
{
  unsigned int modIndex = ModInfo::getIndex(name);
  if (modIndex == UINT_MAX) {
    return -1;
  } else {
    return m_Profile->getModPriority(modIndex);
  }
}

bool ModList::setPriority(const QString &name, int newPriority)
{
  if ((newPriority < 0) || (newPriority >= static_cast<int>(m_Profile->numRegularMods()))) {
    return false;
  }

  unsigned int modIndex = ModInfo::getIndex(name);
  if (modIndex == UINT_MAX) {
    return false;
  } else {
    m_Profile->setModPriority(modIndex, newPriority);
    notifyChange(modIndex);
    return true;
  }
}

bool ModList::onModStateChanged(const std::function<void (const QString &, IModList::ModStates)> &func)
{
  auto conn = m_ModStateChanged.connect(func);
  return conn.connected();
}

bool ModList::onModMoved(const std::function<void (const QString &, int, int)> &func)
{
  auto conn = m_ModMoved.connect(func);
  return conn.connected();
}

bool ModList::dropURLs(const QMimeData *mimeData, int row, const QModelIndex &parent)
{
  QStringList source;
  QStringList target;

  if (row == -1) {
    row = parent.row();
  }

  ModInfo::Ptr modInfo = ModInfo::getByIndex(row);
  QDir modDirectory(modInfo->absolutePath());
  QDir gameDirectory(qApp->property("dataPath").toString() + "/" + QString::fromStdWString(AppConfig::overwritePath()));

  unsigned int overwriteIndex = ModInfo::findMod([] (ModInfo::Ptr mod) -> bool {
    std::vector<ModInfo::EFlag> flags = mod->getFlags();
    return std::find(flags.begin(), flags.end(), ModInfo::FLAG_OVERWRITE) != flags.end(); });

  QString overwriteName = ModInfo::getByIndex(overwriteIndex)->name();

  for (const QUrl &url : mimeData->urls()) {
    if (!url.isLocalFile()) {
      continue;
    }
    QString relativePath = gameDirectory.relativeFilePath(url.toLocalFile());
    if (relativePath.startsWith("..")) {
      qDebug("%s drop ignored", qPrintable(url.toLocalFile()));
      continue;
    }
    source.append(url.toLocalFile());
    target.append(modDirectory.absoluteFilePath(relativePath));
    emit fileMoved(relativePath, overwriteName, modInfo->name());
  }

  if (source.count() != 0) {
    shellMove(source, target);
  }

  return true;
}

bool ModList::dropMod(const QMimeData *mimeData, int row, const QModelIndex &parent)
{

  try {
    QByteArray encoded = mimeData->data("application/x-qabstractitemmodeldatalist");
    QDataStream stream(&encoded, QIODevice::ReadOnly);
    std::vector<int> sourceRows;

    while (!stream.atEnd()) {
      int sourceRow, col;
      QMap<int,  QVariant> roleDataMap;
      stream >> sourceRow >> col >> roleDataMap;
      if (col == 0) {
        sourceRows.push_back(sourceRow);
      }
    }

    if (row == -1) {
      row = parent.row();
    }

    if ((row < 0) || (static_cast<unsigned int>(row) >= ModInfo::getNumMods())) {
      return false;
    }

    int newPriority = 0;
    {
      if ((row < 0) || (row > static_cast<int>(m_Profile->numRegularMods()))) {
        newPriority = m_Profile->numRegularMods() + 1;
      } else {
        newPriority = m_Profile->getModPriority(row);
      }
      if (newPriority == -1) {
        newPriority = m_Profile->numRegularMods() + 1;
      }
    }
    changeModPriority(sourceRows, newPriority);
  } catch (const std::exception &e) {
    reportError(tr("drag&drop failed: %1").arg(e.what()));
  }

  return false;
}


bool ModList::dropMimeData(const QMimeData *mimeData, Qt::DropAction action, int row, int, const QModelIndex &parent)
{
  if (action == Qt::IgnoreAction) {
    return true;
  }

  if (m_Profile == nullptr) return false;

  if (mimeData->hasUrls()) {
    return dropURLs(mimeData, row, parent);
  } else if (mimeData->hasText()) {
    return dropMod(mimeData, row, parent);
  } else {
    return false;
  }
}

void ModList::removeRowForce(int row, const QModelIndex &parent)
{
  if (static_cast<unsigned int>(row) >= ModInfo::getNumMods()) {
    return;
  }
  if (m_Profile == nullptr) return;

  ModInfo::Ptr modInfo = ModInfo::getByIndex(row);

  bool wasEnabled = m_Profile->modEnabled(row);

  m_Profile->setModEnabled(row, false);

  m_Profile->modlistWriter().cancel();
  beginRemoveRows(parent, row, row);
  ModInfo::removeMod(row);
  endRemoveRows();
  m_Profile->refreshModStatus();  // removes the mod from the status list
  m_Profile->modlistWriter().write(); // this ensures the modified list gets written back before new mods can be installed

  if (wasEnabled) {
    emit removeOrigin(modInfo->name());
  }

  emit modUninstalled(modInfo->getInstallationFile());
}

bool ModList::removeRows(int row, int count, const QModelIndex &parent)
{
  if (static_cast<unsigned int>(row) >= ModInfo::getNumMods()) {
    return false;
  }
  if (m_Profile == nullptr) {
    return false;
  }

  bool success = false;

  for (int i = 0; i < count; ++i) {
    ModInfo::Ptr modInfo = ModInfo::getByIndex(row + i);
    if (!modInfo->isRegular()) {
      continue;
    }

    success = true;

    QMessageBox confirmBox(QMessageBox::Question, tr("Confirm"),
                           tr("Are you sure you want to remove \"%1\"?").arg(modInfo->name()),
                           QMessageBox::Yes | QMessageBox::No);

    if (confirmBox.exec() == QMessageBox::Yes) {
      m_Profile->setModEnabled(row + i, false);
      removeRowForce(row + i, parent);
    }
  }

  return success;
}


void ModList::notifyChange(int rowStart, int rowEnd)
{
  if (rowStart < 0) {
    m_Overwrite.clear();
    m_Overwritten.clear();
    beginResetModel();
    endResetModel();
  } else {
    if (rowEnd == -1) {
      rowEnd = rowStart;
    }
    emit dataChanged(this->index(rowStart, 0), this->index(rowEnd, this->columnCount() - 1));
  }
}


QModelIndex ModList::index(int row, int column, const QModelIndex&) const
{
  if ((row < 0) || (row >= rowCount()) || (column < 0) || (column >= columnCount())) {
    return QModelIndex();
  }
  QModelIndex res = createIndex(row, column, row);
  return res;
}


QModelIndex ModList::parent(const QModelIndex&) const
{
  return QModelIndex();
}

QMap<int, QVariant> ModList::itemData(const QModelIndex &index) const
{
  QMap<int, QVariant> result = QAbstractItemModel::itemData(index);
  result[Qt::UserRole] = data(index, Qt::UserRole);
  return result;
}


void ModList::dropModeUpdate(bool dropOnItems)
{
  if (m_DropOnItems != dropOnItems) {
    m_DropOnItems = dropOnItems;
  }
}


QString ModList::getColumnName(int column)
{
  switch (column) {
    case COL_FLAGS:    return tr("Flags");
    case COL_CONTENT:  return tr("Content");
    case COL_NAME:     return tr("Mod Name");
    case COL_VERSION:  return tr("Version");
    case COL_PRIORITY: return tr("Priority");
    case COL_CATEGORY: return tr("Category");
    case COL_MODID:    return tr("Nexus ID");
    case COL_INSTALLTIME: return tr("Installation");
    default: return tr("unknown");
  }
}


QString ModList::getColumnToolTip(int column)
{
  switch (column) {
    case COL_NAME:     return tr("Name of your mods");
    case COL_VERSION:  return tr("Version of the mod (if available)");
    case COL_PRIORITY: return tr("Installation priority of your mod. The higher, the more \"important\" it is and thus "
                                 "overwrites files from mods with lower priority.");
    case COL_CATEGORY: return tr("Category of the mod.");
    case COL_MODID:    return tr("Id of the mod as used on Nexus.");
    case COL_FLAGS:    return tr("Emblemes to highlight things that might require attention.");
    case COL_CONTENT:  return tr("Depicts the content of the mod:<br>"
                                 "<table cellspacing=7>"
                                 "<tr><td><img src=\":/MO/gui/content/plugin\" width=32/></td><td>Game plugins (esp/esm)</tr>"
                                 "<tr><td><img src=\":/MO/gui/content/interface\" width=32/></td><td>Interface</tr>"
                                 "<tr><td><img src=\":/MO/gui/content/mesh\" width=32/></td><td>Meshes</tr>"
                                 "<tr><td><img src=\":/MO/gui/content/bsa\" width=32/></td><td>BSA</tr>"
                                 "<tr><td><img src=\":/MO/gui/content/texture\" width=32/></td><td>Textures</tr>"
                                 "<tr><td><img src=\":/MO/gui/content/sound\" width=32/></td><td>Sounds</tr>"
                                 "<tr><td><img src=\":/MO/gui/content/music\" width=32/></td><td>Music</tr>"
                                 "<tr><td><img src=\":/MO/gui/content/string\" width=32/></td><td>Strings</tr>"
                                 "<tr><td><img src=\":/MO/gui/content/script\" width=32/></td><td>Scripts (Papyrus)</tr>"
                                 "<tr><td><img src=\":/MO/gui/content/skse\" width=32/></td><td>Script Extender plugins</tr>"
                                 "<tr><td><img src=\":/MO/gui/content/skyproc\" width=32/></td><td>SkyProc Patcher</tr>"
                                 "</table>");
    case COL_INSTALLTIME: return tr("Time this mod was installed");
    default: return tr("unknown");
  }
}


bool ModList::moveSelection(QAbstractItemView *itemView, int direction)
{
  QItemSelectionModel *selectionModel = itemView->selectionModel();

  const QAbstractProxyModel *proxyModel = qobject_cast<const QAbstractProxyModel*>(selectionModel->model());
  const QSortFilterProxyModel *filterModel = nullptr;

  while ((filterModel == nullptr) && (proxyModel != nullptr)) {
    filterModel = qobject_cast<const QSortFilterProxyModel*>(proxyModel);
    if (filterModel == nullptr) {
      proxyModel = qobject_cast<const QAbstractProxyModel*>(proxyModel->sourceModel());
    }
  }
  if (filterModel == nullptr) {
    return true;
  }

  int offset = -1;
  if (((direction < 0) && (filterModel->sortOrder() == Qt::DescendingOrder)) ||
      ((direction > 0) && (filterModel->sortOrder() == Qt::AscendingOrder))) {
    offset = 1;
  }

  QModelIndexList rows = selectionModel->selectedRows();
  if (direction > 0) {
    for (int i = 0; i < rows.size() / 2; ++i) {
      rows.swap(i, rows.size() - i - 1);
    }
  }
  for (QModelIndex idx : rows) {
    if (filterModel != nullptr) {
      idx = filterModel->mapToSource(idx);
    }
    int newPriority = m_Profile->getModPriority(idx.row()) + offset;
    if ((newPriority >= 0) && (newPriority < static_cast<int>(m_Profile->numRegularMods()))) {
      m_Profile->setModPriority(idx.row(), newPriority);
      notifyChange(idx.row());
    }
  }
  emit modorder_changed();
  return true;
}

bool ModList::deleteSelection(QAbstractItemView *itemView)
{
  QItemSelectionModel *selectionModel = itemView->selectionModel();

  QModelIndexList rows = selectionModel->selectedRows();
  if (rows.count() > 1) {
    emit removeSelectedMods();
  } else if (rows.count() == 1) {
    removeRow(rows[0].data(Qt::UserRole + 1).toInt(), QModelIndex());
  }
  return true;
}

bool ModList::toggleSelection(QAbstractItemView *itemView)
{
  emit aboutToChangeData();

  QItemSelectionModel *selectionModel = itemView->selectionModel();

  for (QModelIndex idx : selectionModel->selectedRows()) {
    int modId = idx.data(Qt::UserRole + 1).toInt();
    m_Profile->setModEnabled(modId, !m_Profile->modEnabled(modId));
    emit modlist_changed(idx, 0);
  }

  m_Modified = true;
  m_LastCheck.restart();

  emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));

  emit postDataChanged();

  return true;
}

bool ModList::eventFilter(QObject *obj, QEvent *event)
{
  if (event->type() == QEvent::ContextMenu) {
    QContextMenuEvent *contextEvent = static_cast<QContextMenuEvent*>(event);
    QWidget *object = qobject_cast<QWidget*>(obj);
    if ((object != nullptr) && (contextEvent->reason() == QContextMenuEvent::Mouse)) {
      emit requestColumnSelect(object->mapToGlobal(contextEvent->pos()));

      return true;
    }
  } else if ((event->type() == QEvent::KeyPress) && (m_Profile != nullptr)) {
    QAbstractItemView *itemView = qobject_cast<QAbstractItemView*>(obj);
    QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);

    if ((itemView != nullptr)
        && (keyEvent->modifiers() == Qt::ControlModifier)
        && ((keyEvent->key() == Qt::Key_Up) || (keyEvent->key() == Qt::Key_Down))) {
      return moveSelection(itemView, keyEvent->key() == Qt::Key_Up ? -1 : 1);
    } else if (keyEvent->key() == Qt::Key_Delete) {
      return deleteSelection(itemView);
    } else if (keyEvent->key() == Qt::Key_Space) {
      return toggleSelection(itemView);
    }
  }
  return QAbstractItemModel::eventFilter(obj, event);
}


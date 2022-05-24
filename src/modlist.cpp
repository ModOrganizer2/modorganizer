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
#include "modinforegular.h"
#include "modlistdropinfo.h"
#include "modlistsortproxy.h"
#include "organizercore.h"
#include "pluginlist.h"
#include "qtgroupingproxy.h"
#include "settings.h"
#include "shared/directoryentry.h"
#include "shared/fileentry.h"
#include "shared/filesorigin.h"
#include "viewmarkingscrollbar.h"
#include "widgetutility.h"

#include "filesystemutilities.h"
#include "shared/appconfig.h"
#include <report.h>

#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QContextMenuEvent>
#include <QDir>
#include <QDirIterator>
#include <QEvent>
#include <QFileInfo>
#include <QFontDatabase>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QStringList>
#include <QWidgetAction>

#include <algorithm>
#include <sstream>
#include <stdexcept>

using namespace MOBase;

ModList::ModList(PluginManager* pluginManager, OrganizerCore* organizer)
    : QAbstractItemModel(organizer), m_Organizer(organizer), m_Profile(nullptr),
      m_NexusInterface(nullptr), m_Modified(false), m_InNotifyChange(false),
      m_FontMetrics(QFont()), m_PluginManager(pluginManager)
{
  m_LastCheck.start();
}

ModList::~ModList()
{
  m_ModInstalled.disconnect_all_slots();
  m_ModRemoved.disconnect_all_slots();
  m_ModStateChanged.disconnect_all_slots();
  m_ModMoved.disconnect_all_slots();
}

void ModList::setProfile(Profile* profile)
{
  m_Profile = profile;
}

int ModList::rowCount(const QModelIndex& parent) const
{
  if (!parent.isValid()) {
    return ModInfo::getNumMods();
  } else {
    return 0;
  }
}

bool ModList::hasChildren(const QModelIndex& parent) const
{
  if (!parent.isValid()) {
    return ModInfo::getNumMods() > 0;
  } else {
    return false;
  }
}

int ModList::columnCount(const QModelIndex&) const
{
  return COL_LASTCOLUMN + 1;
}

QString ModList::getDisplayName(ModInfo::Ptr info) const
{
  QString name = info->name();
  if (info->isSeparator()) {
    name = name.replace("_separator", "");
  }
  return name;
}

QString ModList::makeInternalName(ModInfo::Ptr info, QString name) const
{
  if (info->isSeparator()) {
    name += "_separator";
  }
  return name;
}

QString ModList::getFlagText(ModInfo::EFlag flag, ModInfo::Ptr modInfo) const
{
  switch (flag) {
  case ModInfo::FLAG_BACKUP:
    return tr("Backup");
  case ModInfo::FLAG_SEPARATOR:
    return tr("Separator");
  case ModInfo::FLAG_INVALID:
    return tr("No valid game data");
  case ModInfo::FLAG_NOTENDORSED:
    return tr("Not endorsed yet");
  case ModInfo::FLAG_NOTES: {
    QStringList output;
    if (!modInfo->comments().isEmpty())
      output << QString("<i>%1</i>").arg(modInfo->comments());
    if (!modInfo->notes().isEmpty())
      output << QString("<i>%1</i>").arg(modInfo->notes());
    return output.join("");
  }
  case ModInfo::FLAG_ALTERNATE_GAME:
    return tr("This mod is for a different<br> game, "
              "make sure it's compatible or it could cause crashes.");
  case ModInfo::FLAG_TRACKED:
    return tr("Mod is being tracked on the website");
  case ModInfo::FLAG_HIDDEN_FILES:
    return tr("Contains hidden files");
  default:
    return "";
  }
}

QString ModList::getConflictFlagText(ModInfo::EConflictFlag flag,
                                     ModInfo::Ptr modInfo) const
{
  switch (flag) {
  case ModInfo::FLAG_CONFLICT_OVERWRITE:
    return tr("Overwrites loose files");
  case ModInfo::FLAG_CONFLICT_OVERWRITTEN:
    return tr("Overwritten loose files");
  case ModInfo::FLAG_CONFLICT_MIXED:
    return tr("Loose files Overwrites & Overwritten");
  case ModInfo::FLAG_CONFLICT_REDUNDANT:
    return tr("Redundant");
  case ModInfo::FLAG_ARCHIVE_LOOSE_CONFLICT_OVERWRITE:
    return tr("Overwrites an archive with loose files");
  case ModInfo::FLAG_ARCHIVE_LOOSE_CONFLICT_OVERWRITTEN:
    return tr("Archive is overwritten by loose files");
  case ModInfo::FLAG_ARCHIVE_CONFLICT_OVERWRITE:
    return tr("Overwrites another archive file");
  case ModInfo::FLAG_ARCHIVE_CONFLICT_OVERWRITTEN:
    return tr("Overwritten by another archive file");
  case ModInfo::FLAG_ARCHIVE_CONFLICT_MIXED:
    return tr("Archive files overwrites & overwritten");
  default:
    return "";
  }
}

QVariant ModList::data(const QModelIndex& modelIndex, int role) const
{
  if (m_Profile == nullptr)
    return QVariant();
  if (!modelIndex.isValid())
    return QVariant();
  unsigned int modIndex = modelIndex.row();
  int column            = modelIndex.column();

  ModInfo::Ptr modInfo = ModInfo::getByIndex(modIndex);
  if ((role == Qt::DisplayRole) || (role == Qt::EditRole)) {
    if ((column == COL_FLAGS) || (column == COL_CONTENT) ||
        (column == COL_CONFLICTFLAGS)) {
      return QVariant();
    } else if (column == COL_NAME) {
      return getDisplayName(modInfo);
    } else if (column == COL_VERSION) {
      VersionInfo verInfo = modInfo->version();
      QString version     = verInfo.displayString();
      if (role != Qt::EditRole) {
        if (version.isEmpty() && modInfo->canBeUpdated()) {
          version = "?";
        }
      }
      return version;
    } else if (column == COL_PRIORITY) {
      if (modInfo->hasAutomaticPriority()) {
        return QVariant();  // hide priority for mods where it's fixed
      } else {
        return m_Profile->getModPriority(modIndex);
      }
    } else if (column == COL_MODID) {
      int modID = modInfo->nexusId();
      if (modID > 0) {
        return modID;
      } else {
        return QVariant();
      }
    } else if (column == COL_GAME) {
      if (m_PluginManager != nullptr) {
        for (auto game : m_PluginManager->plugins<IPluginGame>()) {
          if (game->gameShortName().compare(modInfo->gameName(), Qt::CaseInsensitive) ==
              0)
            return game->gameName();
        }
      }
      return modInfo->gameName();
    } else if (column == COL_CATEGORY) {
      if (modInfo->hasFlag(ModInfo::FLAG_FOREIGN)) {
        return tr("Non-MO");
      } else {
        int category = modInfo->primaryCategory();
        if (category != -1) {
          CategoryFactory& categoryFactory = CategoryFactory::instance();
          if (categoryFactory.categoryExists(category)) {
            try {
              int categoryIdx = categoryFactory.getCategoryIndex(category);
              return categoryFactory.getCategoryName(categoryIdx);
            } catch (const std::exception& e) {
              log::error("failed to retrieve category name: {}", e.what());
              return QString();
            }
          } else {
            log::warn("category {} doesn't exist (may have been removed)", category);
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
    } else if (column == COL_NOTES) {
      return modInfo->comments();
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
    } else if (column == COL_NOTES) {
      return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
    } else {
      return QVariant(Qt::AlignCenter | Qt::AlignVCenter);
    }
  } else if (role == GroupingRole) {
    if (column == COL_CATEGORY) {
      QVariantList categoryNames;
      std::set<int> categories         = modInfo->getCategories();
      CategoryFactory& categoryFactory = CategoryFactory::instance();
      for (auto iter = categories.begin(); iter != categories.end(); ++iter) {
        categoryNames.append(
            categoryFactory.getCategoryName(categoryFactory.getCategoryIndex(*iter)));
      }
      if (categoryNames.count() != 0) {
        return categoryNames;
      } else {
        return QVariant();
      }
    } else {
      return modInfo->nexusId();
    }
  } else if (role == IndexRole) {
    return modIndex;
  } else if (role == AggrRole) {
    switch (column) {
    case COL_MODID:
      return QtGroupingProxy::AGGR_FIRST;
    case COL_VERSION:
      return QtGroupingProxy::AGGR_MAX;
    case COL_CATEGORY:
      return QtGroupingProxy::AGGR_FIRST;
    case COL_PRIORITY:
      return QtGroupingProxy::AGGR_MIN;
    default:
      return QtGroupingProxy::AGGR_NONE;
    }
  } else if (role == GameNameRole) {
    return modInfo->gameName();
  } else if (role == PriorityRole) {
    return m_Profile->getModPriority(modIndex);
  } else if (role == Qt::FontRole) {
    QFont result;
    if (column == COL_NAME) {
      if (modInfo->isSeparator()) {
        result.setItalic(true);
        result.setBold(true);
      } else if (modInfo->getHighlight() & ModInfo::HIGHLIGHT_INVALID) {
        result.setItalic(true);
      }
    } else if (column == COL_CATEGORY && modInfo->isForeign()) {
      result.setItalic(true);
    } else if (column == COL_VERSION) {
      if (modInfo->updateAvailable() || modInfo->downgradeAvailable()) {
        result.setWeight(QFont::Bold);
      }
      if (modInfo->canBeUpdated()) {
        result.setItalic(true);
      }
    }
    return result;
  } else if (role == Qt::DecorationRole) {
    if (column == COL_VERSION) {
      if (modInfo->updateAvailable()) {
        return QIcon(":/MO/gui/update_available");
      } else if (modInfo->downgradeAvailable()) {
        return QIcon(":/MO/gui/warning");
      } else if (modInfo->version().scheme() == VersionInfo::SCHEME_DATE) {
        return QIcon(":/MO/gui/version_date");
      }
    }
    return QVariant();
  } else if (role == Qt::ForegroundRole) {
    if (column == COL_NAME) {
      int highlight = modInfo->getHighlight();
      if (highlight & ModInfo::HIGHLIGHT_IMPORTANT) {
        return QBrush(Qt::darkRed);
      } else if (highlight & ModInfo::HIGHLIGHT_INVALID) {
        return QBrush(Qt::darkGray);
      }
    } else if (column == COL_VERSION) {
      if (!modInfo->newestVersion().isValid()) {
        return QVariant();
      } else if (modInfo->updateAvailable() || modInfo->downgradeAvailable()) {
        return QBrush(Qt::red);
      } else {
        return QBrush(Qt::darkGreen);
      }
    }
    return QVariant();
  } else if (role == Qt::BackgroundRole || role == ScrollMarkRole) {
    if (column == COL_NOTES && modInfo->color().isValid()) {
      return modInfo->color();
    } else if (modInfo->isSeparator() && modInfo->color().isValid() &&
               (role != ScrollMarkRole ||
                Settings::instance().colors().colorSeparatorScrollbar())) {
      return modInfo->color();
    } else {
      return QVariant();
    }
  } else if (role == Qt::ToolTipRole) {
    if (column == COL_FLAGS) {
      QString result;

      for (ModInfo::EFlag flag : modInfo->getFlags()) {
        if (result.length() != 0)
          result += "<br>";
        result += getFlagText(flag, modInfo);
      }

      return result;
    } else if (column == COL_CONFLICTFLAGS) {
      QString result;

      for (ModInfo::EConflictFlag flag : modInfo->getConflictFlags()) {
        if (result.length() != 0)
          result += "<br>";
        result += getConflictFlagText(flag, modInfo);
      }

      return result;
    } else if (column == COL_NAME) {
      try {
        return modInfo->getDescription();
      } catch (const std::exception& e) {
        log::error("invalid mod description: {}", e.what());
        return QString();
      }
    } else if (column == COL_VERSION) {
      QString text = tr("installed version: \"%1\", newest version: \"%2\"")
                         .arg(modInfo->version().displayString(3))
                         .arg(modInfo->newestVersion().displayString(3));
      if (modInfo->downgradeAvailable()) {
        text +=
            "<br>" + tr("The newest version on Nexus seems to be older than the one "
                        "you have installed. This could either mean the version you "
                        "have has been withdrawn "
                        "(i.e. due to a bug) or the author uses a non-standard "
                        "versioning scheme and that newest version is actually newer. "
                        "Either way you may want to \"upgrade\".");
      }
      if (modInfo->getNexusFileStatus() == NexusInterface::FileStatus::OLD_VERSION) {
        text += "<br>" + tr("This file has been marked as \"Old\". There is most "
                            "likely an updated version of this file available.");
      } else if (modInfo->getNexusFileStatus() == NexusInterface::FileStatus::REMOVED ||
                 modInfo->getNexusFileStatus() ==
                     NexusInterface::FileStatus::ARCHIVED ||
                 modInfo->getNexusFileStatus() ==
                     NexusInterface::FileStatus::ARCHIVED_HIDDEN) {
        text +=
            "<br>" + tr("This file has been marked as \"Deleted\"! You may want to "
                        "check for an update or remove the nexus ID from this mod!");
      }
      if (modInfo->nexusId() > 0) {
        if (!modInfo->canBeUpdated()) {
          qint64 remains =
              QDateTime::currentDateTimeUtc().secsTo(modInfo->getExpires());
          qint64 minutes = remains / 60;
          qint64 seconds = remains % 60;
          QString remainsStr(
              tr("%1 minute(s) and %2 second(s)").arg(minutes).arg(seconds));
          text +=
              "<br>" + tr("This mod will be available to check in %2.").arg(remainsStr);
        }
      }
      return text;
    } else if (column == COL_CATEGORY) {
      const std::set<int>& categories = modInfo->getCategories();
      std::wostringstream categoryString;
      categoryString << ToWString(tr("Categories: <br>"));
      CategoryFactory& categoryFactory = CategoryFactory::instance();
      for (std::set<int>::const_iterator catIter = categories.begin();
           catIter != categories.end(); ++catIter) {
        if (catIter != categories.begin()) {
          categoryString << " , ";
        }
        try {
          categoryString << "<span style=\"white-space: nowrap;\"><i>"
                         << ToWString(categoryFactory.getCategoryName(
                                categoryFactory.getCategoryIndex(*catIter)))
                         << "</font></span>";
        } catch (const std::exception& e) {
          log::error("failed to generate tooltip: {}", e.what());
          return QString();
        }
      }

      return ToQString(categoryString.str());
    } else if (column == COL_NOTES) {
      return getFlagText(ModInfo::FLAG_NOTES, modInfo);
    } else {
      return QVariant();
    }
  } else {
    return QVariant();
  }
}

bool ModList::renameMod(int index, const QString& newName)
{
  QString nameFixed = newName;
  if (!fixDirectoryName(nameFixed) || nameFixed.isEmpty()) {
    MessageDialog::showMessage(tr("Invalid name"), nullptr);
    return false;
  }

  if (ModList::allMods().contains(nameFixed, Qt::CaseInsensitive) &&
      nameFixed.toLower() != ModInfo::getByIndex(index)->name().toLower()) {
    MessageDialog::showMessage(tr("Name is already in use by another mod"), nullptr);
    return false;
  }

  ModInfo::Ptr modInfo = ModInfo::getByIndex(index);
  QString oldName      = modInfo->name();
  if (nameFixed != oldName) {
    // before we rename, ensure there is no scheduled asynchronous to rewrite
    m_Profile->cancelModlistWrite();

    if (modInfo->setName(nameFixed)) {
      // Notice there is a good chance that setName() updated the modinfo indexes
      // the modRenamed() call will refresh the indexes in the current profile
      // and update the modlists in all profiles
      emit modRenamed(oldName, nameFixed);
    }

    // invalidate the currently displayed state of this list
    notifyChange(-1);
  }
  return true;
}

bool ModList::setData(const QModelIndex& index, const QVariant& value, int role)
{
  if (m_Profile == nullptr)
    return false;

  if (static_cast<unsigned int>(index.row()) >= ModInfo::getNumMods()) {
    return false;
  }

  int modID = index.row();

  ModInfo::Ptr info            = ModInfo::getByIndex(modID);
  IModList::ModStates oldState = state(modID);

  bool result = false;

  emit aboutToChangeData();

  if (role == Qt::CheckStateRole) {
    bool enabled = value.toInt() == Qt::Checked;
    if (m_Profile->modEnabled(modID) != enabled) {
      m_Profile->setModEnabled(modID, enabled);
      m_Modified = true;
      m_LastCheck.restart();
      emit tutorialModlistUpdate();
    }
    result = true;
    emit dataChanged(index, index);
  } else if (role == Qt::EditRole) {
    switch (index.column()) {
    case COL_NAME: {
      result = renameMod(modID, makeInternalName(info, value.toString()));
    } break;
    case COL_PRIORITY: {
      bool ok         = false;
      int newPriority = value.toInt(&ok);
      if (ok) {
        changeModPriority(modID, newPriority);
        result = true;
      } else {
        result = false;
      }
    } break;
    case COL_MODID: {
      bool ok   = false;
      int newID = value.toInt(&ok);
      if (ok) {
        info->setNexusID(newID);
        emit tutorialModlistUpdate();
        result = true;
      } else {
        result = false;
      }
    } break;
    case COL_VERSION: {
      VersionInfo::VersionScheme scheme = info->version().scheme();
      VersionInfo version(value.toString(), scheme, true);
      if (version.isValid()) {
        info->setVersion(version);
        result = true;
      } else {
        result = false;
      }
    } break;
    case COL_NOTES: {
      info->setComments(value.toString());
      result = true;
    } break;
    default: {
      log::warn("edit on column \"{}\" not supported",
                getColumnName(index.column()).toUtf8().constData());
      result = false;
    } break;
    }
    if (result) {
      emit dataChanged(index, index);
    }
  }

  emit postDataChanged();
  return result;
}

QVariant ModList::headerData(int section, Qt::Orientation orientation, int role) const
{
  if (orientation == Qt::Horizontal) {
    if (role == Qt::DisplayRole) {
      return getColumnName(section);
    } else if (role == Qt::ToolTipRole) {
      return getColumnToolTip(section);
    } else if (role == Qt::TextAlignmentRole) {
      return QVariant(Qt::AlignCenter);
    } else if (role == MOBase::EnabledColumnRole) {
      if (section == COL_CONTENT) {
        return !m_Organizer->modDataContents().empty();
      } else {
        return true;
      }
    }
  }
  return QAbstractItemModel::headerData(section, orientation, role);
}

Qt::ItemFlags ModList::flags(const QModelIndex& modelIndex) const
{
  Qt::ItemFlags result = QAbstractItemModel::flags(modelIndex);
  if (modelIndex.internalId() < 0) {
    return Qt::ItemIsEnabled;
  }
  if (modelIndex.isValid()) {
    ModInfo::Ptr modInfo = ModInfo::getByIndex(modelIndex.row());
    if (!modInfo->hasAutomaticPriority()) {
      result |= Qt::ItemIsDragEnabled;
      result |= Qt::ItemIsUserCheckable;
      if ((modelIndex.column() == COL_PRIORITY) ||
          (modelIndex.column() == COL_VERSION) || (modelIndex.column() == COL_MODID)) {
        result |= Qt::ItemIsEditable;
      }
      if ((modelIndex.column() == COL_NAME || modelIndex.column() == COL_NOTES) &&
          !modInfo->isForeign()) {
        result |= Qt::ItemIsEditable;
      }
    }
    if (modInfo->isSeparator() || m_DropOnMod) {
      result |= Qt::ItemIsDropEnabled;
    }
  } else if (!m_DropOnMod) {
    result |= Qt::ItemIsDropEnabled;
  }

  return result;
}

QStringList ModList::mimeTypes() const
{
  QStringList result = QAbstractItemModel::mimeTypes();
  result.append("text/uri-list");
  return result;
}

QMimeData* ModList::mimeData(const QModelIndexList& indexes) const
{
  QMimeData* result = QAbstractItemModel::mimeData(indexes);
  result->setData("text/plain", ModListDropInfo::ModText);
  return result;
}

void ModList::changeModPriority(std::vector<int> sourceIndices, int newPriority)
{
  if (m_Profile == nullptr)
    return;

  emit layoutAboutToBeChanged();

  // sort the moving mods by ascending priorities
  std::sort(sourceIndices.begin(), sourceIndices.end(),
            [=](const int& LHS, const int& RHS) {
              return m_Profile->getModPriority(LHS) > m_Profile->getModPriority(RHS);
            });

  // move mods that are decreasing in priority
  for (const auto& index : sourceIndices) {
    int oldPriority = m_Profile->getModPriority(index);
    if (oldPriority > newPriority) {
      if (m_Profile->setModPriority(index, newPriority)) {
        m_ModMoved(ModInfo::getByIndex(index)->name(), oldPriority, newPriority);
      }
    }
  }

  // sort the moving mods by descending priorities
  std::sort(sourceIndices.begin(), sourceIndices.end(),
            [=](const int& LHS, const int& RHS) {
              return m_Profile->getModPriority(LHS) < m_Profile->getModPriority(RHS);
            });

  // if at least one mod is increasing in priority, the target index is
  // that of the row BELOW the dropped location, otherwise it's the one above
  for (const auto& index : sourceIndices) {
    int oldPriority = m_Profile->getModPriority(index);
    if (oldPriority < newPriority) {
      --newPriority;
      break;
    }
  }

  // move mods that are increasing in priority
  for (const auto& index : sourceIndices) {
    int oldPriority = m_Profile->getModPriority(index);
    if (oldPriority < newPriority) {
      if (m_Profile->setModPriority(index, newPriority)) {
        m_ModMoved(ModInfo::getByIndex(index)->name(), oldPriority, newPriority);
      }
    }
  }

  emit layoutChanged();

  QModelIndexList indices;
  for (auto& idx : sourceIndices) {
    indices.append(index(idx, 0, QModelIndex()));
  }

  emit modPrioritiesChanged(indices);
}

void ModList::changeModPriority(int sourceIndex, int newPriority)
{
  if (m_Profile == nullptr)
    return;
  emit layoutAboutToBeChanged();

  m_Profile->setModPriority(sourceIndex, newPriority);

  emit layoutChanged();
  emit modPrioritiesChanged({index(sourceIndex, 0)});
}

void ModList::setPluginManager(PluginManager* pluginContianer)
{
  m_PluginManager = pluginContianer;
}

bool ModList::modInfoAboutToChange(ModInfo::Ptr info)
{
  if (m_ChangeInfo.name.isEmpty()) {
    m_ChangeInfo.name  = info->name();
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
      m_ModStateChanged({{info->name(), newState}});
    }

    int row = ModInfo::getIndex(info->name());
    info->diskContentModified();
    emit aboutToChangeData();
    emit dataChanged(index(row, 0), index(row, columnCount()));
    emit postDataChanged();
  } else {
    log::error("modInfoChanged not called after modInfoAboutToChange");
  }
  m_ChangeInfo.name = QString();
}

void ModList::disconnectSlots()
{
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
    if (modInfo->endorsedState() == EndorsedState::ENDORSED_TRUE) {
      result |= IModList::STATE_ENDORSED;
    }
    if (modInfo->isValid()) {
      result |= IModList::STATE_VALID;
    }
    if (modInfo->isRegular()) {
      QSharedPointer<ModInfoRegular> modInfoRegular =
          modInfo.staticCast<ModInfoRegular>();
      if (modInfoRegular->isAlternate() && !modInfoRegular->isConverted())
        result |= IModList::STATE_ALTERNATE;
      if (!modInfo->isValid() && modInfoRegular->isValidated())
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

QString ModList::displayName(const QString& internalName) const
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

QStringList ModList::allModsByProfilePriority(MOBase::IProfile* profile) const
{
  Profile* mo2Profile = profile == nullptr ? m_Organizer->currentProfile()
                                           : static_cast<Profile*>(profile);

  QStringList res;
  for (auto& [priority, index] : mo2Profile->getAllIndexesByPriority()) {
    auto modInfo = ModInfo::getByIndex(index);
    if (!modInfo->isBackup() && !modInfo->isOverwrite()) {
      res.push_back(modInfo->internalName());
    }
  }
  return res;
}

MOBase::IModInterface* ModList::getMod(const QString& name) const
{
  unsigned int index = ModInfo::getIndex(name);
  return index == UINT_MAX ? nullptr : ModInfo::getByIndex(index).data();
}

bool ModList::removeMod(MOBase::IModInterface* mod)
{
  bool result = ModInfo::removeMod(ModInfo::getIndex(mod->name()));
  if (result) {
    notifyModRemoved(mod->name());
  }
  return result;
}

MOBase::IModInterface* ModList::renameMod(MOBase::IModInterface* mod,
                                          const QString& name)
{
  unsigned int index = ModInfo::getIndex(mod->name());
  if (index == UINT_MAX) {
    if (auto* p = dynamic_cast<ModInfo*>(mod)) {
      p->setName(name);
      return p;
    } else {
      return nullptr;
    }
  } else {
    if (renameMod(index, name)) {
      return ModInfo::getByName(name).get();
    } else {
      return nullptr;
    }
  }
}

IModList::ModStates ModList::state(const QString& name) const
{
  unsigned int modIndex = ModInfo::getIndex(name);

  return state(modIndex);
}

bool ModList::setActive(const QString& name, bool active)
{
  unsigned int modIndex = ModInfo::getIndex(name);
  if (modIndex == UINT_MAX) {
    log::debug("Trying to {} mod {} which does not exist.",
               active ? "enable" : "disable", name);
    return false;
  } else {
    m_Profile->setModEnabled(modIndex, active);
    return true;
  }
}

int ModList::setActive(const QStringList& names, bool active)
{

  // We only add indices for mods that exist (modIndex != UINT_MAX)
  // and that can be enabled / disabled.
  QList<unsigned int> indices;
  for (const auto& name : names) {
    auto modIndex = ModInfo::getIndex(name);
    if (modIndex != UINT_MAX) {
      indices.append(modIndex);
    } else {
      log::debug("Trying to {} mod {} which does not exist.",
                 active ? "enable" : "disable", name);
    }
  }

  if (active) {
    m_Profile->setModsEnabled(indices, {});
  } else {
    m_Profile->setModsEnabled({}, indices);
  }

  return indices.size();
}

int ModList::priority(const QString& name) const
{
  unsigned int modIndex = ModInfo::getIndex(name);
  if (modIndex == UINT_MAX) {
    return -1;
  } else {
    return m_Profile->getModPriority(modIndex);
  }
}

bool ModList::setPriority(const QString& name, int newPriority)
{
  unsigned int index = ModInfo::getIndex(name);
  if (index == UINT_MAX) {
    return false;
  } else {
    if (m_Profile->setModPriority(index, newPriority)) {
      notifyChange(index);
    }
    return true;
  }
}

boost::signals2::connection
ModList::onModInstalled(const std::function<void(MOBase::IModInterface*)>& func)
{
  return m_ModInstalled.connect(func);
}

boost::signals2::connection
ModList::onModRemoved(const std::function<void(QString const&)>& func)
{
  return m_ModRemoved.connect(func);
}

boost::signals2::connection ModList::onModStateChanged(
    const std::function<void(const std::map<QString, IModList::ModStates>&)>& func)
{
  return m_ModStateChanged.connect(func);
}

void ModList::notifyModInstalled(MOBase::IModInterface* mod) const
{
  m_ModInstalled(mod);
}

void ModList::notifyModRemoved(QString const& modName) const
{
  m_ModRemoved(modName);
}

void ModList::notifyModStateChanged(QList<unsigned int> modIndices) const
{
  QModelIndexList indices;
  std::map<QString, IModList::ModStates> mods;
  for (auto modIndex : modIndices) {
    indices.append(index(modIndex, 0));
    ModInfo::Ptr modInfo = ModInfo::getByIndex(modIndex);
    mods.emplace(modInfo->name(), state(modIndex));
  }

  emit modStatesChanged(indices);
  m_ModStateChanged(mods);
}

boost::signals2::connection
ModList::onModMoved(const std::function<void(const QString&, int, int)>& func)
{
  return m_ModMoved.connect(func);
}

int ModList::dropPriority(int row, const QModelIndex& parent) const
{
  if (row == -1) {
    row = parent.row();
  }

  if ((row < 0) || (static_cast<unsigned int>(row) >= ModInfo::getNumMods())) {
    return -1;
  }

  int newPriority = 0;
  {
    if (row < 0 || row >= rowCount()) {
      newPriority = Profile::MaximumPriority;
    } else {
      newPriority = m_Profile->getModPriority(row);
    }
  }

  return newPriority;
}

bool ModList::dropLocalFiles(const ModListDropInfo& dropInfo, int row,
                             const QModelIndex& parent)
{
  if (row == -1) {
    row = parent.row();
  }
  ModInfo::Ptr modInfo = ModInfo::getByIndex(row);
  QDir modDir          = QDir(modInfo->absolutePath());

  QStringList sourceList;
  QStringList targetList;
  QList<QPair<QString, QString>> relativePathList;

  for (auto localUrl : dropInfo.localUrls()) {

    QFileInfo sourceInfo(localUrl.url.toLocalFile());
    QString sourceFile = sourceInfo.canonicalFilePath();

    QFileInfo targetInfo(modDir.absoluteFilePath(localUrl.relativePath));
    sourceList << sourceFile;
    targetList << targetInfo.absoluteFilePath();
    relativePathList << QPair<QString, QString>(localUrl.relativePath,
                                                localUrl.originName);
  }

  if (sourceList.count()) {
    if (!shellMove(sourceList, targetList)) {
      log::debug("Failed to move file (error {})", ::GetLastError());
      return false;
    }
  }

  for (auto iter : relativePathList) {
    emit fileMoved(iter.first, iter.second, modInfo->name());
  }

  if (!modInfo->isValid()) {
    modInfo->diskContentModified();
  }

  return true;
}

void ModList::onDragEnter(const QMimeData* mimeData)
{
  m_DropOnMod = ModListDropInfo(mimeData, *m_Organizer).isLocalFileDrop();
}

bool ModList::canDropMimeData(const QMimeData* mimeData, Qt::DropAction action, int row,
                              int column, const QModelIndex& parent) const
{
  if (action == Qt::IgnoreAction) {
    return false;
  }

  ModListDropInfo dropInfo(mimeData, *m_Organizer);

  if (dropInfo.isLocalFileDrop()) {
    if (row == -1 && parent.isValid()) {
      ModInfo::Ptr modInfo = ModInfo::getByIndex(parent.row());
      return modInfo->isRegular() && !modInfo->isSeparator();
    }
  } else if (dropInfo.isValid()) {
    // drop on item
    if (row == -1 && parent.isValid()) {
      return true;
    } else if (hasIndex(row, column, parent)) {
      ModInfo::Ptr modInfo = ModInfo::getByIndex(row);
      return !modInfo->isBackup() && (modInfo->isSeparator() || !parent.isValid());
    } else {
      return true;
    }
  }

  return false;
}

bool ModList::dropMimeData(const QMimeData* mimeData, Qt::DropAction action, int row,
                           int, const QModelIndex& parent)
{
  if (action == Qt::IgnoreAction) {
    return true;
  }

  ModListDropInfo dropInfo(mimeData, *m_Organizer);

  if (!m_Profile || !dropInfo.isValid()) {
    return false;
  }

  int dropPriority = this->dropPriority(row, parent);
  if (dropPriority == -1) {
    return false;
  }

  if (dropInfo.isLocalFileDrop()) {
    return dropLocalFiles(dropInfo, row, parent);
  } else {
    if (dropInfo.isModDrop()) {
      changeModPriority(dropInfo.rows(), dropPriority);
    } else if (dropInfo.isDownloadDrop()) {
      emit downloadArchiveDropped(dropInfo.download(), dropPriority);
    } else if (dropInfo.isExternalArchiveDrop()) {
      emit externalArchiveDropped(dropInfo.externalUrl(), dropPriority);
    } else if (dropInfo.isExternalFolderDrop()) {
      emit externalFolderDropped(dropInfo.externalUrl(), dropPriority);
    } else {
      return false;
    }
  }
  return false;
}

void ModList::removeRowForce(int row, const QModelIndex& parent)
{
  if (static_cast<unsigned int>(row) >= ModInfo::getNumMods()) {
    return;
  }
  if (m_Profile == nullptr)
    return;

  ModInfo::Ptr modInfo = ModInfo::getByIndex(row);

  bool wasEnabled = m_Profile->modEnabled(row);

  m_Profile->setModEnabled(row, false);

  m_Profile->cancelModlistWrite();
  beginRemoveRows(parent, row, row);
  ModInfo::removeMod(row);
  m_Profile->refreshModStatus();  // removes the mod from the status list
  endRemoveRows();
  m_Profile->writeModlist();  // this ensures the modified list gets written back before
                              // new mods can be installed

  notifyModRemoved(modInfo->name());

  if (wasEnabled) {
    emit removeOrigin(modInfo->name());
  }
  if (!modInfo->isBackup()) {
    emit modUninstalled(modInfo->installationFile());
  }
}

bool ModList::removeRows(int row, int count, const QModelIndex& parent)
{
  if (static_cast<unsigned int>(row) >= ModInfo::getNumMods()) {
    return false;
  }
  if (m_Profile == nullptr) {
    return false;
  }

  bool success = false;

  if (count == 1) {
    ModInfo::Ptr modInfo = ModInfo::getByIndex(row);
    if (modInfo->isOverwrite() && QDir(modInfo->absolutePath()).count() > 2) {
      emit clearOverwrite();
      success = true;
    }
  }

  for (int i = 0; i < count; ++i) {
    ModInfo::Ptr modInfo = ModInfo::getByIndex(row + i);
    if (!modInfo->isRegular()) {
      continue;
    }

    success = true;

    QMessageBox confirmBox(
        QMessageBox::Question, tr("Confirm"),
        tr("Are you sure you want to remove \"%1\"?").arg(getDisplayName(modInfo)),
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
  // this function can emit dataChanged(), which can eventually recurse back
  // here; for example:
  //
  // - a filter is active in the mod list, such as "no categories"
  // - mods are selected and a category is set on them
  // - these mods get updated here and disappear from the list because they're
  //   not in "no categories" anymore
  // - dataChanged() is emitted
  // - it's picked up in MainWindow::modlistSelectionsChanged() because the
  //   selected mods are gone
  // - it calls setOverwriteMarkers(), which calls notifyChange() again and
  //   ends up here
  // - dataChanged() is emitted again
  //
  // at this point, MO crashes because dataChanged() is not reentrant: it's in
  // the middle of modifying internal data and crashes when trying to change an
  // internal vector
  //
  // long story short, this prevents reentrancy
  if (m_InNotifyChange) {
    return;
  }

  m_InNotifyChange = true;
  Guard g([&] {
    m_InNotifyChange = false;
  });

  if (rowStart < 0) {
    beginResetModel();
    endResetModel();
  } else {
    if (rowEnd == -1) {
      rowEnd = rowStart;
    }
    emit dataChanged(this->index(rowStart, 0),
                     this->index(rowEnd, this->columnCount() - 1));
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

QMap<int, QVariant> ModList::itemData(const QModelIndex& index) const
{
  QMap<int, QVariant> result = QAbstractItemModel::itemData(index);
  for (int role = Qt::UserRole; role < ModUserRole; ++role) {
    result[role] = data(index, role);
  }
  return result;
}

QString ModList::getColumnName(int column)
{
  switch (column) {
  case COL_CONFLICTFLAGS:
    return tr("Conflicts");
  case COL_FLAGS:
    return tr("Flags");
  case COL_CONTENT:
    return tr("Content");
  case COL_NAME:
    return tr("Mod Name");
  case COL_VERSION:
    return tr("Version");
  case COL_PRIORITY:
    return tr("Priority");
  case COL_CATEGORY:
    return tr("Category");
  case COL_GAME:
    return tr("Source Game");
  case COL_MODID:
    return tr("Nexus ID");
  case COL_INSTALLTIME:
    return tr("Installation");
  case COL_NOTES:
    return tr("Notes");
  default:
    return tr("unknown");
  }
}

QString ModList::getColumnToolTip(int column) const
{
  switch (column) {
  case COL_NAME:
    return tr("Name of your mods");
  case COL_VERSION:
    return tr("Version of the mod (if available)");
  case COL_PRIORITY:
    return tr("Installation priority of your mod. The higher, the more \"important\" "
              "it is and thus "
              "overwrites files from mods with lower priority.");
  case COL_CATEGORY:
    return tr("Primary category of the mod.");
  case COL_GAME:
    return tr("The source game which was the origin of this mod.");
  case COL_MODID:
    return tr("Id of the mod as used on Nexus.");
  case COL_CONFLICTFLAGS:
    return tr("Indicators of file conflicts between mods.");
  case COL_FLAGS:
    return tr("Emblems to highlight things that might require attention.");
  case COL_CONTENT: {
    auto& contents = m_Organizer->modDataContents();
    if (m_Organizer->modDataContents().empty()) {
      return QString();
    }
    QString result =
        tr("Depicts the content of the mod:") + "<br>" + "<table cellspacing=7>";
    m_Organizer->modDataContents().forEachContent([&result](auto const& content) {
      result += QString("<tr><td><img src=\"%1\" width=32/></td><td>%2</td></tr>")
                    .arg(content.icon())
                    .arg(content.name());
    });
    return result + "</table>";
  };
  case COL_INSTALLTIME:
    return tr("Time this mod was installed");
  case COL_NOTES:
    return tr("User notes about the mod");
  default:
    return tr("unknown");
  }
}

void ModList::shiftModsPriority(const QModelIndexList& indices, int offset)
{
  // retrieve the mod index and sort them by priority to avoid issue
  // when moving them
  std::vector<int> allIndex;
  for (auto& idx : indices) {
    auto index = idx.data(IndexRole).toInt();
    allIndex.push_back(index);
  }
  std::sort(allIndex.begin(), allIndex.end(), [=](int lhs, int rhs) {
    bool cmp = m_Profile->getModPriority(lhs) < m_Profile->getModPriority(rhs);
    return offset > 0 ? !cmp : cmp;
  });

  emit layoutAboutToBeChanged();

  std::vector<int> notify;
  for (auto index : allIndex) {
    int newPriority = m_Profile->getModPriority(index) + offset;
    if (m_Profile->setModPriority(index, newPriority)) {
      notify.push_back(index);
    }
  }

  emit layoutChanged();

  for (auto index : notify) {
    notifyChange(index);
  }

  emit modPrioritiesChanged(indices);
}

void ModList::changeModsPriority(const QModelIndexList& indices, int priority)
{
  if (indices.isEmpty()) {
    return;
  }

  std::vector<int> allIndex;
  for (auto& idx : indices) {
    auto index = idx.data(IndexRole).toInt();
    allIndex.push_back(index);
  }

  if (allIndex.size() == 1) {
    changeModPriority(allIndex[0], priority);
  } else {
    changeModPriority(allIndex, priority);
  }
}

bool ModList::toggleState(const QModelIndexList& indices)
{
  emit aboutToChangeData();

  QList<unsigned int> modsToEnable;
  QList<unsigned int> modsToDisable;
  for (auto index : indices) {
    auto idx = index.data(IndexRole).toInt();
    if (m_Profile->modEnabled(idx)) {
      modsToDisable.append(idx);
    } else {
      modsToEnable.append(idx);
    }
  }

  m_Profile->setModsEnabled(modsToEnable, modsToDisable);

  emit tutorialModlistUpdate();

  m_Modified = true;
  m_LastCheck.restart();

  emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));

  emit postDataChanged();

  return true;
}

void ModList::setActive(const QModelIndexList& indices, bool active)
{
  QList<unsigned int> mods;
  for (auto& index : indices) {
    mods.append(index.data(IndexRole).toInt());
  }

  if (active) {
    m_Profile->setModsEnabled(mods, {});
  } else {
    m_Profile->setModsEnabled({}, mods);
  }
}

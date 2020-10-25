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

#include "widgetutility.h"
#include "messagedialog.h"
#include "qtgroupingproxy.h"
#include "viewmarkingscrollbar.h"
#include "modlistsortproxy.h"
#include "pluginlist.h"
#include "settings.h"
#include "organizercore.h"
#include "modinforegular.h"
#include "shared/directoryentry.h"
#include "shared/fileentry.h"
#include "shared/filesorigin.h"

#include "shared/appconfig.h"
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
#include <QFontDatabase>

#include <sstream>
#include <stdexcept>
#include <algorithm>


using namespace MOBase;


ModList::ModList(PluginContainer *pluginContainer, OrganizerCore *organizer)
  : QAbstractItemModel(organizer)
  , m_Organizer(organizer)
  , m_Profile(nullptr)
  , m_NexusInterface(nullptr)
  , m_Modified(false)
  , m_InNotifyChange(false)
  , m_FontMetrics(QFont())
  , m_DropOnItems(false)
  , m_PluginContainer(pluginContainer)
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
    case ModInfo::FLAG_SEPARATOR: return tr("Separator");
    case ModInfo::FLAG_INVALID: return tr("No valid game data");
    case ModInfo::FLAG_NOTENDORSED: return tr("Not endorsed yet");
    case ModInfo::FLAG_NOTES: {
      QStringList output;
      if (!modInfo->comments().isEmpty())
        output << QString("<i>%1</i>").arg(modInfo->comments());
      if (!modInfo->notes().isEmpty())
        output << QString("<i>%1</i>").arg(modInfo->notes());
      return output.join("");
    }
    case ModInfo::FLAG_ALTERNATE_GAME: return tr("This mod is for a different<br> game, "
      "make sure it's compatible or it could cause crashes.");
    case ModInfo::FLAG_TRACKED: return tr("Mod is being tracked on the website");
    case ModInfo::FLAG_HIDDEN_FILES: return tr("Contains hidden files");
    default: return "";
  }
}


QString ModList::getConflictFlagText(ModInfo::EConflictFlag flag, ModInfo::Ptr modInfo) const
{
  switch (flag) {
  case ModInfo::FLAG_CONFLICT_OVERWRITE: return tr("Overwrites loose files");
  case ModInfo::FLAG_CONFLICT_OVERWRITTEN: return tr("Overwritten loose files");
  case ModInfo::FLAG_CONFLICT_MIXED: return tr("Loose files Overwrites & Overwritten");
  case ModInfo::FLAG_CONFLICT_REDUNDANT: return tr("Redundant");
  case ModInfo::FLAG_ARCHIVE_LOOSE_CONFLICT_OVERWRITE: return tr("Overwrites an archive with loose files");
  case ModInfo::FLAG_ARCHIVE_LOOSE_CONFLICT_OVERWRITTEN: return tr("Archive is overwritten by loose files");
  case ModInfo::FLAG_ARCHIVE_CONFLICT_OVERWRITE: return tr("Overwrites another archive file");
  case ModInfo::FLAG_ARCHIVE_CONFLICT_OVERWRITTEN: return tr("Overwritten by another archive file");
  case ModInfo::FLAG_ARCHIVE_CONFLICT_MIXED: return tr("Archive files overwrites & overwritten");
  default: return "";
  }
}


QVariantList ModList::contentsToIcons(const std::set<int> &contents) const
{
  QVariantList result;
  m_Organizer->modDataContents().forEachContentInOrOut(
    contents,
    [&result](auto const& content) { result.append(content.icon()); },
    [&result](auto const&) { result.append(QString()); });
  return result;
}

QString ModList::contentsToToolTip(const std::set<int> &contents) const
{
  QString result("<table cellspacing=7>");
  m_Organizer->modDataContents().forEachContentIn(contents, [&result](auto const& content) {
    result.append(QString("<tr><td><img src=\"%1\" width=32/></td>"
      "<td valign=\"middle\">%2</td></tr>")
      .arg(content.icon()).arg(content.name()));
    });
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
        || (column == COL_CONTENT)
        || (column == COL_CONFLICTFLAGS)) {
      return QVariant();
    } else if (column == COL_NAME) {
      auto flags = modInfo->getFlags();
      if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_SEPARATOR) != flags.end())
      {
        return modInfo->name().replace("_separator", "");
      }
      else
        return modInfo->name();
    } else if (column == COL_VERSION) {
      VersionInfo verInfo = modInfo->version();
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
      int modID = modInfo->nexusId();
      if (modID > 0) {
        return modID;
      }
      else {
        return QVariant();
      }
    } else if (column == COL_GAME) {
      if (m_PluginContainer != nullptr) {
        for (auto game : m_PluginContainer->plugins<IPluginGame>()) {
          if (game->gameShortName().compare(modInfo->gameName(), Qt::CaseInsensitive) == 0)
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
          CategoryFactory &categoryFactory = CategoryFactory::instance();
          if (categoryFactory.categoryExists(category)) {
            try {
              int categoryIdx = categoryFactory.getCategoryIndex(category);
              return categoryFactory.getCategoryName(categoryIdx);
            } catch (const std::exception &e) {
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
    auto flags = modInfo->getFlags();
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
      return modInfo->nexusId();
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
  } else if (role == Qt::UserRole + 4) {
    return modInfo->gameName();
  } else if (role == Qt::FontRole) {
    QFont result;
    auto flags = modInfo->getFlags();
    if (column == COL_NAME) {
      if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_SEPARATOR) != flags.end())
      {
        //result.setCapitalization(QFont::AllUppercase);
        result.setItalic(true);
        //result.setUnderline(true);
        result.setBold(true);
      } else if (modInfo->getHighlight() & ModInfo::HIGHLIGHT_INVALID) {
        result.setItalic(true);
      }
    } else if ((column == COL_CATEGORY) && (modInfo->hasFlag(ModInfo::FLAG_FOREIGN))) {
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
    if ((modInfo->hasFlag(ModInfo::FLAG_SEPARATOR) || (column == COL_NOTES)) && modInfo->color().isValid()) {
      return ColorSettings::idealTextColor(modInfo->color());
    } else if (column == COL_NAME) {
      int highlight = modInfo->getHighlight();
      if (highlight & ModInfo::HIGHLIGHT_IMPORTANT)
        return QBrush(Qt::darkRed);
      else if (highlight & ModInfo::HIGHLIGHT_INVALID)
        return QBrush(Qt::darkGray);
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
  } else if ((role == Qt::BackgroundRole)
             || (role == ViewMarkingScrollBar::DEFAULT_ROLE)) {
    bool overwrite = m_Overwrite.find(modIndex) != m_Overwrite.end();
    bool archiveOverwrite = m_ArchiveOverwrite.find(modIndex) != m_ArchiveOverwrite.end();
    bool archiveLooseOverwrite = m_ArchiveLooseOverwrite.find(modIndex) != m_ArchiveLooseOverwrite.end();
    bool overwritten = m_Overwritten.find(modIndex) != m_Overwritten.end();
    bool archiveOverwritten = m_ArchiveOverwritten.find(modIndex) != m_ArchiveOverwritten.end();
    bool archiveLooseOverwritten = m_ArchiveLooseOverwritten.find(modIndex) != m_ArchiveLooseOverwritten.end();
    if (column == COL_NOTES && modInfo->color().isValid()) {
      return modInfo->color();
    } else if (modInfo->getHighlight() & ModInfo::HIGHLIGHT_PLUGIN) {
      return Settings::instance().colors().modlistContainsPlugin();
    } else if (overwritten || archiveLooseOverwritten) {
      return Settings::instance().colors().modlistOverwritingLoose();
    } else if (overwrite || archiveLooseOverwrite) {
      return Settings::instance().colors().modlistOverwrittenLoose();
    } else if (archiveOverwritten) {
      return Settings::instance().colors().modlistOverwritingArchive();
    } else if (archiveOverwrite) {
      return Settings::instance().colors().modlistOverwrittenArchive();
    } else if (modInfo->hasFlag(ModInfo::FLAG_SEPARATOR)
               && modInfo->color().isValid()
               && ((role != ViewMarkingScrollBar::DEFAULT_ROLE)
                    || Settings::instance().colors().colorSeparatorScrollbar())) {
      return modInfo->color();
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
    } else if (column == COL_CONFLICTFLAGS) {
      QString result;

      for (ModInfo::EConflictFlag flag : modInfo->getConflictFlags()) {
        if (result.length() != 0) result += "<br>";
        result += getConflictFlagText(flag, modInfo);
      }

      return result;
    } else if (column == COL_CONTENT) {
      return contentsToToolTip(modInfo->getContents());
    } else if (column == COL_NAME) {
      try {
        return modInfo->getDescription();
      } catch (const std::exception &e) {
        log::error("invalid mod description: {}", e.what());
        return QString();
      }
    } else if (column == COL_VERSION) {
      QString text = tr("installed version: \"%1\", newest version: \"%2\"").arg(modInfo->version().displayString(3)).arg(modInfo->newestVersion().displayString(3));
      if (modInfo->downgradeAvailable()) {
        text += "<br>" + tr("The newest version on Nexus seems to be older than the one you have installed. This could either mean the version you have has been withdrawn "
                          "(i.e. due to a bug) or the author uses a non-standard versioning scheme and that newest version is actually newer. "
                          "Either way you may want to \"upgrade\".");
      }
      if (modInfo->getNexusFileStatus() == 4) {
        text += "<br>" + tr("This file has been marked as \"Old\". There is most likely an updated version of this file available.");
      } else if (modInfo->getNexusFileStatus() == 6) {
        text += "<br>" + tr("This file has been marked as \"Deleted\"! You may want to check for an update or remove the nexus ID from this mod!");
      }
      if (modInfo->nexusId() > 0) {
        if (!modInfo->canBeUpdated()) {
          qint64 remains = QDateTime::currentDateTimeUtc().secsTo(modInfo->getExpires());
          qint64 minutes = remains / 60;
          qint64 seconds = remains % 60;
          QString remainsStr(tr("%1 minute(s) and %2 second(s)").arg(minutes).arg(seconds));
          text += "<br>" + tr("This mod will be available to check in %2.")
            .arg(remainsStr);
        }
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


bool ModList::renameMod(int index, const QString &newName)
{
  QString nameFixed = newName;
  if (!fixDirectoryName(nameFixed) || nameFixed.isEmpty()) {
    MessageDialog::showMessage(tr("Invalid name"), nullptr);
    return false;
  }

  if (ModList::allMods().contains(nameFixed, Qt::CaseInsensitive) && nameFixed.toLower()!=ModInfo::getByIndex(index)->name().toLower() ) {
	  MessageDialog::showMessage(tr("Name is already in use by another mod"), nullptr);
	  return false;
  }

  ModInfo::Ptr modInfo = ModInfo::getByIndex(index);
  QString oldName = modInfo->name();
  if (nameFixed != oldName) {
    // before we rename, ensure there is no scheduled asynchronous to rewrite
    m_Profile->cancelModlistWrite();


    if (modInfo->setName(nameFixed))
      // Notice there is a good chance that setName() updated the modinfo indexes
      // the modRenamed() call will refresh the indexes in the current profile
      // and update the modlists in all profiles
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
      emit modlistChanged(index, role);
      emit tutorialModlistUpdate();
    }
    result = true;
    emit dataChanged(index, index);
  } else if (role == Qt::EditRole) {
    switch (index.column()) {
      case COL_NAME: {
        auto flags = info->getFlags();
        if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_SEPARATOR) != flags.end())
        {
          result = renameMod(modID, value.toString() + "_separator");
        }
        else
          result = renameMod(modID, value.toString());
      } break;
      case COL_PRIORITY: {
        bool ok = false;
        int newPriority = value.toInt(&ok);
        if (ok && newPriority < 0) {
          newPriority = 0;
        }
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
          emit modlistChanged(index, role);
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
        log::warn(
          "edit on column \"{}\" not supported",
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
    } else if (role == MOBase::EnabledColumnRole) {
      if (section == COL_CONTENT) {
        return !m_Organizer->modDataContents().empty();
      }
      else {
        return true;
      }
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
    std::vector<ModInfo::EFlag> flags = modInfo->getFlags();
    if (modInfo->getFixedPriority() == INT_MIN) {
      result |= Qt::ItemIsDragEnabled;
      result |= Qt::ItemIsUserCheckable;
      if ((modelIndex.column() == COL_PRIORITY) ||
          (modelIndex.column() == COL_VERSION) ||
          (modelIndex.column() == COL_MODID)) {
        result |= Qt::ItemIsEditable;
      }
      if (((modelIndex.column() == COL_NAME) ||
           (modelIndex.column() == COL_NOTES))
          && (std::find(flags.begin(), flags.end(), ModInfo::FLAG_FOREIGN) == flags.end())) {
        result |= Qt::ItemIsEditable;
      }
    }
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

  // sort the moving mods by ascending priorities
  std::sort(sourceIndices.begin(), sourceIndices.end(),
    [profile](const int &LHS, const int &RHS) {
    return profile->getModPriority(LHS) > profile->getModPriority(RHS);
  });

  // move mods that are decreasing in priority
  for (std::vector<int>::const_iterator iter = sourceIndices.begin();
       iter != sourceIndices.end(); ++iter) {
    int oldPriority = profile->getModPriority(*iter);
    if (oldPriority > newPriority) {
      profile->setModPriority(*iter, newPriority);
      m_ModMoved(ModInfo::getByIndex(*iter)->name(), oldPriority, newPriority);
    }
  }

  // sort the moving mods by descending priorities
  std::sort(sourceIndices.begin(), sourceIndices.end(),
    [profile](const int &LHS, const int &RHS) {
    return profile->getModPriority(LHS) < profile->getModPriority(RHS);
  });

  // if at least one mod is increasing in priority, the target index is
  // that of the row BELOW the dropped location, otherwise it's the one above
  for (std::vector<int>::const_iterator iter = sourceIndices.begin();
    iter != sourceIndices.end(); ++iter) {
    int oldPriority = profile->getModPriority(*iter);
    if (oldPriority < newPriority) {
      --newPriority;
      break;
    }
  }

  // move mods that are increasing in priority
  for (std::vector<int>::const_iterator iter = sourceIndices.begin();
    iter != sourceIndices.end(); ++iter) {
    int oldPriority = profile->getModPriority(*iter);
    if (oldPriority < newPriority) {
      profile->setModPriority(*iter, newPriority);
      m_ModMoved(ModInfo::getByIndex(*iter)->name(), oldPriority, newPriority);
    }
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

void ModList::setArchiveOverwriteMarkers(const std::set<unsigned int> &overwrite, const std::set<unsigned int> &overwritten)
{
  m_ArchiveOverwrite = overwrite;
  m_ArchiveOverwritten = overwritten;
  notifyChange(0, rowCount() - 1);
}

void ModList::setArchiveLooseOverwriteMarkers(const std::set<unsigned int> &overwrite, const std::set<unsigned int> &overwritten)
{
  m_ArchiveLooseOverwrite = overwrite;
  m_ArchiveLooseOverwritten = overwritten;
  notifyChange(0, rowCount() - 1);
}

void ModList::setPluginContainer(PluginContainer *pluginContianer)
{
  m_PluginContainer = pluginContianer;
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
      m_ModStateChanged({ {info->name(), newState} });
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

void ModList::disconnectSlots() {
  m_ModMoved.disconnect_all_slots();
  m_ModStateChanged.disconnect_all_slots();
}

int ModList::timeElapsedSinceLastChecked() const
{
  return m_LastCheck.elapsed();
}

void ModList::highlightMods(const QItemSelectionModel *selection, const MOShared::DirectoryEntry &directoryEntry)
{
  for (unsigned int i = 0; i < ModInfo::getNumMods(); ++i) {
      ModInfo::getByIndex(i)->setPluginSelected(false);
  }
  for (QModelIndex idx : selection->selectedRows(PluginList::COL_NAME)) {
    QString modName = idx.data().toString();

    const MOShared::FileEntryPtr fileEntry = directoryEntry.findFile(modName.toStdWString());
    if (fileEntry.get() != nullptr) {
      bool archive = false;
      std::vector<std::pair<int, std::pair<std::wstring, int>>> origins;
      {
        std::vector<std::pair<int, std::pair<std::wstring, int>>> alternatives = fileEntry->getAlternatives();
        origins.insert(origins.end(), std::pair<int, std::pair<std::wstring, int>>(fileEntry->getOrigin(archive), fileEntry->getArchive()));
      }
      for (auto originInfo : origins) {
        MOShared::FilesOrigin& origin = directoryEntry.getOriginByID(originInfo.first);
        for (unsigned int i = 0; i < ModInfo::getNumMods(); ++i) {
          if (ModInfo::getByIndex(i)->internalName() == QString::fromStdWString(origin.getName())) {
            ModInfo::getByIndex(i)->setPluginSelected(true);
            break;
          }
        }
      }
    }
  }
  notifyChange(0, rowCount() - 1);
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
      QSharedPointer<ModInfoRegular> modInfoRegular = modInfo.staticCast<ModInfoRegular>();
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

QStringList ModList::allModsByProfilePriority(MOBase::IProfile* profile) const
{
  Profile* mo2Profile = profile == nullptr ?
    m_Organizer->currentProfile()
    : dynamic_cast<Profile*>(profile);
  return m_Organizer->modsSortedByProfilePriority(mo2Profile);
}

MOBase::IModInterface* ModList::getMod(const QString& name) const
{
  unsigned int index = ModInfo::getIndex(name);
  return index == UINT_MAX ? nullptr : ModInfo::getByIndex(index).data();
}

bool ModList::removeMod(MOBase::IModInterface* mod)
{
  unsigned int index = ModInfo::getIndex(mod->name());
  if (index == UINT_MAX) {
    return mod->remove();
  }
  else {
    return ModInfo::removeMod(index);
  }
  notifyModRemoved(mod->name());
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
    log::debug("Trying to {} mod {} which does not exist.",
      active ? "enable" : "disable", name);
    return false;
  } else {
    m_Profile->setModEnabled(modIndex, active);
    return true;
  }
}

int ModList::setActive(const QStringList& names, bool active) {

  // We only add indices for mods that exist (modIndex != UINT_MAX)
  // and that can be enabled / disabled.
  QList<unsigned int> indices;
  for (const auto& name : names) {
    auto modIndex = ModInfo::getIndex(name);
    if (modIndex != UINT_MAX) {
      indices.append(modIndex);
    }
    else {
      log::debug("Trying to {} mod {} which does not exist.", 
        active ? "enable" : "disable", name);
    }
  }

  if (active) {
    m_Profile->setModsEnabled(indices, {});
  }
  else {
    m_Profile->setModsEnabled({}, indices);
  }

  return indices.size();
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

bool ModList::onModInstalled(const std::function<void(MOBase::IModInterface*)>& func)
{
  return m_ModInstalled.connect(func).connected();
}

bool ModList::onModRemoved(const std::function<void(QString const&)>& func)
{
  return m_ModRemoved.connect(func).connected();
}

bool ModList::onModStateChanged(const std::function<void(const std::map<QString, ModStates>&)>& func)
{
  return m_ModStateChanged.connect(func).connected();
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
  std::map<QString, ModStates> mods;
  for (auto modIndex : modIndices) {
    ModInfo::Ptr modInfo = ModInfo::getByIndex(modIndex);
    mods.emplace(modInfo->name(), state(modIndex));
  }
  m_ModStateChanged(mods);
}

bool ModList::onModMoved(const std::function<void (const QString &, int, int)> &func)
{
  auto conn = m_ModMoved.connect(func);
  return conn.connected();
}

bool ModList::dropURLs(const QMimeData *mimeData, int row, const QModelIndex &parent)
{
  if (row == -1) {
    row = parent.row();
  }
  ModInfo::Ptr modInfo = ModInfo::getByIndex(row);
  QDir modDir = QDir(modInfo->absolutePath());

  QDir allModsDir(Settings::instance().paths().mods());
  QDir overwriteDir(Settings::instance().paths().overwrite());

  QStringList sourceList;
  QStringList targetList;
  QList<QPair<QString,QString>> relativePathList;

  unsigned int overwriteIndex = ModInfo::findMod([] (ModInfo::Ptr mod) -> bool {
    std::vector<ModInfo::EFlag> flags = mod->getFlags();
    return std::find(flags.begin(), flags.end(), ModInfo::FLAG_OVERWRITE) != flags.end(); });

  QString overwriteName = ModInfo::getByIndex(overwriteIndex)->name();

  for (auto url : mimeData->urls()) {
    if (!url.isLocalFile()) {
      log::debug("URL drop ignored: \"{}\" is not a local file", url.url());
      continue;
    }

    QFileInfo sourceInfo(url.toLocalFile());
    QString sourceFile = sourceInfo.canonicalFilePath();

    QString relativePath;
    QString originName;

    if (sourceFile.startsWith(allModsDir.canonicalPath())) {
      QDir relativeDir(allModsDir.relativeFilePath(sourceFile));
      QStringList splitPath = relativeDir.path().split("/");
      originName = splitPath[0];
      splitPath.pop_front();
      relativePath = splitPath.join("/");
    } else if (sourceFile.startsWith(overwriteDir.canonicalPath())) {
      originName = overwriteName;
      relativePath = overwriteDir.relativeFilePath(sourceFile);
    } else {
      log::debug("URL drop ignored: \"{}\" is not a known file to MO", sourceFile);
      continue;
    }

    QFileInfo targetInfo(modDir.absoluteFilePath(relativePath));
    sourceList << sourceFile;
    targetList << targetInfo.absoluteFilePath();
    relativePathList << QPair<QString,QString>(relativePath, originName);
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

  m_Profile->cancelModlistWrite();
  beginRemoveRows(parent, row, row);
  ModInfo::removeMod(row);
  endRemoveRows();
  m_Profile->refreshModStatus();  // removes the mod from the status list
  m_Profile->writeModlist(); // this ensures the modified list gets written back before new mods can be installed

  notifyModRemoved(modInfo->name());

  if (wasEnabled) {
    emit removeOrigin(modInfo->name());
  }
  auto flags = modInfo->getFlags();
  if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_BACKUP) == flags.end()) {
    emit modUninstalled(modInfo->installationFile());
  }
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

  if (count == 1) {
    ModInfo::Ptr modInfo = ModInfo::getByIndex(row);
    std::vector<ModInfo::EFlag> flags = modInfo->getFlags();
    if ((std::find(flags.begin(), flags.end(), ModInfo::FLAG_OVERWRITE) != flags.end()) && (QDir(modInfo->absolutePath()).count() > 2)) {
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
  Guard g([&]{ m_InNotifyChange = false; });

  if (rowStart < 0) {
    m_Overwrite.clear();
    m_Overwritten.clear();
    m_ArchiveOverwrite.clear();
    m_ArchiveOverwritten.clear();
    m_ArchiveLooseOverwrite.clear();
    m_ArchiveLooseOverwritten.clear();
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
    case COL_CONFLICTFLAGS: return tr("Conflicts");
    case COL_FLAGS:    return tr("Flags");
    case COL_CONTENT:  return tr("Content");
    case COL_NAME:     return tr("Mod Name");
    case COL_VERSION:  return tr("Version");
    case COL_PRIORITY: return tr("Priority");
    case COL_CATEGORY: return tr("Category");
    case COL_GAME:     return tr("Source Game");
    case COL_MODID:    return tr("Nexus ID");
    case COL_INSTALLTIME: return tr("Installation");
    case COL_NOTES:    return tr("Notes");
    default: return tr("unknown");
  }
}


QString ModList::getColumnToolTip(int column) const
{
  switch (column) {
    case COL_NAME:     return tr("Name of your mods");
    case COL_VERSION:  return tr("Version of the mod (if available)");
    case COL_PRIORITY: return tr("Installation priority of your mod. The higher, the more \"important\" it is and thus "
                                 "overwrites files from mods with lower priority.");
    case COL_CATEGORY: return tr("Primary category of the mod.");
    case COL_GAME:     return tr("The source game which was the origin of this mod.");
    case COL_MODID:    return tr("Id of the mod as used on Nexus.");
    case COL_CONFLICTFLAGS: return tr("Indicators of file conflicts between mods.");
    case COL_FLAGS:    return tr("Emblems to highlight things that might require attention.");
    case COL_CONTENT: {
      auto& contents = m_Organizer->modDataContents();
      if (m_Organizer->modDataContents().empty()) {
        return QString();
      }
      QString result = tr("Depicts the content of the mod:") + "<br>" + "<table cellspacing=7>";
      m_Organizer->modDataContents().forEachContent([&result](auto const& content) {
        result += QString("<tr><td><img src=\"%1\" width=32/></td><td>%2</td></tr>")
          .arg(content.icon()).arg(content.name());
      });
      return result + "</table>";
    };
    case COL_INSTALLTIME: return tr("Time this mod was installed");
    case COL_NOTES:       return tr("User notes about the mod");
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
      rows.swapItemsAt(i, rows.size() - i - 1);
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

  QList<unsigned int> modsToEnable;
  QList<unsigned int> modsToDisable;
  QModelIndexList dirtyMods;
  for (QModelIndex idx : selectionModel->selectedRows()) {
    int modId = idx.data(Qt::UserRole + 1).toInt();
    if (m_Profile->modEnabled(modId)) {
      modsToDisable.append(modId);
      dirtyMods.append(idx);
    } else {
      modsToEnable.append(modId);
      dirtyMods.append(idx);
    }
  }

  m_Profile->setModsEnabled(modsToEnable, modsToDisable);

  emit modlistChanged(dirtyMods, 0);
  emit tutorialModlistUpdate();

  m_Modified = true;
  m_LastCheck.restart();

  emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));

  emit postDataChanged();

  return true;
}

bool ModList::eventFilter(QObject *obj, QEvent *event)
{
  if ((event->type() == QEvent::KeyPress) && (m_Profile != nullptr)) {
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
    return QAbstractItemModel::eventFilter(obj, event);
  }
  return QAbstractItemModel::eventFilter(obj, event);
}

//note: caller needs to make sure sort proxy is updated
void ModList::enableSelected(const QItemSelectionModel *selectionModel)
{
  if (selectionModel->hasSelection()) {
    QList<unsigned int> modsToEnable;
    for (auto row : selectionModel->selectedRows(COL_PRIORITY)) {
      int modID = m_Profile->modIndexByPriority(row.data().toInt());
      modsToEnable.append(modID);
    }
    m_Profile->setModsEnabled(modsToEnable, QList<unsigned int>());
  }
}

//note: caller needs to make sure sort proxy is updated
void ModList::disableSelected(const QItemSelectionModel *selectionModel)
{
  if (selectionModel->hasSelection()) {
    QList<unsigned int> modsToDisable;
    for (auto row : selectionModel->selectedRows(COL_PRIORITY)) {
      int modID = m_Profile->modIndexByPriority(row.data().toInt());
      modsToDisable.append(modID);
    }
    m_Profile->setModsEnabled(QList<unsigned int>(), modsToDisable);
  }
}

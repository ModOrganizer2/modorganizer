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

#include "report.h"
#include "utility.h"
#include "messagedialog.h"
#include "installationtester.h"
#include <gameinfo.h>

#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QMimeData>
#include <stdexcept>
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
#include <sstream>
#include <algorithm>


ModList::ModList(QObject *parent)
  : QAbstractItemModel(parent), m_Profile(NULL), m_Modified(false),
    m_FontMetrics(QFont())
{
//  setSupportedDragActions(Qt::MoveAction);
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


int ModList::columnCount(const QModelIndex &) const
{
  return COL_LASTCOLUMN + 1;
}


QVariant ModList::getOverwriteData(int column, int role) const
{
  switch (role) {
    case Qt::DisplayRole: {
      if (column == 0) {
        return tr("Overwrite");
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


QVariant ModList::data(const QModelIndex &modelIndex, int role) const
{
  if (m_Profile == NULL) return QVariant();
  unsigned int modIndex = modelIndex.row();
  int column = modelIndex.column();

  ModInfo::Ptr modInfo = ModInfo::getByIndex(modIndex);
  if ((role == Qt::DisplayRole) ||
      (role == Qt::EditRole)) {
    if (column == COL_FLAGS) {
      return QVariant();
    } else if (column == COL_NAME) {
      return modInfo->name();
    } else if (column == COL_VERSION) {
      QString version = modInfo->getVersion().canonicalString();
      if (version.isEmpty() && modInfo->canBeUpdated()) {
        version = "?";
      }
      return version;
    } else if (column == COL_PRIORITY) {
      int priority = modInfo->getFixedPriority();
      if (priority != INT_MIN) {
        return QVariant(); // hide priority for mods where it's fixed
      } else {
        if ((m_Profile->getModPriority(modIndex) == 0) && (role == Qt::DisplayRole)) {
          return tr("min");
        } else if ((m_Profile->getModPriority(modIndex) == m_Profile->numRegularMods() - 1) &&
                   (role == Qt::DisplayRole)) {
          return tr("max");
        } else {
          //return QString::number(m_Profile->getModPriority(modIndex));
          return m_Profile->getModPriority(modIndex);
        }
      }
    } else if (column == COL_MODID) {
      int modID = modInfo->getNexusID();
      if (modID >= 0) {
        return modID;
      } else {
        return QVariant();
      }
    } else if (column == COL_CATEGORY) {
      int category = modInfo->getPrimaryCategory();
      if (category != -1) {
        CategoryFactory &categoryFactory = CategoryFactory::instance();
        try {
          return categoryFactory.getCategoryName(categoryFactory.getCategoryIndex(category));
        } catch (const std::exception &e) {
          qCritical("failed to retrieve category name: %s", e.what());
          return QString();
        }
      } else {
        //return tr("None");
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
    return modInfo->getNexusID();
  } else if (role == Qt::FontRole) {
    QFont result;
    if (column == COL_NAME) {
      if (modInfo->getHighlight() & ModInfo::HIGHLIGHT_INVALID) {
        result.setItalic(true);
      }
    } else if (column == COL_VERSION) {
      if (modInfo->updateAvailable()) {
        result.setWeight(QFont::Bold);
      }
    }
    return result;
  } else if (role == Qt::DecorationRole) {
    if ((column == COL_VERSION) &&
        modInfo->updateAvailable() &&
        modInfo->getNewestVersion().isValid()) {
      return QIcon(":/MO/gui/update_available");
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
      } else if (modInfo->updateAvailable()) {
        return QBrush(Qt::red);
      } else {
        return QBrush(Qt::darkGreen);
      }
    }
    return QVariant();
  } else if (role == Qt::ToolTipRole) {
    if (column == COL_FLAGS) {
      QString result;

      std::vector<ModInfo::EFlag> flags = modInfo->getFlags();

      for (auto iter = flags.begin(); iter != flags.end(); ++iter) {
        if (result.length() != 0) result += "<br>";
        result += getFlagText(*iter, modInfo);
      }

      return result;
    } else if (column == COL_NAME) {
      try {
        return modInfo->getDescription();
      } catch (const std::exception &e) {
        qCritical("invalid mod description: %s", e.what());
        return QString();
      }
    } else if (column == COL_VERSION) {
      return tr("installed version: %1, newest version: %2").arg(modInfo->getVersion().canonicalString()).arg(modInfo->getNewestVersion().canonicalString());
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
  // before we rename, write back the current profile so we don't lose changes and to ensure
  // there is no scheduled asynchronous rewrite anytime soon
  m_Profile->writeModlistNow();

  ModInfo::Ptr modInfo = ModInfo::getByIndex(index);
  QString oldName = modInfo->name();
  modInfo->setName(newName);
  // this just broke all profiles! The recipient of modRenamed has to do some magic
  // to can't write the currently active profile back
  emit modRenamed(oldName, newName);

  // invalidate the currently displayed state of this list
  notifyChange(-1);
  return true;
}


bool ModList::setData(const QModelIndex &index, const QVariant &value, int role)
{
  if (m_Profile == NULL) return false;

  if (static_cast<unsigned int>(index.row()) >= ModInfo::getNumMods()) {
    return false;
  }

  if (role == Qt::CheckStateRole) {
    bool enabled = value.toInt() == Qt::Checked;
    if (m_Profile->modEnabled(index.row()) != enabled) {
      m_Profile->setModEnabled(index.row(), enabled);
      m_Modified = true;

      emit modlist_changed(index.row());
    }
    return true;
  } else if (role == Qt::EditRole) {
    switch (index.column()) {
      case COL_NAME: {
        return renameMod(index.row(), value.toString());
      } break;
      case COL_PRIORITY: {
        bool ok = false;
        int newPriority = value.toInt(&ok);
        if (ok) {
          m_Profile->setModPriority(index.row(), newPriority);

          emit modlist_changed(index.row());
          return true;
        } else {
          return false;
        }
      } break;
      case COL_MODID: {
        ModInfo::Ptr info = ModInfo::getByIndex(index.row());
        bool ok = false;
        int newID = value.toInt(&ok);
        if (ok) {
          info->setNexusID(newID);
          emit modlist_changed(index.row());
          return true;
        } else {
          return false;
        }
      } break;
      case COL_VERSION: {
        ModInfo::Ptr info = ModInfo::getByIndex(index.row());
        VersionInfo version(value.toString());
        if (version.isValid()) {
          info->setVersion(version);
          return true;
        } else {
          return false;
        }
      } break;
      default: {
        qWarning("edit on column \"%s\" not supported",
                 getColumnName(index.column()).toUtf8().constData());
        return false;
      } break;
    }
  } else {
    return false;
  }
}


ModList::EColumn ModList::getEnabledColumn(int index) const
{
  for (int i = 0; i <= COL_LASTCOLUMN; ++i) {
    if (index == 0) {
      return static_cast<EColumn>(i);
    } else {
      --index;
    }
  }
  return ModList::COL_NAME;
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
  } else {
    result |= Qt::ItemIsDropEnabled;
  }
  return result;
}


void ModList::changeModPriority(std::vector<int> sourceIndices, int newPriority)
{
  if (m_Profile == NULL) return;

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
    m_Profile->setModPriority(*iter, newPriority);
  }

  emit layoutChanged();

  emit modorder_changed();
}


void ModList::changeModPriority(int sourceIndex, int newPriority)
{
  if (m_Profile == NULL) return;
  emit layoutAboutToBeChanged();

  m_Profile->setModPriority(sourceIndex, newPriority);

  emit layoutChanged();

  emit modorder_changed();
}


bool ModList::dropMimeData(const QMimeData *mimeData, Qt::DropAction action, int row, int, const QModelIndex &parent)
{
  if (action == Qt::IgnoreAction) {
    return true;
  }

  if (m_Profile == NULL) return false;

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


void ModList::removeRowForce(int row)
{
  if (static_cast<unsigned int>(row) >= ModInfo::getNumMods()) {
    return;
  }
  if (m_Profile == NULL) return;

  ModInfo::Ptr modInfo = ModInfo::getByIndex(row);

  bool wasEnabled = m_Profile->modEnabled(row);
  beginRemoveRows(QModelIndex(), row, row);

  m_Profile->cancelWriteModlist(); // don't write modlist while we're changing it
  ModInfo::removeMod(row);
  m_Profile->refreshModStatus();  // removes the mod from the status list
  m_Profile->writeModlist(); // this ensures the modified list gets written back before new mods can be installed

  endRemoveRows();
  if (wasEnabled) {
    emit removeOrigin(modInfo->name());
  }
}


void ModList::removeRow(int row, const QModelIndex&)
{
  if (static_cast<unsigned int>(row) >= ModInfo::getNumMods()) {
    return;
  }
  if (m_Profile == NULL) return;

  ModInfo::Ptr modInfo = ModInfo::getByIndex(row);

  QMessageBox confirmBox(QMessageBox::Question, tr("Confirm"), tr("Are you sure you want to remove \"%1\"?").arg(modInfo->name()),
                         QMessageBox::Yes | QMessageBox::No);

  if (confirmBox.exec() == QMessageBox::Yes) {
    removeRowForce(row);
  }
}


void ModList::notifyChange(int row)
{
  if (row < 0) {
    beginResetModel();
    endResetModel();
  } else {
    emit dataChanged(this->index(row, 0), this->index(row, this->columnCount() - 1));
  }
}


QModelIndex ModList::index(int row, int column, const QModelIndex&) const
{
  return createIndex(row, column, row);
}


QModelIndex ModList::parent(const QModelIndex&) const
{
  return QModelIndex();
}


QString ModList::getColumnName(int column)
{
  switch (column) {
    case COL_FLAGS:    return tr("Flags");
    case COL_NAME:     return tr("Name");
    case COL_VERSION:  return tr("Version");
    case COL_PRIORITY: return tr("Priority");
    case COL_CATEGORY: return tr("Category");
    case COL_MODID:    return tr("Nexus ID");
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
    default: return tr("unknown");
  }
}


bool ModList::eventFilter(QObject *obj, QEvent *event)
{
  if (event->type() == QEvent::ContextMenu) {
    QContextMenuEvent *contextEvent = static_cast<QContextMenuEvent*>(event);
    QWidget *object = qobject_cast<QWidget*>(obj);
    if (object != NULL) {
      emit requestColumnSelect(object->mapToGlobal(contextEvent->pos()));

      return true;
    }
  } else if ((event->type() == QEvent::KeyPress) && (m_Profile != NULL)) {
    QAbstractItemView *itemView = qobject_cast<QAbstractItemView*>(obj);
    QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
    if ((itemView != NULL) &&
        (keyEvent->modifiers() == Qt::ControlModifier) &&
        ((keyEvent->key() == Qt::Key_Up) || (keyEvent->key() == Qt::Key_Down))) {
      QItemSelectionModel *selectionModel = itemView->selectionModel();
      const QSortFilterProxyModel *proxyModel = qobject_cast<const QSortFilterProxyModel*>(selectionModel->model());
      int diff = -1;
      if (((keyEvent->key() == Qt::Key_Up) && (proxyModel->sortOrder() == Qt::DescendingOrder)) ||
          ((keyEvent->key() == Qt::Key_Down) && (proxyModel->sortOrder() == Qt::AscendingOrder))) {
        diff = 1;
      }
      QModelIndexList rows = selectionModel->selectedRows();
      if (keyEvent->key() == Qt::Key_Down) {
        for (int i = 0; i < rows.size() / 2; ++i) {
          rows.swap(i, rows.size() - i - 1);
        }
      }
      foreach (QModelIndex idx, rows) {
        if (proxyModel != NULL) {
          idx = proxyModel->mapToSource(idx);
        }
        int newPriority = m_Profile->getModPriority(idx.row()) + diff;
        if ((newPriority >= 0) && (newPriority < static_cast<int>(m_Profile->numRegularMods()))) {
          m_Profile->setModPriority(idx.row(), newPriority);
          notifyChange(idx.row());
        }
      }
      return true;
    } else if (keyEvent->key() == Qt::Key_Delete) {
      QItemSelectionModel *selectionModel = itemView->selectionModel();
      QModelIndexList rows = selectionModel->selectedRows();
      if (rows.count() > 1) {
        emit removeSelectedMods();
      } else if (rows.count() == 1) {
        removeRow(rows[0].row(), QModelIndex());
      }
      return true;
    }
  }
  return QObject::eventFilter(obj, event);
}

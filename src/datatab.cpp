#include "datatab.h"
#include "ui_mainwindow.h"
#include "settings.h"
#include "organizercore.h"
#include "directoryentry.h"
#include "messagedialog.h"
#include "filetree.h"
#include <log.h>
#include <report.h>

using namespace MOShared;
using namespace MOBase;

// in mainwindow.cpp
QString UnmanagedModName();


DataTab::DataTab(
  OrganizerCore& core, PluginContainer& pc,
  QWidget* parent, Ui::MainWindow* mwui) :
    m_core(core), m_pluginContainer(pc), m_parent(parent),
    ui{
      mwui->btnRefreshData, mwui->dataTree,
      mwui->conflictsCheckBox, mwui->showArchiveDataCheckBox}
{
  m_filetree.reset(new FileTree(core, m_pluginContainer, ui.tree));

  connect(
    ui.refresh, &QPushButton::clicked,
    [&]{ onRefresh(); });

  //connect(
  //  ui.tree, &QTreeWidget::itemActivated,
  //  [&](auto* item, int col){ onItemActivated(item, col); });

  connect(
    ui.conflicts, &QCheckBox::toggled,
    [&]{ onConflicts(); });

  connect(
    ui.archives, &QCheckBox::toggled,
    [&]{ onArchives(); });
}

void DataTab::saveState(Settings& s) const
{
  s.geometry().saveState(ui.tree->header());
}

void DataTab::restoreState(const Settings& s)
{
  s.geometry().restoreState(ui.tree->header());
}

void DataTab::activated()
{
  refreshDataTreeKeepExpandedNodes();
}

QTreeWidgetItem* DataTab::singleSelection()
{
  //const auto sel = ui.tree->selectedItems();
  //if (sel.count() != 1) {
  //  return nullptr;
  //}
  //
  //return sel[0];
  return nullptr;
}

void DataTab::openSelection()
{
}

void DataTab::open(QTreeWidgetItem* item)
{
}

void DataTab::runSelectionHooked()
{
  if (auto* item=singleSelection()) {
    runHooked(item);
  }
}

void DataTab::runHooked(QTreeWidgetItem* item)
{
}

void DataTab::previewSelection()
{
  if (auto* item=singleSelection()) {
    preview(item);
  }
}

void DataTab::preview(QTreeWidgetItem* item)
{
}

void DataTab::onRefresh()
{
  m_core.refreshDirectoryStructure();
}

void DataTab::refreshDataTree()
{
  m_filetree->refresh();
}

void DataTab::refreshDataTreeKeepExpandedNodes()
{
  //m_model->refreshKeepExpandedNodes();
  m_filetree->refresh();  // temp

  /*QIcon folderIcon = (new QFileIconProvider())->icon(QFileIconProvider::Folder);
  QIcon fileIcon = (new QFileIconProvider())->icon(QFileIconProvider::File);
  QStringList expandedNodes;
  QTreeWidgetItemIterator it1(ui.tree, QTreeWidgetItemIterator::NotHidden | QTreeWidgetItemIterator::HasChildren);
  while (*it1) {
    QTreeWidgetItem *current = (*it1);
    if (current->isExpanded() && !(current->text(0)=="data")) {
      expandedNodes.append(current->text(0)+"/"+current->parent()->text(0));
    }
    ++it1;
  }

  ui.tree->clear();
  QStringList columns("data");
  columns.append("");
  QTreeWidgetItem *subTree = new QTreeWidgetItem(columns);
  subTree->setData(0, Qt::DecorationRole, (new QFileIconProvider())->icon(QFileIconProvider::Folder));
  updateTo(subTree, L"", *m_core.directoryStructure(), ui.conflicts->isChecked(), &fileIcon, &folderIcon);
  ui.tree->insertTopLevelItem(0, subTree);
  subTree->setExpanded(true);
  QTreeWidgetItemIterator it2(ui.tree, QTreeWidgetItemIterator::HasChildren);
  while (*it2) {
    QTreeWidgetItem *current = (*it2);
    if (!(current->text(0)=="data") && expandedNodes.contains(current->text(0)+"/"+current->parent()->text(0))) {
      current->setExpanded(true);
    }
    ++it2;
  }*/
}

void DataTab::updateTo(
  QTreeWidgetItem *subTree, const std::wstring &directorySoFar,
  const DirectoryEntry &directoryEntry, bool conflictsOnly,
  QIcon *fileIcon, QIcon *folderIcon)
{
}

void DataTab::onItemExpanded(QTreeWidgetItem* item)
{
  if ((item->childCount() == 1) && (item->child(0)->data(0, Qt::UserRole).toString() == "__loaded_on_demand__")) {
    // read the data we need from the sub-item, then dispose of it
    QTreeWidgetItem *onDemandDataItem = item->child(0);
    const QString path = onDemandDataItem->data(0, Qt::UserRole + 1).toString();
    std::wstring wspath = path.toStdWString();
    bool conflictsOnly = onDemandDataItem->data(0, Qt::UserRole + 2).toBool();

    std::wstring virtualPath = (wspath + L"\\").substr(6) + item->text(0).toStdWString();
    DirectoryEntry *dir = m_core.directoryStructure()->findSubDirectoryRecursive(virtualPath);
    if (dir != nullptr) {
      QIcon folderIcon = (new QFileIconProvider())->icon(QFileIconProvider::Folder);
      QIcon fileIcon = (new QFileIconProvider())->icon(QFileIconProvider::File);
      updateTo(item, wspath, *dir, conflictsOnly, &fileIcon, &folderIcon);
    } else {
      log::warn("failed to update view of {}", path);
    }

    m_removeLater.push_back(item);

    QTimer::singleShot(5, [this]{
      for (QTreeWidgetItem *item : m_removeLater) {
        item->removeChild(item->child(0));
      }
      m_removeLater.clear();
    });
  }
}

void DataTab::onItemActivated(QTreeWidgetItem *item, int column)
{
  const auto isArchive = item->data(0, Qt::UserRole + 1).toBool();
  const auto isDirectory = item->data(0, Qt::UserRole + 3).toBool();

  if (isArchive || isDirectory) {
    return;
  }

  const QString path = item->data(0, Qt::UserRole).toString();
  const QFileInfo targetInfo(path);

  const auto tryPreview = m_core.settings().interface().doubleClicksOpenPreviews();

  if (tryPreview && m_pluginContainer.previewGenerator().previewSupported(targetInfo.suffix())) {
    emit preview(item);
  } else {
    emit open(item);
  }
}

void DataTab::onConflicts()
{
  updateOptions();
}

void DataTab::onArchives()
{
  updateOptions();
}

void DataTab::updateOptions()
{
  FileTreeModel::Flags flags = FileTreeModel::NoFlags;

  if (ui.conflicts->isChecked()) {
    flags |= FileTreeModel::Conflicts;
  }

  if (ui.archives->isChecked()) {
    flags |= FileTreeModel::Archives;
  }

  m_filetree->setFlags(flags);
  refreshDataTree();
}

void DataTab::addAsExecutable()
{
}

void DataTab::openOriginInExplorer()
{
  auto* item = singleSelection();
  if (!item) {
    return;
  }

  const auto isArchive = item->data(0, Qt::UserRole + 1).toBool();
  const auto isDirectory = item->data(0, Qt::UserRole + 3).toBool();

  if (isArchive || isDirectory) {
    return;
  }

  const auto fullPath = item->data(0, Qt::UserRole).toString();

  log::debug("opening in explorer: {}", fullPath);
  shell::Explore(fullPath);
}

void DataTab::openModInfo()
{
  auto* item = singleSelection();
  if (!item) {
    return;
  }

  const auto originID = item->data(1, Qt::UserRole + 1).toInt();
  if (originID == 0) {
    // unmanaged
    return;
  }

  const auto& origin = m_core.directoryStructure()->getOriginByID(originID);
  const auto& name = QString::fromStdWString(origin.getName());

  unsigned int index = ModInfo::getIndex(name);
  if (index == UINT_MAX) {
    log::error("can't open mod info, mod '{}' not found", name);
    return;
  }

  ModInfo::Ptr modInfo = ModInfo::getByIndex(index);
  if (modInfo) {
    emit displayModInformation(modInfo, index, ModInfoTabIDs::None);
  }
}

void DataTab::hideFile()
{
  auto* item = singleSelection();
  if (!item) {
    return;
  }

  QString oldName = item->data(0, Qt::UserRole).toString();
  QString newName = oldName + ModInfo::s_HiddenExt;

  if (QFileInfo(newName).exists()) {
    if (QMessageBox::question(m_parent, tr("Replace file?"), tr("There already is a hidden version of this file. Replace it?"),
      QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
      if (!QFile(newName).remove()) {
        QMessageBox::critical(m_parent, tr("File operation failed"), tr("Failed to remove \"%1\". Maybe you lack the required file permissions?").arg(newName));
        return;
      }
    } else {
      return;
    }
  }

  if (QFile::rename(oldName, newName)) {
    emit originModified(item->data(1, Qt::UserRole + 1).toInt());
    refreshDataTreeKeepExpandedNodes();
  } else {
    reportError(tr("failed to rename \"%1\" to \"%2\"").arg(oldName).arg(QDir::toNativeSeparators(newName)));
  }
}

void DataTab::unhideFile()
{
  auto* item = singleSelection();
  if (!item) {
    return;
  }

  QString oldName = item->data(0, Qt::UserRole).toString();
  QString newName = oldName.left(oldName.length() - ModInfo::s_HiddenExt.length());
  if (QFileInfo(newName).exists()) {
    if (QMessageBox::question(m_parent, tr("Replace file?"), tr("There already is a visible version of this file. Replace it?"),
      QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
      if (!QFile(newName).remove()) {
        QMessageBox::critical(m_parent, tr("File operation failed"), tr("Failed to remove \"%1\". Maybe you lack the required file permissions?").arg(newName));
        return;
      }
    } else {
      return;
    }
  }
  if (QFile::rename(oldName, newName)) {
    emit originModified(item->data(1, Qt::UserRole + 1).toInt());
    refreshDataTreeKeepExpandedNodes();
  } else {
    reportError(tr("failed to rename \"%1\" to \"%2\"").arg(QDir::toNativeSeparators(oldName)).arg(QDir::toNativeSeparators(newName)));
  }
}

void DataTab::writeDataToFile(
  QFile &file, const QString &directory, const DirectoryEntry &directoryEntry)
{
  for (FileEntry::Ptr current : directoryEntry.getFiles()) {
    bool isArchive = false;
    int origin = current->getOrigin(isArchive);
    if (isArchive) {
      // TODO: don't list files from archives. maybe make this an option?
      continue;
    }
    QString fullName = directory + "\\" + ToQString(current->getName());
    file.write(fullName.toUtf8());

    file.write("\t(");
    file.write(ToQString(m_core.directoryStructure()->getOriginByID(origin).getName()).toUtf8());
    file.write(")\r\n");
  }

  // recurse into subdirectories
  std::vector<DirectoryEntry*>::const_iterator current, end;
  directoryEntry.getSubDirectories(current, end);
  for (; current != end; ++current) {
    writeDataToFile(file, directory + "\\" + ToQString((*current)->getName()), **current);
  }
}

void DataTab::writeDataToFile()
{
  QString fileName = QFileDialog::getSaveFileName(m_parent);
  if (!fileName.isEmpty()) {
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
      reportError(tr("failed to write to file %1").arg(fileName));
    }

    writeDataToFile(file, "data", *m_core.directoryStructure());
    file.close();

    MessageDialog::showMessage(tr("%1 written").arg(QDir::toNativeSeparators(fileName)), m_parent);
  }
}

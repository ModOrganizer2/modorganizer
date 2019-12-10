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
    m_core(core), m_pluginContainer(pc), m_archives(false), m_parent(parent),
    m_model(new FileTreeModel(core)),
    ui{
      mwui->btnRefreshData, mwui->dataTree,
      mwui->conflictsCheckBox, mwui->showArchiveDataCheckBox}
{
  ui.tree->setModel(m_model);

  connect(
    ui.refresh, &QPushButton::clicked,
    [&]{ onRefresh(); });

  //connect(
  //  ui.tree, &QTreeWidget::itemExpanded,
  //  [&](auto* item){ onItemExpanded(item); });
  //
  //connect(
  //  ui.tree, &QTreeWidget::itemActivated,
  //  [&](auto* item, int col){ onItemActivated(item, col); });

  connect(
    ui.tree, &QTreeWidget::customContextMenuRequested,
    [&](auto pos){ onContextMenu(pos); });

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
  if (auto* item=singleSelection()) {
    open(item);
  }
}

void DataTab::open(QTreeWidgetItem* item)
{
  const auto isArchive = item->data(0, Qt::UserRole + 1).toBool();
  const auto isDirectory = item->data(0, Qt::UserRole + 3).toBool();

  if (isArchive || isDirectory) {
    return;
  }

  const QString path = item->data(0, Qt::UserRole).toString();
  const QFileInfo targetInfo(path);

  m_core.processRunner()
    .setFromFile(m_parent, targetInfo)
    .setHooked(false)
    .setWaitForCompletion(ProcessRunner::Refresh)
    .run();
}

void DataTab::runSelectionHooked()
{
  if (auto* item=singleSelection()) {
    runHooked(item);
  }
}

void DataTab::runHooked(QTreeWidgetItem* item)
{
  const auto isArchive = item->data(0, Qt::UserRole + 1).toBool();
  const auto isDirectory = item->data(0, Qt::UserRole + 3).toBool();

  if (isArchive || isDirectory) {
    return;
  }

  const QString path = item->data(0, Qt::UserRole).toString();
  const QFileInfo targetInfo(path);

  m_core.processRunner()
    .setFromFile(m_parent, targetInfo)
    .setHooked(true)
    .setWaitForCompletion(ProcessRunner::Refresh)
    .run();
}

void DataTab::previewSelection()
{
  if (auto* item=singleSelection()) {
    preview(item);
  }
}

void DataTab::preview(QTreeWidgetItem* item)
{
  QString fileName = QDir::fromNativeSeparators(item->data(0, Qt::UserRole).toString());
  m_core.previewFileWithAlternatives(m_parent, fileName);
}

void DataTab::onRefresh()
{
  m_core.refreshDirectoryStructure();
}

void DataTab::refreshDataTree()
{
  m_model->refresh();
  ui.tree->expand(m_model->index(0, 0));
}

void DataTab::refreshDataTreeKeepExpandedNodes()
{
  //m_model->refreshKeepExpandedNodes();
  m_model->refresh();  // temp

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

void DataTab::onContextMenu(const QPoint &pos)
{
  QTreeWidgetItem* item = nullptr;//ui.tree->itemAt(pos.x(), pos.y());

  QMenu menu;
  if ((item != nullptr) && (item->childCount() == 0)
    && (item->data(0, Qt::UserRole + 3).toBool() != true)) {
    QString fileName = item->text(0);
    const auto isArchive = item->data(0, Qt::UserRole + 1).toBool();
    const auto isDirectory = item->data(0, Qt::UserRole + 3).toBool();

    QAction* open = nullptr;
    QAction* runHooked = nullptr;
    QAction* preview = nullptr;

    if (canRunFile(isArchive, fileName)) {
      open = new QAction(tr("&Execute"), ui.tree);
      runHooked = new QAction(tr("Execute with &VFS"), ui.tree);
    } else if (canOpenFile(isArchive, fileName)) {
      open = new QAction(tr("&Open"), ui.tree);
      runHooked = new QAction(tr("Open with &VFS"), ui.tree);
    }

    if (m_pluginContainer.previewGenerator().previewSupported(QFileInfo(fileName).suffix())) {
      preview = new QAction(tr("Preview"), ui.tree);
    }

    if (open) {
      connect(open, &QAction::triggered, [&]{ openSelection(); });
    }

    if (runHooked) {
      connect(runHooked, &QAction::triggered, [&]{ runSelectionHooked(); });
    }

    if (preview) {
      connect(preview, &QAction::triggered, [&]{ previewSelection(); });
    }

    if (open && preview) {
      if (m_core.settings().interface().doubleClicksOpenPreviews()) {
        menu.addAction(preview);
        menu.addAction(open);
      } else {
        menu.addAction(open);
        menu.addAction(preview);
      }
    } else {
      if (open) {
        menu.addAction(open);
      }

      if (preview) {
        menu.addAction(preview);
      }
    }

    if (runHooked) {
      menu.addAction(runHooked);
    }

    menu.addAction(tr("&Add as Executable"), [&]{ addAsExecutable(); });

    if (!isArchive && !isDirectory) {
      menu.addAction("Open Origin in Explorer", [&]{ openOriginInExplorer(); });
    }

    menu.addAction("Open Mod Info", [&]{ openModInfo(); });

    menu.addSeparator();

    // offer to hide/unhide file, but not for files from archives
    if (!isArchive) {
      if (item->text(0).endsWith(ModInfo::s_HiddenExt)) {
        menu.addAction(tr("Un-Hide"), [&]{ unhideFile(); });
      } else {
        menu.addAction(tr("Hide"), [&]{ hideFile(); });
      }
    }

    if (open || preview || runHooked) {
      // bold the first option
      auto* top = menu.actions()[0];
      auto f = top->font();
      f.setBold(true);
      top->setFont(f);
    }
  }

  menu.addAction(tr("Write To File..."), [&]{ writeDataToFile(); });
  menu.addAction(tr("Refresh"), [&]{ onRefresh(); });

  menu.exec(ui.tree->viewport()->mapToGlobal(pos));
}

void DataTab::onConflicts()
{
  refreshDataTreeKeepExpandedNodes();
}

void DataTab::onArchives()
{
  m_archives = (m_core.getArchiveParsing() && ui.archives->isChecked());
  refreshDataTree();
}

void DataTab::addAsExecutable()
{
  auto* item = singleSelection();
  if (!item) {
    return;
  }

  const QFileInfo target(item->data(0, Qt::UserRole).toString());
  const auto fec = spawn::getFileExecutionContext(m_parent, target);

  switch (fec.type)
  {
    case spawn::FileExecutionTypes::Executable:
    {
      const QString name = QInputDialog::getText(
        m_parent, tr("Enter Name"),
        tr("Enter a name for the executable"),
        QLineEdit::Normal,
        target.completeBaseName());

      if (!name.isEmpty()) {
        //Note: If this already exists, you'll lose custom settings
        m_core.executablesList()->setExecutable(Executable()
          .title(name)
          .binaryInfo(fec.binary)
          .arguments(fec.arguments)
          .workingDirectory(target.absolutePath()));

        emit executablesChanged();
      }

      break;
    }

    case spawn::FileExecutionTypes::Other:  // fall-through
    default:
    {
      QMessageBox::information(
        m_parent, tr("Not an executable"),
        tr("This is not a recognized executable."));

      break;
    }
  }
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

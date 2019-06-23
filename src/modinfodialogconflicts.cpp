#include "modinfodialogconflicts.h"
#include "ui_modinfodialog.h"
#include "modinfodialog.h"
#include "utility.h"

using namespace MOShared;
using namespace MOBase;

// if there are more than 50 selected items in the conflict tree or filetree,
// don't bother checking whether menu items apply to them, just show all of them
const int max_scan_for_context_menu = 50;


int naturalCompare(const QString& a, const QString& b)
{
  static QCollator c = []{
    QCollator c;
    c.setNumericMode(true);
    c.setCaseSensitivity(Qt::CaseInsensitive);
    return c;
  }();

  return c.compare(a, b);
}


class ElideLeftDelegate : public QStyledItemDelegate
{
public:
  using QStyledItemDelegate::QStyledItemDelegate;

protected:
  void initStyleOption(QStyleOptionViewItem* o, const QModelIndex& i) const
  {
    QStyledItemDelegate::initStyleOption(o, i);
    o->textElideMode = Qt::ElideLeft;
  }
};


class ConflictItem : public QTreeWidgetItem
{
public:
  static const int FILENAME_USERROLE = Qt::UserRole + 1;
  static const int ALT_ORIGIN_USERROLE = Qt::UserRole + 2;
  static const int ARCHIVE_USERROLE = Qt::UserRole + 3;
  static const int INDEX_USERROLE = Qt::UserRole + 4;
  static const int HAS_ALTS_USERROLE = Qt::UserRole + 5;

  ConflictItem(
    QStringList columns, FileEntry::Index index,  const QString& fileName,
    bool hasAltOrigins, const QString& altOrigin,  bool archive)
    : QTreeWidgetItem(columns)
  {
    setData(0, FILENAME_USERROLE, fileName);
    setData(0, ALT_ORIGIN_USERROLE, altOrigin);
    setData(0, ARCHIVE_USERROLE, archive);
    setData(0, INDEX_USERROLE, index);
    setData(0, HAS_ALTS_USERROLE, hasAltOrigins);

    if (archive) {
      QFont f = font(0);
      f.setItalic(true);

      for (int i=0; i<columnCount(); ++i) {
        setFont(i, f);
      }
    }
  }

  QString fileName() const
  {
    return data(0, FILENAME_USERROLE).toString();
  }

  QString altOrigin() const
  {
    return data(0, ALT_ORIGIN_USERROLE).toString();
  }

  bool hasAlts() const
  {
    return data(0, HAS_ALTS_USERROLE).toBool();
  }

  bool isArchive() const
  {
    return data(0, ARCHIVE_USERROLE).toBool();
  }

  FileEntry::Index fileIndex() const
  {
    static_assert(std::is_same_v<FileEntry::Index, unsigned int>);
    return data(0, INDEX_USERROLE).toUInt();
  }

  bool canHide() const
  {
    return canHideFile(isArchive(), fileName());
  }

  bool canUnhide() const
  {
    return canUnhideFile(isArchive(), fileName());
  }

  bool canOpen() const
  {
    return canOpenFile(isArchive(), fileName());
  }

  bool canPreview(PluginContainer& pluginContainer) const
  {
    return canPreviewFile(pluginContainer, isArchive(), fileName());
  }

  bool operator<(const QTreeWidgetItem& other) const
  {
    const int column = treeWidget()->sortColumn();

    if (column >= columnCount() || column >= other.columnCount()) {
      // shouldn't happen
      qWarning().nospace() << "ConflictItem::operator<() mistmatch in column count";
      return false;
    }

    return (naturalCompare(text(column), other.text(column)) < 0);
  }
};


ConflictsTab::ConflictsTab(
  OrganizerCore& oc, PluginContainer& plugin,
  QWidget* parent, Ui::ModInfoDialog* ui) :
    ModInfoDialogTab(oc, plugin, parent, ui),
    m_general(this, ui, oc), m_advanced(this, ui, oc)
{
  connect(
    &m_general, &GeneralConflictsTab::modOpen,
    [&](const QString& name){ emitModOpen(name); });

  connect(
    &m_advanced, &AdvancedConflictsTab::modOpen,
    [&](const QString& name){ emitModOpen(name); });
}

void ConflictsTab::update()
{
  m_general.update();
  m_advanced.update();
}

void ConflictsTab::clear()
{
  m_general.clear();
  m_advanced.clear();
}

void ConflictsTab::saveState(Settings& s)
{
  s.directInterface().setValue(
    "mod_info_conflicts_tab", ui->tabConflictsTabs->currentIndex());

  m_general.saveState(s);
  m_advanced.saveState(s);
}

void ConflictsTab::restoreState(const Settings& s)
{
  ui->tabConflictsTabs->setCurrentIndex(
    s.directInterface().value("mod_info_conflicts_tab", 0).toInt());

  m_general.restoreState(s);
  m_advanced.restoreState(s);
}

void ConflictsTab::changeItemsVisibility(
  const QList<QTreeWidgetItem*>& items, bool visible)
{
  bool changed = false;
  bool stop = false;

  qDebug().nospace()
    << (visible ? "unhiding" : "hiding") << " "
    << items.size() << " conflict files";

  QFlags<FileRenamer::RenameFlags> flags =
    (visible ? FileRenamer::UNHIDE : FileRenamer::HIDE);

  if (items.size() > 1) {
    flags |= FileRenamer::MULTIPLE;
  }

  FileRenamer renamer(parentWidget(), flags);

  for (const auto* item : items) {
    if (stop) {
      break;
    }

    const auto* ci = dynamic_cast<const ConflictItem*>(item);
    if (!ci) {
      continue;
    }

    auto result = FileRenamer::RESULT_CANCEL;

    if (visible) {
      if (!ci->canUnhide()) {
        qDebug().nospace() << "cannot unhide " << item->text(0) << ", skipping";
        continue;
      }

      result = unhideFile(renamer, ci->fileName());

    } else {
      if (!ci->canHide()) {
        qDebug().nospace() << "cannot hide " << item->text(0) << ", skipping";
        continue;
      }

      result = hideFile(renamer, ci->fileName());
    }

    switch (result) {
      case FileRenamer::RESULT_OK: {
        // will trigger a refresh at the end
        changed = true;
        break;
      }

      case FileRenamer::RESULT_SKIP: {
        // nop
        break;
      }

      case FileRenamer::RESULT_CANCEL: {
        // stop right now, but make sure to refresh if needed
        stop = true;
        break;
      }
    }
  }

  qDebug().nospace() << (visible ? "unhiding" : "hiding") << " conflict files done";

  if (changed) {
    qDebug().nospace() << "triggering refresh";

    if (origin()) {
      emit originModified(origin()->getID());
    }

    update();
  }
}

void ConflictsTab::openItems(const QList<QTreeWidgetItem*>& items)
{
  // the menu item is only shown for a single selection, but handle all of them
  // in case this changes
  for (auto* item : items) {
    if (auto* ci=dynamic_cast<ConflictItem*>(item)) {
      core().executeFileVirtualized(parentWidget(), ci->fileName());
    }
  }
}

void ConflictsTab::previewItems(const QList<QTreeWidgetItem*>& items)
{
  // the menu item is only shown for a single selection, but handle all of them
  // in case this changes
  for (auto* item : items) {
    if (auto* ci=dynamic_cast<ConflictItem*>(item)) {
      core().previewFileWithAlternatives(parentWidget(), ci->fileName());
    }
  }
}

void ConflictsTab::showContextMenu(const QPoint &pos, QTreeWidget* tree)
{
  auto actions = createMenuActions(tree->selectedItems());

  QMenu menu;

  // open
  if (actions.open) {
    connect(actions.open, &QAction::triggered, [&]{
      openItems(tree->selectedItems());
    });

    menu.addAction(actions.open);
  }

  // preview
  if (actions.preview) {
    connect(actions.preview, &QAction::triggered, [&]{
      previewItems(tree->selectedItems());
    });

    menu.addAction(actions.preview);
  }

  // hide
  if (actions.hide) {
    connect(actions.hide, &QAction::triggered, [&]{
      changeItemsVisibility(tree->selectedItems(), false);
    });

    menu.addAction(actions.hide);
  }

  // unhide
  if (actions.unhide) {
    connect(actions.unhide, &QAction::triggered, [&]{
      changeItemsVisibility(tree->selectedItems(), true);
    });

    menu.addAction(actions.unhide);
  }

  // goto
  if (actions.gotoMenu) {
    menu.addMenu(actions.gotoMenu);

    for (auto* a : actions.gotoActions) {
      connect(a, &QAction::triggered, [&, name=a->text()]{
        emitModOpen(name);
       });

      actions.gotoMenu->addAction(a);
    }
  }

  if (!menu.isEmpty()) {
    menu.exec(tree->viewport()->mapToGlobal(pos));
  }
}

ConflictsTab::Actions ConflictsTab::createMenuActions(
  const QList<QTreeWidgetItem*>& selection)
{
  if (selection.empty()) {
    return {};
  }

  bool enableHide = true;
  bool enableUnhide = true;
  bool enableOpen = true;
  bool enablePreview = true;
  bool enableGoto = true;

  if (selection.size() == 1) {
    // this is a single selection
    const auto* ci = dynamic_cast<const ConflictItem*>(selection[0]);
    if (!ci) {
      return {};
    }

    enableHide = ci->canHide();
    enableUnhide = ci->canUnhide();
    enableOpen = ci->canOpen();
    enablePreview = ci->canPreview(plugin());
    enableGoto = ci->hasAlts();
  }
  else {
    // this is a multiple selection, don't show open/preview so users don't open
    // a thousand files
    enableOpen = false;
    enablePreview = false;

    // don't bother with this on multiple selection, at least for now
    enableGoto = false;

    if (selection.size() < max_scan_for_context_menu) {
      // if the number of selected items is low, checking them to accurately
      // show the menu items is worth it
      enableHide = false;
      enableUnhide = false;

      for (const auto* item : selection) {
        if (const auto* ci=dynamic_cast<const ConflictItem*>(item)) {
          if (ci->canHide()) {
            enableHide = true;
          }

          if (ci->canUnhide()) {
            enableUnhide = true;
          }

          if (enableHide && enableUnhide && enableGoto) {
            // found all, no need to check more
            break;
          }
        }
      }
    }
  }

  Actions actions;

  actions.hide = new QAction(tr("Hide"), parentWidget());
  actions.hide->setEnabled(enableHide);

  // note that it is possible for hidden files to appear if they override other
  // hidden files from another mod
  actions.unhide = new QAction(tr("Unhide"), parentWidget());
  actions.unhide->setEnabled(enableUnhide);

  actions.open = new QAction(tr("Open/Execute"), parentWidget());
  actions.open->setEnabled(enableOpen);

  actions.preview = new QAction(tr("Preview"), parentWidget());
  actions.preview->setEnabled(enablePreview);

  actions.gotoMenu = new QMenu(tr("Go to..."), parentWidget());
  actions.gotoMenu->setEnabled(enableGoto);

  if (enableGoto) {
    actions.gotoActions = createGotoActions(selection);
  }

  return actions;
}

std::vector<QAction*> ConflictsTab::createGotoActions(
  const QList<QTreeWidgetItem*>& selection)
{
  if (!origin() || selection.size() != 1) {
    return {};
  }

  const auto* item = dynamic_cast<const ConflictItem*>(selection[0]);
  if (!item) {
    return {};
  }

  auto file = origin()->findFile(item->fileIndex());
  if (!file) {
    return {};
  }


  std::vector<QString> mods;
  const auto& ds = *core().directoryStructure();

  // add all alternatives
  for (const auto& alt : file->getAlternatives()) {
    const auto& o = ds.getOriginByID(alt.first);
    if (o.getID() != origin()->getID()) {
      mods.push_back(ToQString(o.getName()));
    }
  }

  // add the real origin if different from this mod
  const FilesOrigin& realOrigin = ds.getOriginByID(file->getOrigin());
  if (realOrigin.getID() != origin()->getID()) {
    mods.push_back(ToQString(realOrigin.getName()));
  }

  std::sort(mods.begin(), mods.end(), [](const auto& a, const auto& b) {
    return (QString::localeAwareCompare(a, b) < 0);
    });

  std::vector<QAction*> actions;

  for (const auto& name : mods) {
    actions.push_back(new QAction(name, parentWidget()));
  }

  return actions;
}


GeneralConflictsTab::GeneralConflictsTab(
  ConflictsTab* tab, Ui::ModInfoDialog* ui, OrganizerCore& oc)
    : m_tab(tab), ui(ui), m_core(oc)
{
  m_expanders.overwrite.set(ui->overwriteExpander, ui->overwriteTree, true);
  m_expanders.overwritten.set(ui->overwrittenExpander, ui->overwrittenTree, true);
  m_expanders.nonconflict.set(ui->noConflictExpander, ui->noConflictTree);

  QObject::connect(
    ui->overwriteTree, &QTreeWidget::itemDoubleClicked,
    [&](auto* item, int col){ onOverwriteActivated(item, col); });

  QObject::connect(
    ui->overwrittenTree, &QTreeWidget::itemDoubleClicked,
    [&](auto* item, int col){ onOverwrittenActivated(item, col); });

  QObject::connect(
    ui->overwriteTree, &QTreeWidget::customContextMenuRequested,
    [&](const QPoint& p){ m_tab->showContextMenu(p, ui->overwriteTree); });

  QObject::connect(
    ui->overwrittenTree, &QTreeWidget::customContextMenuRequested,
    [&](const QPoint& p){ m_tab->showContextMenu(p, ui->overwrittenTree); });

  QObject::connect(
    ui->noConflictTree, &QTreeWidget::customContextMenuRequested,
    [&](const QPoint& p){ m_tab->showContextMenu(p, ui->noConflictTree); });
}

void GeneralConflictsTab::clear()
{
  ui->overwriteTree->clear();
  ui->overwrittenTree->clear();
  ui->noConflictTree->clear();

  ui->overwriteCount->display(0);
  ui->overwrittenCount->display(0);
  ui->noConflictCount->display(0);
}

void GeneralConflictsTab::saveState(Settings& s)
{
  QByteArray result;
  QDataStream stream(&result, QIODevice::WriteOnly);

  stream
    << m_expanders.overwrite.opened()
    << m_expanders.overwritten.opened()
    << m_expanders.nonconflict.opened();

  s.directInterface().setValue(
    "mod_info_conflicts_general_expanders", result);

  s.directInterface().setValue(
    "mod_info_conflicts_general_overwrite",
    ui->overwriteTree->header()->saveState());

  s.directInterface().setValue(
    "mod_info_conflicts_general_noconflict",
    ui->noConflictTree->header()->saveState());

  s.directInterface().setValue(
    "mod_info_conflicts_general_overwritten",
    ui->overwrittenTree->header()->saveState());
}

void GeneralConflictsTab::restoreState(const Settings& s)
{
  QDataStream stream(s.directInterface()
    .value("mod_info_conflicts_general_expanders").toByteArray());

  bool overwriteExpanded = false;
  bool overwrittenExpanded = false;
  bool noConflictExpanded = false;

  stream >> overwriteExpanded >> overwrittenExpanded >> noConflictExpanded;

  if (stream.status() == QDataStream::Ok) {
    m_expanders.overwrite.toggle(overwriteExpanded);
    m_expanders.overwritten.toggle(overwrittenExpanded);
    m_expanders.nonconflict.toggle(noConflictExpanded);
  }

  ui->overwriteTree->header()->restoreState(s.directInterface()
    .value("mod_info_conflicts_general_overwrite").toByteArray());

  ui->noConflictTree->header()->restoreState(s.directInterface()
    .value("mod_info_conflicts_general_noconflict").toByteArray());

  ui->overwrittenTree->header()->restoreState(s.directInterface()
    .value("mod_info_conflicts_general_overwritten").toByteArray());
}

void GeneralConflictsTab::update()
{
  clear();

  int numNonConflicting = 0;
  int numOverwrite = 0;
  int numOverwritten = 0;

  if (m_tab->origin() != nullptr) {
    const auto rootPath = m_tab->mod()->absolutePath();

    for (const auto& file : m_tab->origin()->getFiles()) {
      const QString relativeName = QDir::fromNativeSeparators(ToQString(file->getRelativePath()));
      const QString fileName = relativeName.mid(0).prepend(rootPath);

      bool archive = false;
      const int fileOrigin = file->getOrigin(archive);
      const auto& alternatives = file->getAlternatives();

      if (fileOrigin == m_tab->origin()->getID()) {
        if (!alternatives.empty()) {
          ui->overwriteTree->addTopLevelItem(createOverwriteItem(
            file->getIndex(), archive, fileName, relativeName, alternatives));

          ++numOverwrite;
        } else {
          // otherwise, put the file in the noconflict tree
          ui->noConflictTree->addTopLevelItem(createNoConflictItem(
            file->getIndex(), archive, fileName, relativeName));

          ++numNonConflicting;
        }
      } else {
        ui->overwrittenTree->addTopLevelItem(createOverwrittenItem(
          file->getIndex(), fileOrigin, archive, fileName, relativeName));

        ++numOverwritten;
      }
    }
  }

  ui->overwriteCount->display(numOverwrite);
  ui->overwrittenCount->display(numOverwritten);
  ui->noConflictCount->display(numNonConflicting);
}

QTreeWidgetItem* GeneralConflictsTab::createOverwriteItem(
  FileEntry::Index index, bool archive,
  const QString& fileName, const QString& relativeName,
  const FileEntry::AlternativesVector& alternatives)
{
  const auto& ds = *m_core.directoryStructure();

  QString altString;

  for (const auto& alt : alternatives) {
    if (!altString.isEmpty()) {
      altString += ", ";
    }

    altString += ToQString(ds.getOriginByID(alt.first).getName());
  }

  QStringList fields(relativeName);
  fields.append(altString);

  const auto origin = ToQString(ds.getOriginByID(alternatives.back().first).getName());

  return new ConflictItem(fields, index, fileName, true, origin, archive);
}

QTreeWidgetItem* GeneralConflictsTab::createNoConflictItem(
  FileEntry::Index index, bool archive,
  const QString& fileName, const QString& relativeName)
{
  return new ConflictItem({relativeName}, index, fileName, false, "", archive);
}

QTreeWidgetItem* GeneralConflictsTab::createOverwrittenItem(
  FileEntry::Index index, int fileOrigin, bool archive,
  const QString& fileName, const QString& relativeName)
{
  const auto& ds = *m_core.directoryStructure();

  const FilesOrigin &realOrigin = ds.getOriginByID(fileOrigin);

  QStringList fields(relativeName);
  fields.append(ToQString(realOrigin.getName()));

  return new ConflictItem(
    fields, index, fileName, true, ToQString(realOrigin.getName()), archive);
}

void GeneralConflictsTab::onOverwriteActivated(QTreeWidgetItem *item, int)
{
  if (auto* ci=dynamic_cast<ConflictItem*>(item)) {
    const auto origin = ci->altOrigin();

    if (!origin.isEmpty()) {
      emit modOpen(origin);
    }
  }
}

void GeneralConflictsTab::onOverwrittenActivated(QTreeWidgetItem *item, int)
{
  if (const auto* ci=dynamic_cast<ConflictItem*>(item)) {
    const auto origin = ci->altOrigin();

    if (!origin.isEmpty()) {
      emit modOpen(origin);
    }
  }
}


AdvancedConflictsTab::AdvancedConflictsTab(
  ConflictsTab* tab, Ui::ModInfoDialog* ui, OrganizerCore& oc)
    : m_tab(tab), ui(ui), m_core(oc)
{
  // left-elide the overwrites column so that the nearest are visible
  ui->conflictsAdvancedList->setItemDelegateForColumn(
    0, new ElideLeftDelegate(ui->conflictsAdvancedList));

  // left-elide the file column to see filenames
  ui->conflictsAdvancedList->setItemDelegateForColumn(
    1, new ElideLeftDelegate(ui->conflictsAdvancedList));

  // don't elide the overwritten by column so that the nearest are visible

  QObject::connect(
    ui->conflictsAdvancedShowNoConflict, &QCheckBox::clicked,
    [&]{ update(); });

  QObject::connect(
    ui->conflictsAdvancedShowAll, &QRadioButton::clicked,
    [&]{ update(); });

  QObject::connect(
    ui->conflictsAdvancedShowNearest, &QRadioButton::clicked,
    [&]{ update(); });

  QObject::connect(
    ui->conflictsAdvancedList, &QTreeWidget::customContextMenuRequested,
    [&](const QPoint& p){ m_tab->showContextMenu(p, ui->conflictsAdvancedList); });

  m_filter.set(ui->conflictsAdvancedFilter);
  m_filter.changed = [&]{ update(); };
}

void AdvancedConflictsTab::clear()
{
  ui->conflictsAdvancedList->clear();
}

void AdvancedConflictsTab::saveState(Settings& s)
{
  s.directInterface().setValue(
    "mod_info_conflicts_advanced_list",
    ui->conflictsAdvancedList->header()->saveState());

  QByteArray result;
  QDataStream stream(&result, QIODevice::WriteOnly);

  stream
    << ui->conflictsAdvancedShowNoConflict->isChecked()
    << ui->conflictsAdvancedShowAll->isChecked()
    << ui->conflictsAdvancedShowNearest->isChecked();

  s.directInterface().setValue(
    "mod_info_conflicts_advanced_options", result);
}

void AdvancedConflictsTab::restoreState(const Settings& s)
{
  ui->conflictsAdvancedList->header()->restoreState(
    s.directInterface().value("mod_info_conflicts_advanced_list").toByteArray());

  QDataStream stream(s.directInterface()
    .value("mod_info_conflicts_advanced_options").toByteArray());

  bool noConflictChecked = false;
  bool showAllChecked = false;
  bool showNearestChecked = false;

  stream >> noConflictChecked >> showAllChecked >> showNearestChecked;

  if (stream.status() == QDataStream::Ok) {
    ui->conflictsAdvancedShowNoConflict->setChecked(noConflictChecked);
    ui->conflictsAdvancedShowAll->setChecked(showAllChecked);
    ui->conflictsAdvancedShowNearest->setChecked(showNearestChecked);
  }
}

void AdvancedConflictsTab::update()
{
  clear();

  if (m_tab->origin() != nullptr) {
    const auto rootPath = m_tab->mod()->absolutePath();

    for (const auto& file : m_tab->origin()->getFiles()) {
      const QString relativeName = QDir::fromNativeSeparators(ToQString(file->getRelativePath()));
      const QString fileName = relativeName.mid(0).prepend(rootPath);

      bool archive = false;
      const int fileOrigin = file->getOrigin(archive);
      const auto& alternatives = file->getAlternatives();

      auto* advancedItem = createItem(
        file->getIndex(), fileOrigin, archive,
        fileName, relativeName, alternatives);

      if (advancedItem) {
        ui->conflictsAdvancedList->addTopLevelItem(advancedItem);
      }
    }
  }
}

QTreeWidgetItem* AdvancedConflictsTab::createItem(
  FileEntry::Index index, int fileOrigin, bool archive,
  const QString& fileName, const QString& relativeName,
  const MOShared::FileEntry::AlternativesVector& alternatives)
{
  const auto& ds = *m_core.directoryStructure();

  QString before, after;

  if (!alternatives.empty()) {
    int beforePrio = 0;
    int afterPrio = std::numeric_limits<int>::max();

    for (const auto& alt : alternatives)
    {
      const auto altOrigin = ds.getOriginByID(alt.first);

      if (ui->conflictsAdvancedShowAll->isChecked()) {
        // fills 'before' and 'after' with all the alternatives that come
        // before and after this mod in terms of priority

        if (altOrigin.getPriority() < m_tab->origin()->getPriority()) {
          // add all the mods having a lower priority than this one
          if (!before.isEmpty()) {
            before += ", ";
          }

          before += ToQString(altOrigin.getName());
        } else if (altOrigin.getPriority() > m_tab->origin()->getPriority()) {
          // add all the mods having a higher priority than this one
          if (!after.isEmpty()) {
            after += ", ";
          }

          after += ToQString(altOrigin.getName());
        }
      } else {
        // keep track of the nearest mods that come before and after this one
        // in terms of priority

        if (altOrigin.getPriority() < m_tab->origin()->getPriority()) {
          // the alternative has a lower priority than this mod

          if (altOrigin.getPriority() > beforePrio) {
            // the alternative has a higher priority and therefore is closer
            // to this mod, use it
            before = ToQString(altOrigin.getName());
            beforePrio = altOrigin.getPriority();
          }
        }

        if (altOrigin.getPriority() > m_tab->origin()->getPriority()) {
          // the alternative has a higher priority than this mod

          if (altOrigin.getPriority() < afterPrio) {
            // the alternative has a lower priority and there is closer
            // to this mod, use it
            after = ToQString(altOrigin.getName());
            afterPrio = altOrigin.getPriority();
          }
        }
      }
    }

    // the primary origin is never in the list of alternatives, so it has to
    // be handled separately
    //
    // if 'after' is not empty, it means at least one alternative with a higher
    // priority than this mod was found; if the user only wants to see the
    // nearest mods, it's not worth checking for the primary origin because it
    // will always have a higher priority than the alternatives (or it wouldn't
    // be the primary)
    if (after.isEmpty() || ui->conflictsAdvancedShowAll->isChecked()) {
      FilesOrigin &realOrigin = ds.getOriginByID(fileOrigin);

      // if no mods overwrite this file, the primary origin is the same as this
      // mod, so ignore that
      if (realOrigin.getID() != m_tab->origin()->getID()) {
        if (!after.isEmpty()) {
          after += ", ";
        }

        after += ToQString(realOrigin.getName());
      }
    }
  }

  bool hasAlts = !before.isEmpty() || !after.isEmpty();

  if (!ui->conflictsAdvancedShowNoConflict->isChecked()) {
    // if both before and after are empty, it means this file has no conflicts
    // at all, only display it if the user wants it
    if (!hasAlts) {
      return nullptr;
    }
  }

  bool matched = m_filter.matches([&](auto&& what) {
    return
      before.contains(what, Qt::CaseInsensitive) ||
      relativeName.contains(what, Qt::CaseInsensitive) ||
      after.contains(what, Qt::CaseInsensitive);
    });

  if (!matched) {
    return nullptr;
  }

  return new ConflictItem(
    {before, relativeName, after}, index, fileName, hasAlts, "", archive);
}

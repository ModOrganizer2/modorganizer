#include "modinfodialogconflicts.h"
#include "ui_modinfodialog.h"
#include "utility.h"

using namespace MOShared;
using namespace MOBase;

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

  bool canPreview(PluginContainer* pluginContainer) const
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


ConflictsTab::ConflictsTab(Ui::ModInfoDialog* ui, OrganizerCore& oc)
  : ui(ui), m_general(ui, oc), m_advanced(ui, oc)
{
}

void ConflictsTab::saveState(Settings& s)
{
  s.directInterface().setValue(
    "mod_info_conflicts_tab", ui->tabConflictsTabs1->currentIndex());

  m_general.saveState(s);
  m_advanced.saveState(s);
}

void ConflictsTab::restoreState(const Settings& s)
{
  ui->tabConflictsTabs1->setCurrentIndex(
    s.directInterface().value("mod_info_conflicts_tab", 0).toInt());

  m_general.restoreState(s);
  m_advanced.restoreState(s);
}


GeneralConflictsTab::GeneralConflictsTab(Ui::ModInfoDialog* ui, OrganizerCore& oc)
  : ui(ui), m_core(oc), m_origin(nullptr)
{
  m_expanders.overwrite.set(ui->overwriteExpander1, ui->overwriteTree1, true);
  m_expanders.overwritten.set(ui->overwrittenExpander1, ui->overwrittenTree1, true);
  m_expanders.nonconflict.set(ui->noConflictExpander1, ui->noConflictTree1);

  QObject::connect(
    ui->overwriteTree1, &QTreeWidget::itemDoubleClicked,
    [&](auto* item, int col){ onOverwriteActivated(item, col); });
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
    ui->overwriteTree1->header()->saveState());

  s.directInterface().setValue(
    "mod_info_conflicts_general_noconflict",
    ui->noConflictTree1->header()->saveState());

  s.directInterface().setValue(
    "mod_info_conflicts_general_overwritten",
    ui->overwrittenTree1->header()->saveState());
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

  ui->overwriteTree1->header()->restoreState(s.directInterface()
    .value("mod_info_conflicts_general_overwrite").toByteArray());

  ui->noConflictTree1->header()->restoreState(s.directInterface()
    .value("mod_info_conflicts_general_noconflict").toByteArray());

  ui->overwrittenTree1->header()->restoreState(s.directInterface()
    .value("mod_info_conflicts_general_overwritten").toByteArray());
}

void GeneralConflictsTab::rebuild(ModInfo::Ptr mod, FilesOrigin* origin)
{
  m_mod = mod;
  m_origin = origin;

  update();
}

void GeneralConflictsTab::update()
{
  int numNonConflicting = 0;
  int numOverwrite = 0;
  int numOverwritten = 0;

  ui->overwriteTree1->clear();
  ui->overwrittenTree1->clear();
  ui->noConflictTree1->clear();

  if (m_origin != nullptr) {
    const auto rootPath = m_mod->absolutePath();

    for (const auto& file : m_origin->getFiles()) {
      const QString relativeName = QDir::fromNativeSeparators(ToQString(file->getRelativePath()));
      const QString fileName = relativeName.mid(0).prepend(rootPath);

      bool archive = false;
      const int fileOrigin = file->getOrigin(archive);
      const auto& alternatives = file->getAlternatives();

      if (fileOrigin == m_origin->getID()) {
        if (!alternatives.empty()) {
          ui->overwriteTree1->addTopLevelItem(createOverwriteItem(
            file->getIndex(), archive, fileName, relativeName, alternatives));

          ++numOverwrite;
        } else {
          // otherwise, put the file in the noconflict tree
          ui->noConflictTree1->addTopLevelItem(createNoConflictItem(
            file->getIndex(), archive, fileName, relativeName));

          ++numNonConflicting;
        }
      } else {
        ui->overwrittenTree1->addTopLevelItem(createOverwrittenItem(
          file->getIndex(), fileOrigin, archive, fileName, relativeName));

        ++numOverwritten;
      }
    }
  }

  ui->overwriteCount1->display(numOverwrite);
  ui->overwrittenCount1->display(numOverwritten);
  ui->noConflictCount1->display(numNonConflicting);
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
      close();
      emit modOpen(origin, TAB_CONFLICTS);
    }
  }
}


AdvancedConflictsTab::AdvancedConflictsTab(Ui::ModInfoDialog* ui, OrganizerCore& oc)
  : ui(ui), m_core(oc), m_origin(nullptr)
{
  // left-elide the overwrites column so that the nearest are visible
  ui->conflictsAdvancedList1->setItemDelegateForColumn(
    0, new ElideLeftDelegate(ui->conflictsAdvancedList1));

  // left-elide the file column to see filenames
  ui->conflictsAdvancedList1->setItemDelegateForColumn(
    1, new ElideLeftDelegate(ui->conflictsAdvancedList1));

  // don't elide the overwritten by column so that the nearest are visible

  QObject::connect(ui->conflictsAdvancedShowNoConflict1, &QCheckBox::clicked, [&] {
    update();
  });

  QObject::connect(ui->conflictsAdvancedShowAll1, &QRadioButton::clicked, [&] {
    update();
  });

  QObject::connect(ui->conflictsAdvancedShowNearest1, &QRadioButton::clicked, [&] {
    update();
  });

  m_filter.set(ui->conflictsAdvancedFilter1);
  m_filter.changed = [&]{ update(); };
}

void AdvancedConflictsTab::saveState(Settings& s)
{
  s.directInterface().setValue(
    "mod_info_conflicts_advanced_list",
    ui->conflictsAdvancedList1->header()->saveState());

  QByteArray result;
  QDataStream stream(&result, QIODevice::WriteOnly);

  stream
    << ui->conflictsAdvancedShowNoConflict1->isChecked()
    << ui->conflictsAdvancedShowAll1->isChecked()
    << ui->conflictsAdvancedShowNearest1->isChecked();

  s.directInterface().setValue(
    "mod_info_conflicts_advanced_options",
    ui->conflictsAdvancedList1->header()->saveState());
}

void AdvancedConflictsTab::restoreState(const Settings& s)
{
  ui->conflictsAdvancedList1->header()->restoreState(
    s.directInterface().value("mod_info_conflicts_advanced_list").toByteArray());

  QDataStream stream(s.directInterface()
    .value("mod_info_conflicts_advanced_options").toByteArray());

  bool noConflictChecked = false;
  bool showAllChecked = false;
  bool showNearestChecked = false;

  stream >> noConflictChecked >> showAllChecked >> showNearestChecked;

  if (stream.status() == QDataStream::Ok) {
    ui->conflictsAdvancedShowNoConflict1->setChecked(noConflictChecked);
    ui->conflictsAdvancedShowAll1->setChecked(showAllChecked);
    ui->conflictsAdvancedShowNearest1->setChecked(showNearestChecked);
  }
}

void AdvancedConflictsTab::rebuild(ModInfo::Ptr mod, MOShared::FilesOrigin* origin)
{
  m_mod = mod;
  m_origin = origin;

  update();
}

void AdvancedConflictsTab::update()
{
  ui->conflictsAdvancedList1->clear();

  if (m_origin != nullptr) {
    const auto rootPath = m_mod->absolutePath();

    for (const auto& file : m_origin->getFiles()) {
      const QString relativeName = QDir::fromNativeSeparators(ToQString(file->getRelativePath()));
      const QString fileName = relativeName.mid(0).prepend(rootPath);

      bool archive = false;
      const int fileOrigin = file->getOrigin(archive);
      const auto& alternatives = file->getAlternatives();

      auto* advancedItem = createItem(
        file->getIndex(), fileOrigin, archive,
        fileName, relativeName, alternatives);

      if (advancedItem) {
        ui->conflictsAdvancedList1->addTopLevelItem(advancedItem);
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

      if (ui->conflictsAdvancedShowAll1->isChecked()) {
        // fills 'before' and 'after' with all the alternatives that come
        // before and after this mod in terms of priority

        if (altOrigin.getPriority() < m_origin->getPriority()) {
          // add all the mods having a lower priority than this one
          if (!before.isEmpty()) {
            before += ", ";
          }

          before += ToQString(altOrigin.getName());
        } else if (altOrigin.getPriority() > m_origin->getPriority()) {
          // add all the mods having a higher priority than this one
          if (!after.isEmpty()) {
            after += ", ";
          }

          after += ToQString(altOrigin.getName());
        }
      } else {
        // keep track of the nearest mods that come before and after this one
        // in terms of priority

        if (altOrigin.getPriority() < m_origin->getPriority()) {
          // the alternative has a lower priority than this mod

          if (altOrigin.getPriority() > beforePrio) {
            // the alternative has a higher priority and therefore is closer
            // to this mod, use it
            before = ToQString(altOrigin.getName());
            beforePrio = altOrigin.getPriority();
          }
        }

        if (altOrigin.getPriority() > m_origin->getPriority()) {
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
    if (after.isEmpty() || ui->conflictsAdvancedShowAll1->isChecked()) {
      FilesOrigin &realOrigin = ds.getOriginByID(fileOrigin);

      // if no mods overwrite this file, the primary origin is the same as this
      // mod, so ignore that
      if (realOrigin.getID() != m_origin->getID()) {
        if (!after.isEmpty()) {
          after += ", ";
        }

        after += ToQString(realOrigin.getName());
      }
    }
  }

  bool hasAlts = !before.isEmpty() || !after.isEmpty();

  if (!ui->conflictsAdvancedShowNoConflict1->isChecked()) {
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

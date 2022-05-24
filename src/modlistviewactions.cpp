#include "modlistviewactions.h"

#include <QGridLayout>
#include <QGroupBox>
#include <QInputDialog>
#include <QLabel>

#include "filesystemutilities.h"
#include <log.h>
#include <report.h>

#include "categories.h"
#include "csvbuilder.h"
#include "directoryrefresher.h"
#include "downloadmanager.h"
#include "filedialogmemory.h"
#include "filterlist.h"
#include "listdialog.h"
#include "messagedialog.h"
#include "modelutils.h"
#include "modinfodialog.h"
#include "modlist.h"
#include "modlistview.h"
#include "nexusinterface.h"
#include "nxmaccessmanager.h"
#include "organizercore.h"
#include "overwriteinfodialog.h"
#include "pluginlistview.h"
#include "savetextasdialog.h"
#include "shared/directoryentry.h"
#include "shared/fileregister.h"
#include "shared/filesorigin.h"

using namespace MOBase;
using namespace MOShared;

ModListViewActions::ModListViewActions(OrganizerCore& core, FilterList& filters,
                                       CategoryFactory& categoryFactory,
                                       ModListView* view, PluginListView* pluginView,
                                       QObject* nxmReceiver)
    : QObject(view), m_core(core), m_filters(filters), m_categories(categoryFactory),
      m_view(view), m_pluginView(pluginView), m_parent(view->topLevelWidget()),
      m_receiver(nxmReceiver)
{}

int ModListViewActions::findInstallPriority(const QModelIndex& index) const
{
  int newPriority = -1;
  if (index.isValid() && index.data(ModList::IndexRole).isValid() &&
      m_view->sortColumn() == ModList::COL_PRIORITY) {
    auto mIndex = index.data(ModList::IndexRole).toInt();
    auto info   = ModInfo::getByIndex(mIndex);
    newPriority = m_core.currentProfile()->getModPriority(mIndex);
    if (info->isSeparator()) {

      auto isSeparator = [](const auto& p) {
        return ModInfo::getByIndex(p.second)->isSeparator();
      };

      auto& ibp = m_core.currentProfile()->getAllIndexesByPriority();

      // start right after/before the current priority and look for the next
      // separator
      if (m_view->sortOrder() == Qt::AscendingOrder) {
        auto it = std::find_if(ibp.find(newPriority + 1), ibp.end(), isSeparator);
        if (it != ibp.end()) {
          newPriority = it->first;
        } else {
          newPriority = -1;
        }
      } else {
        auto it = std::find_if(std::reverse_iterator{ibp.find(newPriority - 1)},
                               ibp.rend(), isSeparator);
        if (it != ibp.rend()) {
          newPriority = it->first + 1;
        } else {
          // create "before" priority 0, i.e. at the end in descending priority.
          newPriority = 0;
        }
      }
    }
  }

  return newPriority;
}

void ModListViewActions::installMod(const QString& archivePath,
                                    const QModelIndex& index) const
{
  try {
    QString path = archivePath;
    if (path.isEmpty()) {
      QStringList extensions = m_core.installationManager()->getSupportedExtensions();
      for (auto iter = extensions.begin(); iter != extensions.end(); ++iter) {
        *iter = "*." + *iter;
      }

      path = FileDialogMemory::getOpenFileName(
          "installMod", m_parent, tr("Choose Mod"), QString(),
          tr("Mod Archive").append(QString(" (%1)").arg(extensions.join(" "))));
    }

    if (path.isEmpty()) {
      return;
    } else {
      m_core.installMod(path, findInstallPriority(index), false, nullptr, QString());
    }
  } catch (const std::exception& e) {
    reportError(e.what());
  }
}

void ModListViewActions::createEmptyMod(const QModelIndex& index) const
{
  GuessedValue<QString> name;
  name.setFilter(&fixDirectoryName);

  while (name->isEmpty()) {
    bool ok;
    name.update(QInputDialog::getText(m_parent, tr("Create Mod..."),
                                      tr("This will create an empty mod.\n"
                                         "Please enter a name:"),
                                      QLineEdit::Normal, "", &ok),
                GUESS_USER);
    if (!ok) {
      return;
    }
  }

  if (m_core.modList()->getMod(name) != nullptr) {
    reportError(tr("A mod with this name already exists"));
    return;
  }

  if (m_core.createMod(name) == nullptr) {
    return;
  }

  // find the priority before refresh() otherwise the index might not be valid
  const int newPriority = findInstallPriority(index);
  m_core.refresh();

  const auto mIndex = ModInfo::getIndex(name);
  if (newPriority >= 0) {
    m_core.modList()->changeModPriority(mIndex, newPriority);
  }

  m_view->scrollToAndSelect(
      m_view->indexModelToView(m_core.modList()->index(mIndex, 0)));
}

void ModListViewActions::createSeparator(const QModelIndex& index) const
{
  GuessedValue<QString> name;
  name.setFilter(&fixDirectoryName);
  while (name->isEmpty()) {
    bool ok;
    name.update(QInputDialog::getText(m_parent, tr("Create Separator..."),
                                      tr("This will create a new separator.\n"
                                         "Please enter a name:"),
                                      QLineEdit::Normal, "", &ok),
                GUESS_USER);
    if (!ok) {
      return;
    }
  }
  if (m_core.modList()->getMod(name) != nullptr) {
    reportError(tr("A separator with this name already exists"));
    return;
  }
  name->append("_separator");
  if (m_core.modList()->getMod(name) != nullptr) {
    return;
  }

  int newPriority = -1;
  if (index.isValid() && m_view->sortColumn() == ModList::COL_PRIORITY) {
    newPriority =
        m_core.currentProfile()->getModPriority(index.data(ModList::IndexRole).toInt());

    // descending order, we need to fix the priority
    if (m_view->sortOrder() == Qt::DescendingOrder) {
      newPriority++;
    }
  }

  if (m_core.createMod(name) == nullptr) {
    return;
  }

  m_core.refresh();

  const auto mIndex = ModInfo::getIndex(name);
  if (newPriority >= 0) {
    m_core.modList()->changeModPriority(mIndex, newPriority);
  }

  if (auto c = m_core.settings().colors().previousSeparatorColor()) {
    ModInfo::getByIndex(mIndex)->setColor(*c);
  }

  m_view->scrollToAndSelect(
      m_view->indexModelToView(m_core.modList()->index(mIndex, 0)));
}

void ModListViewActions::setAllMatchingModsEnabled(bool enabled) const
{
  // number of mods to enable / disable
  const auto counters = m_view->counters();
  const auto count    = enabled ? counters.visible.regular - counters.visible.active
                                : counters.visible.active;

  // retrieve visible mods from the model view
  const auto allIndex = m_view->indexViewToModel(flatIndex(m_view->model()));
  const QString message =
      enabled ? tr("Really enable %1 mod(s)?") : tr("Really disable %1 mod(s)?");
  if (QMessageBox::question(m_parent, tr("Confirm"), message.arg(count),
                            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    m_core.modList()->setActive(allIndex, enabled);
  }
}

void ModListViewActions::checkModsForUpdates() const
{
  bool checkingModsForUpdate = false;
  if (NexusInterface::instance().getAccessManager()->validated()) {
    checkingModsForUpdate =
        ModInfo::checkAllForUpdate(&m_core.pluginManager(), m_receiver);
    NexusInterface::instance().requestEndorsementInfo(m_receiver, QVariant(),
                                                      QString());
    NexusInterface::instance().requestTrackingInfo(m_receiver, QVariant(), QString());
  } else {
    QString apiKey;
    if (GlobalSettings::nexusApiKey(apiKey)) {
      m_core.doAfterLogin([=]() {
        checkModsForUpdates();
      });
      NexusInterface::instance().getAccessManager()->apiCheck(apiKey);
    } else {
      log::warn("{}", tr("You are not currently authenticated with Nexus. Please do so "
                         "under Settings -> Nexus."));
    }
  }

  bool updatesAvailable = false;
  for (auto mod : m_core.modList()->allMods()) {
    ModInfo::Ptr modInfo = ModInfo::getByName(mod);
    if (modInfo->updateAvailable()) {
      updatesAvailable = true;
      break;
    }
  }

  if (updatesAvailable || checkingModsForUpdate) {
    m_view->setFilterCriteria(
        {{ModListSortProxy::TypeSpecial, CategoryFactory::UpdateAvailable, false}});

    m_filters.setSelection(
        {{ModListSortProxy::TypeSpecial, CategoryFactory::UpdateAvailable, false}});
  }
}

void ModListViewActions::assignCategories() const
{
  if (!GlobalSettings::hideAssignCategoriesQuestion()) {
    QMessageBox warning;
    warning.setWindowTitle(tr("Are you sure?"));
    warning.setText(
        tr("This action will remove any existing categories on any mod with a valid "
           "Nexus category mapping. Are you certain you want to proceed?"));
    warning.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
    QCheckBox dontShow(tr("&Don't show this again"));
    warning.setCheckBox(&dontShow);
    auto result = warning.exec();
    if (dontShow.isChecked())
      GlobalSettings::setHideAssignCategoriesQuestion(true);
    if (result == QMessageBox::Cancel)
      return;
  }
  for (auto mod : m_core.modList()->allMods()) {
    ModInfo::Ptr modInfo = ModInfo::getByName(mod);
    if (modInfo->isSeparator())
      continue;
    int nexusCategory = modInfo->getNexusCategory();
    if (!nexusCategory) {
      QSettings downloadMeta(m_core.downloadsPath() + "/" +
                                 modInfo->installationFile() + ".meta",
                             QSettings::IniFormat);
      if (downloadMeta.contains("category")) {
        nexusCategory = downloadMeta.value("category", 0).toInt();
      }
    }
    int newCategory = CategoryFactory::instance().resolveNexusID(nexusCategory);
    if (newCategory != 0) {
      for (auto category : modInfo->categories()) {
        modInfo->removeCategory(category);
      }
    }
    modInfo->setCategory(CategoryFactory::instance().getCategoryID(newCategory), true);
  }
}

void ModListViewActions::checkModsForUpdates(
    std::multimap<QString, int> const& IDs) const
{
  if (m_core.settings().network().offlineMode()) {
    return;
  }

  if (NexusInterface::instance().getAccessManager()->validated()) {
    ModInfo::manualUpdateCheck(m_receiver, IDs);
  } else {
    QString apiKey;
    if (GlobalSettings::nexusApiKey(apiKey)) {
      m_core.doAfterLogin([=]() {
        checkModsForUpdates(IDs);
      });
      NexusInterface::instance().getAccessManager()->apiCheck(apiKey);
    } else
      log::warn("{}", tr("You are not currently authenticated with Nexus. Please do so "
                         "under Settings -> Nexus."));
  }
}

void ModListViewActions::checkModsForUpdates(const QModelIndexList& indices) const
{
  std::multimap<QString, int> ids;
  for (auto& idx : indices) {
    ModInfo::Ptr info = ModInfo::getByIndex(idx.data(ModList::IndexRole).toInt());
    ids.insert(std::make_pair<QString, int>(info->gameName(), info->nexusId()));
  }
  checkModsForUpdates(ids);
}

void ModListViewActions::exportModListCSV() const
{
  QDialog selection(m_parent);
  QGridLayout* grid = new QGridLayout;
  selection.setWindowTitle(tr("Export to csv"));

  QLabel* csvDescription = new QLabel();
  csvDescription->setText(
      tr("CSV (Comma Separated Values) is a format that can be imported in programs "
         "like Excel to create a spreadsheet.\nYou can also use online editors and "
         "converters instead."));
  grid->addWidget(csvDescription);

  QGroupBox* groupBoxRows = new QGroupBox(tr("Select what mods you want export:"));
  QRadioButton* all       = new QRadioButton(tr("All installed mods"));
  QRadioButton* active =
      new QRadioButton(tr("Only active (checked) mods from your current profile"));
  QRadioButton* visible =
      new QRadioButton(tr("All currently visible mods in the mod list"));

  QVBoxLayout* vbox = new QVBoxLayout;
  vbox->addWidget(all);
  vbox->addWidget(active);
  vbox->addWidget(visible);
  vbox->addStretch(1);
  groupBoxRows->setLayout(vbox);

  grid->addWidget(groupBoxRows);

  QButtonGroup* buttonGroupRows = new QButtonGroup();
  buttonGroupRows->addButton(all, 0);
  buttonGroupRows->addButton(active, 1);
  buttonGroupRows->addButton(visible, 2);
  buttonGroupRows->button(0)->setChecked(true);

  QGroupBox* groupBoxColumns = new QGroupBox(tr("Choose what Columns to export:"));
  groupBoxColumns->setFlat(true);

  QCheckBox* mod_Priority = new QCheckBox(tr("Mod_Priority"));
  mod_Priority->setChecked(true);
  QCheckBox* mod_Name = new QCheckBox(tr("Mod_Name"));
  mod_Name->setChecked(true);
  QCheckBox* mod_Note   = new QCheckBox(tr("Notes_column"));
  QCheckBox* mod_Status = new QCheckBox(tr("Mod_Status"));
  mod_Status->setChecked(true);
  QCheckBox* primary_Category   = new QCheckBox(tr("Primary_Category"));
  QCheckBox* nexus_ID           = new QCheckBox(tr("Nexus_ID"));
  QCheckBox* mod_Nexus_URL      = new QCheckBox(tr("Mod_Nexus_URL"));
  QCheckBox* mod_Version        = new QCheckBox(tr("Mod_Version"));
  QCheckBox* install_Date       = new QCheckBox(tr("Install_Date"));
  QCheckBox* download_File_Name = new QCheckBox(tr("Download_File_Name"));

  QVBoxLayout* vbox1 = new QVBoxLayout;
  vbox1->addWidget(mod_Priority);
  vbox1->addWidget(mod_Name);
  vbox1->addWidget(mod_Status);
  vbox1->addWidget(mod_Note);
  vbox1->addWidget(primary_Category);
  vbox1->addWidget(nexus_ID);
  vbox1->addWidget(mod_Nexus_URL);
  vbox1->addWidget(mod_Version);
  vbox1->addWidget(install_Date);
  vbox1->addWidget(download_File_Name);
  groupBoxColumns->setLayout(vbox1);

  grid->addWidget(groupBoxColumns);

  QPushButton* ok     = new QPushButton("Ok");
  QPushButton* cancel = new QPushButton("Cancel");
  QDialogButtonBox* buttons =
      new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

  connect(buttons, SIGNAL(accepted()), &selection, SLOT(accept()));
  connect(buttons, SIGNAL(rejected()), &selection, SLOT(reject()));

  grid->addWidget(buttons);

  selection.setLayout(grid);

  if (selection.exec() == QDialog::Accepted) {

    unsigned int numMods = ModInfo::getNumMods();
    int selectedRowID    = buttonGroupRows->checkedId();

    try {
      QBuffer buffer;
      buffer.open(QIODevice::ReadWrite);
      CSVBuilder builder(&buffer);
      builder.setEscapeMode(CSVBuilder::TYPE_STRING, CSVBuilder::QUOTE_ALWAYS);
      std::vector<std::pair<QString, CSVBuilder::EFieldType>> fields;
      if (mod_Priority->isChecked())
        fields.push_back(
            std::make_pair(QString("#Mod_Priority"), CSVBuilder::TYPE_STRING));
      if (mod_Status->isChecked())
        fields.push_back(
            std::make_pair(QString("#Mod_Status"), CSVBuilder::TYPE_STRING));
      if (mod_Name->isChecked())
        fields.push_back(std::make_pair(QString("#Mod_Name"), CSVBuilder::TYPE_STRING));
      if (mod_Note->isChecked())
        fields.push_back(std::make_pair(QString("#Note"), CSVBuilder::TYPE_STRING));
      if (primary_Category->isChecked())
        fields.push_back(
            std::make_pair(QString("#Primary_Category"), CSVBuilder::TYPE_STRING));
      if (nexus_ID->isChecked())
        fields.push_back(
            std::make_pair(QString("#Nexus_ID"), CSVBuilder::TYPE_INTEGER));
      if (mod_Nexus_URL->isChecked())
        fields.push_back(
            std::make_pair(QString("#Mod_Nexus_URL"), CSVBuilder::TYPE_STRING));
      if (mod_Version->isChecked())
        fields.push_back(
            std::make_pair(QString("#Mod_Version"), CSVBuilder::TYPE_STRING));
      if (install_Date->isChecked())
        fields.push_back(
            std::make_pair(QString("#Install_Date"), CSVBuilder::TYPE_STRING));
      if (download_File_Name->isChecked())
        fields.push_back(
            std::make_pair(QString("#Download_File_Name"), CSVBuilder::TYPE_STRING));

      builder.setFields(fields);

      builder.writeHeader();

      auto indexesByPriority = m_core.currentProfile()->getAllIndexesByPriority();
      for (auto& iter : indexesByPriority) {
        ModInfo::Ptr info = ModInfo::getByIndex(iter.second);
        bool enabled      = m_core.currentProfile()->modEnabled(iter.second);
        if ((selectedRowID == 1) && !enabled) {
          continue;
        } else if ((selectedRowID == 2) && !m_view->isModVisible(iter.second)) {
          continue;
        }
        std::vector<ModInfo::EFlag> flags = info->getFlags();
        if ((std::find(flags.begin(), flags.end(), ModInfo::FLAG_OVERWRITE) ==
             flags.end()) &&
            (std::find(flags.begin(), flags.end(), ModInfo::FLAG_BACKUP) ==
             flags.end())) {
          if (mod_Priority->isChecked())
            builder.setRowField("#Mod_Priority",
                                QString("%1").arg(iter.first, 4, 10, QChar('0')));
          if (mod_Status->isChecked())
            builder.setRowField("#Mod_Status", (enabled) ? "+" : "-");
          if (mod_Name->isChecked())
            builder.setRowField("#Mod_Name", info->name());
          if (mod_Note->isChecked())
            builder.setRowField("#Note",
                                QString("%1").arg(info->comments().remove(',')));
          if (primary_Category->isChecked())
            builder.setRowField(
                "#Primary_Category",
                (m_categories.categoryExists(info->primaryCategory()))
                    ? m_categories.getCategoryNameByID(info->primaryCategory())
                    : "");
          if (nexus_ID->isChecked())
            builder.setRowField("#Nexus_ID", info->nexusId());
          if (mod_Nexus_URL->isChecked())
            builder.setRowField("#Mod_Nexus_URL",
                                (info->nexusId() > 0)
                                    ? NexusInterface::instance().getModURL(
                                          info->nexusId(), info->gameName())
                                    : "");
          if (mod_Version->isChecked())
            builder.setRowField("#Mod_Version", info->version().canonicalString());
          if (install_Date->isChecked())
            builder.setRowField("#Install_Date",
                                info->creationTime().toString("yyyy/MM/dd HH:mm:ss"));
          if (download_File_Name->isChecked())
            builder.setRowField("#Download_File_Name", info->installationFile());

          builder.writeRow();
        }
      }

      SaveTextAsDialog saveDialog(m_parent);
      saveDialog.setText(buffer.data());
      saveDialog.exec();
    } catch (const std::exception& e) {
      reportError(tr("export failed: %1").arg(e.what()));
    }
  }
}

void ModListViewActions::displayModInformation(const QString& modName,
                                               ModInfoTabIDs tab) const
{
  unsigned int index = ModInfo::getIndex(modName);
  if (index == UINT_MAX) {
    log::error("failed to resolve mod name {}", modName);
    return;
  }

  ModInfo::Ptr modInfo = ModInfo::getByIndex(index);
  displayModInformation(modInfo, index, tab);
}

void ModListViewActions::displayModInformation(unsigned int index,
                                               ModInfoTabIDs tab) const
{
  ModInfo::Ptr modInfo = ModInfo::getByIndex(index);
  displayModInformation(modInfo, index, tab);
}

void ModListViewActions::displayModInformation(ModInfo::Ptr modInfo,
                                               unsigned int modIndex,
                                               ModInfoTabIDs tab) const
{
  if (!m_core.modList()->modInfoAboutToChange(modInfo)) {
    log::debug("a different mod information dialog is open. If this is incorrect, "
               "please restart MO");
    return;
  }
  std::vector<ModInfo::EFlag> flags = modInfo->getFlags();
  if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_OVERWRITE) != flags.end()) {
    QDialog* dialog = m_parent->findChild<QDialog*>("__overwriteDialog");
    try {
      if (dialog == nullptr) {
        dialog = new OverwriteInfoDialog(modInfo, m_parent);
        dialog->setObjectName("__overwriteDialog");
      } else {
        qobject_cast<OverwriteInfoDialog*>(dialog)->setModInfo(modInfo);
      }

      dialog->show();
      dialog->raise();
      dialog->activateWindow();
      connect(dialog, &QDialog::finished, [=]() {
        m_core.modList()->modInfoChanged(modInfo);
        dialog->deleteLater();
        m_core.refreshDirectoryStructure();
      });
    } catch (const std::exception& e) {
      reportError(tr("Failed to display overwrite dialog: %1").arg(e.what()));
    }
  } else {
    modInfo->saveMeta();

    ModInfoDialog dialog(m_core, m_core.pluginManager(), modInfo, m_view, m_parent);
    connect(&dialog, &ModInfoDialog::originModified, this,
            &ModListViewActions::originModified);
    connect(&dialog, &ModInfoDialog::modChanged, [=](unsigned int index) {
      auto idx = m_view->indexModelToView(m_core.modList()->index(index, 0));
      m_view->selectionModel()->select(idx, QItemSelectionModel::ClearAndSelect |
                                                QItemSelectionModel::Rows);
      m_view->scrollTo(idx);
    });

    // Open the tab first if we want to use the standard indexes of the tabs.
    if (tab != ModInfoTabIDs::None) {
      dialog.selectTab(tab);
    }

    dialog.exec();

    modInfo->saveMeta();
    m_core.modList()->modInfoChanged(modInfo);
    emit modInfoDisplayed();
  }

  if (m_core.currentProfile()->modEnabled(modIndex) && !modInfo->isForeign()) {
    FilesOrigin& origin =
        m_core.directoryStructure()->getOriginByName(ToWString(modInfo->name()));
    origin.enable(false);

    if (m_core.directoryStructure()->originExists(ToWString(modInfo->name()))) {
      FilesOrigin& origin =
          m_core.directoryStructure()->getOriginByName(ToWString(modInfo->name()));
      origin.enable(false);

      m_core.directoryRefresher()->addModToStructure(
          m_core.directoryStructure(), modInfo->name(),
          m_core.currentProfile()->getModPriority(modIndex), modInfo->absolutePath(),
          modInfo->stealFiles(), modInfo->archives());
      DirectoryRefresher::cleanStructure(m_core.directoryStructure());
      m_core.directoryStructure()->getFileRegister()->sortOrigins();
      m_core.refreshLists();
    }
  }
}

void ModListViewActions::sendModsToTop(const QModelIndexList& indexes) const
{
  m_core.modList()->changeModsPriority(indexes, Profile::MinimumPriority);
}

void ModListViewActions::sendModsToBottom(const QModelIndexList& indexes) const
{
  m_core.modList()->changeModsPriority(indexes, Profile::MaximumPriority);
}

void ModListViewActions::sendModsToPriority(const QModelIndexList& indexes) const
{
  bool ok;
  int priority = QInputDialog::getInt(m_parent, tr("Set Priority"),
                                      tr("Set the priority of the selected mods"), 0, 0,
                                      std::numeric_limits<int>::max(), 1, &ok);
  if (!ok)
    return;

  m_core.modList()->changeModsPriority(indexes, priority);
}

void ModListViewActions::sendModsToSeparator(const QModelIndexList& indexes) const
{
  QStringList separators;
  const auto& ibp = m_core.currentProfile()->getAllIndexesByPriority();
  for (const auto& [priority, index] : ibp) {
    if (index < ModInfo::getNumMods()) {
      ModInfo::Ptr modInfo = ModInfo::getByIndex(index);
      if (modInfo->isSeparator()) {
        separators << modInfo->name().chopped(
            10);  // chops the "_separator" away from the name
      }
    }
  }

  // in descending order, reverse the separator
  if (m_view->sortOrder() == Qt::DescendingOrder) {
    std::reverse(separators.begin(), separators.end());
  }

  ListDialog dialog(m_parent);
  dialog.setWindowTitle("Select a separator...");
  dialog.setChoices(separators);

  if (dialog.exec() != QDialog::Accepted) {
    return;
  }

  const QString result = dialog.getChoice();
  if (result.isEmpty()) {
    return;
  }

  const auto sepPriority =
      m_core.currentProfile()->getModPriority(ModInfo::getIndex(result + "_separator"));

  auto isSeparator = [](const auto& p) {
    return ModInfo::getByIndex(p.second)->isSeparator();
  };

  // start right after/before the current priority and look for the next
  // separator
  int priority = -1;
  if (m_view->sortOrder() == Qt::AscendingOrder) {
    auto it = std::find_if(ibp.find(sepPriority + 1), ibp.end(), isSeparator);
    if (it != ibp.end()) {
      priority = it->first;
    } else {
      priority = Profile::MaximumPriority;
    }
  } else {
    auto it = std::find_if(--std::reverse_iterator{ibp.find(sepPriority - 1)},
                           ibp.rend(), isSeparator);
    if (it != ibp.rend()) {
      priority = it->first + 1;
    } else {
      // create "before" priority 0, i.e. at the end in descending priority.
      priority = Profile::MinimumPriority;
    }
  }

  // when the priority of a single mod is incremented, we need to shift the
  // target priority, otherwise we will miss the target by one
  if (indexes.size() == 1 &&
      indexes[0].data(ModList::PriorityRole).toInt() < sepPriority) {
    priority--;
  }

  m_core.modList()->changeModsPriority(indexes, priority);
}

void ModListViewActions::sendModsToFirstConflict(const QModelIndexList& indexes) const
{
  std::set<unsigned int> conflicts;

  for (auto& idx : indexes) {
    if (!idx.data(ModList::IndexRole).isValid()) {
      continue;
    }
    auto info = ModInfo::getByIndex(idx.data(ModList::IndexRole).toInt());
    conflicts.insert(info->getModOverwrite().begin(), info->getModOverwrite().end());
  }

  std::set<int> priorities;
  std::transform(conflicts.begin(), conflicts.end(),
                 std::inserter(priorities, priorities.end()), [=](auto index) {
                   return m_core.currentProfile()->getModPriority(index);
                 });

  if (!priorities.empty()) {
    m_core.modList()->changeModsPriority(indexes, *priorities.begin());
  }
}

void ModListViewActions::sendModsToLastConflict(const QModelIndexList& indexes) const
{
  std::set<unsigned int> conflicts;

  for (auto& idx : indexes) {
    if (!idx.data(ModList::IndexRole).isValid()) {
      continue;
    }
    auto info = ModInfo::getByIndex(idx.data(ModList::IndexRole).toInt());
    conflicts.insert(info->getModOverwritten().begin(),
                     info->getModOverwritten().end());
  }

  std::set<int> priorities;
  std::transform(conflicts.begin(), conflicts.end(),
                 std::inserter(priorities, priorities.end()), [=](auto index) {
                   return m_core.currentProfile()->getModPriority(index);
                 });

  if (!priorities.empty()) {
    m_core.modList()->changeModsPriority(indexes, *priorities.rbegin());
  }
}

void ModListViewActions::renameMod(const QModelIndex& index) const
{
  try {
    m_view->edit(m_view->indexModelToView(index));
  } catch (const std::exception& e) {
    reportError(tr("failed to rename mod: %1").arg(e.what()));
  }
}

void ModListViewActions::removeMods(const QModelIndexList& indices) const
{
  const int max_items = 20;

  try {
    if (indices.size() > 1) {
      QString mods;
      QStringList modNames;

      int i = 0;
      for (auto& idx : indices) {
        QString name = idx.data().toString();
        if (!ModInfo::getByIndex(idx.data(ModList::IndexRole).toInt())->isRegular()) {
          continue;
        }

        // adds an item for the mod name until `i` reaches `max_items`, which
        // adds one "..." item; subsequent mods are not shown on the list but
        // are still added to `modNames` below so they can be removed correctly

        if (i < max_items) {
          mods += "<li>" + name + "</li>";
        } else if (i == max_items) {
          mods += "<li>...</li>";
        }

        modNames.append(
            ModInfo::getByIndex(idx.data(ModList::IndexRole).toInt())->name());
        ++i;
      }
      if (QMessageBox::question(
              m_parent, tr("Confirm"),
              tr("Remove the following mods?<br><ul>%1</ul>").arg(mods),
              QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        // use mod names instead of indexes because those become invalid during the
        // removal
        DownloadManager::startDisableDirWatcher();
        for (QString name : modNames) {
          m_core.modList()->removeRowForce(ModInfo::getIndex(name), QModelIndex());
        }
        DownloadManager::endDisableDirWatcher();
      }
    } else if (!indices.isEmpty()) {
      m_core.modList()->removeRow(indices[0].data(ModList::IndexRole).toInt(),
                                  QModelIndex());
    }
    m_view->updateModCount();
    m_pluginView->updatePluginCount();
  } catch (const std::exception& e) {
    reportError(tr("failed to remove mod: %1").arg(e.what()));
  }
}

void ModListViewActions::ignoreMissingData(const QModelIndexList& indices) const
{
  for (auto& idx : indices) {
    int row_idx       = idx.data(ModList::IndexRole).toInt();
    ModInfo::Ptr info = ModInfo::getByIndex(row_idx);
    info->markValidated(true);
    m_core.modList()->notifyChange(row_idx);
  }
}

void ModListViewActions::setIgnoreUpdate(const QModelIndexList& indices,
                                         bool ignore) const
{
  for (auto& idx : indices) {
    int modIdx        = idx.data(ModList::IndexRole).toInt();
    ModInfo::Ptr info = ModInfo::getByIndex(modIdx);
    info->ignoreUpdate(ignore);
    m_core.modList()->notifyChange(modIdx);
  }
}

void ModListViewActions::changeVersioningScheme(const QModelIndex& index) const
{
  if (QMessageBox::question(
          m_parent, tr("Continue?"),
          tr("The versioning scheme decides which version is considered newer than "
             "another.\n"
             "This function will guess the versioning scheme under the assumption that "
             "the installed version is outdated."),
          QMessageBox::Yes | QMessageBox::Cancel) == QMessageBox::Yes) {

    ModInfo::Ptr info = ModInfo::getByIndex(index.data(ModList::IndexRole).toInt());

    bool success = false;

    static VersionInfo::VersionScheme schemes[] = {
        VersionInfo::SCHEME_REGULAR, VersionInfo::SCHEME_DECIMALMARK,
        VersionInfo::SCHEME_NUMBERSANDLETTERS};

    for (int i = 0;
         i < sizeof(schemes) / sizeof(VersionInfo::VersionScheme) && !success; ++i) {
      VersionInfo verOld(info->version().canonicalString(), schemes[i]);
      VersionInfo verNew(info->newestVersion().canonicalString(), schemes[i]);
      if (verOld < verNew) {
        info->setVersion(verOld);
        info->setNewestVersion(verNew);
        success = true;
      }
    }
    if (!success) {
      QMessageBox::information(
          m_parent, tr("Sorry"),
          tr("I don't know a versioning scheme where %1 is newer than %2.")
              .arg(info->newestVersion().canonicalString())
              .arg(info->version().canonicalString()),
          QMessageBox::Ok);
    }
  }
}

void ModListViewActions::markConverted(const QModelIndexList& indices) const
{
  for (auto& idx : indices) {
    int modIdx        = idx.data(ModList::IndexRole).toInt();
    ModInfo::Ptr info = ModInfo::getByIndex(modIdx);
    info->markConverted(true);
    m_core.modList()->notifyChange(modIdx);
  }
}

void ModListViewActions::visitOnNexus(const QModelIndexList& indices) const
{
  if (indices.size() > 10) {
    if (QMessageBox::question(m_parent, tr("Opening Nexus Links"),
                              tr("You are trying to open %1 links to Nexus Mods.  Are "
                                 "you sure you want to do this?")
                                  .arg(indices.size()),
                              QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
      return;
    }
  }

  for (auto& idx : indices) {
    ModInfo::Ptr info = ModInfo::getByIndex(idx.data(ModList::IndexRole).toInt());
    int modID         = info->nexusId();
    QString gameName  = info->gameName();
    if (modID > 0) {
      shell::Open(QUrl(NexusInterface::instance().getModURL(modID, gameName)));
    } else {
      log::error("mod '{}' has no nexus id", info->name());
    }
  }
}

void ModListViewActions::visitWebPage(const QModelIndexList& indices) const
{
  if (indices.size() > 10) {
    if (QMessageBox::question(m_parent, tr("Opening Web Pages"),
                              tr("You are trying to open %1 Web Pages.  Are you sure "
                                 "you want to do this?")
                                  .arg(indices.size()),
                              QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
      return;
    }
  }

  for (auto& idx : indices) {
    ModInfo::Ptr info = ModInfo::getByIndex(idx.data(ModList::IndexRole).toInt());

    const auto url = info->parseCustomURL();
    if (url.isValid()) {
      shell::Open(url);
    }
  }
}

void ModListViewActions::visitNexusOrWebPage(const QModelIndexList& indices) const
{
  if (indices.size() > 10) {
    if (QMessageBox::question(m_parent, tr("Opening Web Pages"),
                              tr("You are trying to open %1 Web Pages.  Are you sure "
                                 "you want to do this?")
                                  .arg(indices.size()),
                              QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
      return;
    }
  }

  for (auto& idx : indices) {
    ModInfo::Ptr info = ModInfo::getByIndex(idx.data(ModList::IndexRole).toInt());
    if (!info) {
      log::error("mod {} not found", idx.data(ModList::IndexRole).toInt());
      continue;
    }

    int modID        = info->nexusId();
    QString gameName = info->gameName();
    const auto url   = info->parseCustomURL();

    if (modID > 0) {
      shell::Open(QUrl(NexusInterface::instance().getModURL(modID, gameName)));
    } else if (url.isValid()) {
      shell::Open(url);
    } else {
      log::error("mod '{}' has no valid link", info->name());
    }
  }
}

void ModListViewActions::reinstallMod(const QModelIndex& index) const
{
  ModInfo::Ptr modInfo = ModInfo::getByIndex(index.data(ModList::IndexRole).toInt());
  QString installationFile = modInfo->installationFile();
  if (installationFile.length() != 0) {
    QString fullInstallationFile;
    QFileInfo fileInfo(installationFile);
    if (fileInfo.isAbsolute()) {
      if (fileInfo.exists()) {
        fullInstallationFile = installationFile;
      } else {
        fullInstallationFile =
            m_core.downloadManager()->getOutputDirectory() + "/" + fileInfo.fileName();
      }
    } else {
      fullInstallationFile =
          m_core.downloadManager()->getOutputDirectory() + "/" + installationFile;
    }
    if (QFile::exists(fullInstallationFile)) {
      m_core.installMod(fullInstallationFile, -1, true, modInfo, modInfo->name());
    } else {
      QMessageBox::information(m_parent, tr("Failed"),
                               tr("Installation file no longer exists"));
    }
  } else {
    QMessageBox::information(
        m_parent, tr("Failed"),
        tr("Mods installed with old versions of MO can't be reinstalled in this way."));
  }
}

void ModListViewActions::createBackup(const QModelIndex& index) const
{
  ModInfo::Ptr modInfo = ModInfo::getByIndex(index.data(ModList::IndexRole).toInt());
  QString backupDirectory =
      m_core.installationManager()->generateBackupName(modInfo->absolutePath());
  if (!copyDir(modInfo->absolutePath(), backupDirectory, false)) {
    QMessageBox::information(m_parent, tr("Failed"), tr("Failed to create backup."));
  }
  m_core.refresh();
  m_view->updateModCount();
}

void ModListViewActions::restoreHiddenFiles(const QModelIndexList& indices) const
{
  const int max_items = 20;

  QFlags<FileRenamer::RenameFlags> flags = FileRenamer::UNHIDE;
  flags |= FileRenamer::MULTIPLE;

  FileRenamer renamer(m_parent, flags);

  FileRenamer::RenameResults result = FileRenamer::RESULT_OK;

  // multi selection
  if (indices.size() > 1) {

    QStringList modNames;
    for (auto& idx : indices) {

      ModInfo::Ptr modInfo = ModInfo::getByIndex(idx.data(ModList::IndexRole).toInt());
      const auto flags     = modInfo->getFlags();

      if (!modInfo->isRegular() ||
          std::find(flags.begin(), flags.end(), ModInfo::FLAG_HIDDEN_FILES) ==
              flags.end()) {
        continue;
      }

      modNames.append(idx.data(Qt::DisplayRole).toString());
    }

    QString mods = "<li>" + modNames.mid(0, max_items).join("</li><li>") + "</li>";
    if (modNames.size() > max_items) {
      mods += "<li>...</li>";
    }

    if (QMessageBox::question(
            m_parent, tr("Confirm"),
            tr("Restore all hidden files in the following mods?<br><ul>%1</ul>")
                .arg(mods),
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {

      for (auto& idx : indices) {

        ModInfo::Ptr modInfo =
            ModInfo::getByIndex(idx.data(ModList::IndexRole).toInt());

        const auto flags = modInfo->getFlags();
        if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_HIDDEN_FILES) !=
            flags.end()) {
          const QString modDir = modInfo->absolutePath();

          auto partialResult = restoreHiddenFilesRecursive(renamer, modDir);

          if (partialResult == FileRenamer::RESULT_CANCEL) {
            result = FileRenamer::RESULT_CANCEL;
            break;
          }
          emit originModified((m_core.directoryStructure()->getOriginByName(
                                   ToWString(modInfo->internalName())))
                                  .getID());
        }
      }
    }
  } else if (!indices.isEmpty()) {
    // single selection
    ModInfo::Ptr modInfo =
        ModInfo::getByIndex(indices[0].data(ModList::IndexRole).toInt());
    const QString modDir = modInfo->absolutePath();

    if (QMessageBox::question(
            m_parent, tr("Are you sure?"),
            tr("About to restore all hidden files in:\n") + modInfo->name(),
            QMessageBox::Ok | QMessageBox::Cancel) == QMessageBox::Ok) {

      result = restoreHiddenFilesRecursive(renamer, modDir);

      emit originModified((m_core.directoryStructure()->getOriginByName(
                               ToWString(modInfo->internalName())))
                              .getID());
    }
  }

  if (result == FileRenamer::RESULT_CANCEL) {
    log::debug("Restoring hidden files operation cancelled");
  } else {
    log::debug("Finished restoring hidden files");
  }
}

void ModListViewActions::setTracked(const QModelIndexList& indices, bool tracked) const
{
  m_core.loggedInAction(m_parent, [=] {
    for (auto& idx : indices) {
      ModInfo::getByIndex(idx.data(ModList::IndexRole).toInt())->track(tracked);
    }
  });
}

void ModListViewActions::setEndorsed(const QModelIndexList& indices,
                                     bool endorsed) const
{
  m_core.loggedInAction(m_parent, [=] {
    if (indices.size() > 1) {
      MessageDialog::showMessage(
          tr("Endorsing multiple mods will take a while. Please wait..."), m_parent);
    }

    for (auto& idx : indices) {
      ModInfo::getByIndex(idx.data(ModList::IndexRole).toInt())->endorse(endorsed);
    }
  });
}

void ModListViewActions::willNotEndorsed(const QModelIndexList& indices) const
{
  for (auto& idx : indices) {
    ModInfo::getByIndex(idx.data(ModList::IndexRole).toInt())->setNeverEndorse();
  }
}

void ModListViewActions::remapCategory(const QModelIndexList& indices) const
{
  for (auto& idx : indices) {
    ModInfo::Ptr modInfo = ModInfo::getByIndex(idx.data(ModList::IndexRole).toInt());
    if (modInfo->isSeparator())
      continue;

    int categoryID = modInfo->getNexusCategory();
    if (!categoryID) {
      QSettings downloadMeta(m_core.downloadsPath() + "/" +
                                 modInfo->installationFile() + ".meta",
                             QSettings::IniFormat);
      if (downloadMeta.contains("category")) {
        categoryID = downloadMeta.value("category", 0).toInt();
      }
    }
    unsigned int categoryIndex = CategoryFactory::instance().resolveNexusID(categoryID);
    if (categoryIndex != 0)
      modInfo->setPrimaryCategory(
          CategoryFactory::instance().getCategoryID(categoryIndex));
  }
}

void ModListViewActions::setColor(const QModelIndexList& indices,
                                  const QModelIndex& refIndex) const
{
  auto& settings       = m_core.settings();
  ModInfo::Ptr modInfo = ModInfo::getByIndex(refIndex.data(ModList::IndexRole).toInt());

  QColorDialog dialog(m_parent);
  dialog.setOption(QColorDialog::ShowAlphaChannel);

  QColor currentColor = modInfo->color();
  if (currentColor.isValid()) {
    dialog.setCurrentColor(currentColor);
  } else if (auto c = settings.colors().previousSeparatorColor()) {
    dialog.setCurrentColor(*c);
  }

  if (!dialog.exec())
    return;

  currentColor = dialog.currentColor();
  if (!currentColor.isValid())
    return;

  settings.colors().setPreviousSeparatorColor(currentColor);

  for (auto& idx : indices) {
    ModInfo::Ptr info = ModInfo::getByIndex(idx.data(ModList::IndexRole).toInt());
    info->setColor(currentColor);
  }
}

void ModListViewActions::resetColor(const QModelIndexList& indices) const
{
  for (auto& idx : indices) {
    ModInfo::Ptr info = ModInfo::getByIndex(idx.data(ModList::IndexRole).toInt());
    info->setColor(QColor());
  }
  m_core.settings().colors().removePreviousSeparatorColor();
}

void ModListViewActions::setCategories(
    ModInfo::Ptr mod, const std::vector<std::pair<int, bool>>& categories) const
{
  for (auto& [id, enabled] : categories) {
    mod->setCategory(id, enabled);
  }
}

void ModListViewActions::setCategoriesIf(
    ModInfo::Ptr mod, ModInfo::Ptr ref,
    const std::vector<std::pair<int, bool>>& categories) const
{
  for (auto& [id, enabled] : categories) {
    if (ref->categorySet(id) != enabled) {
      mod->setCategory(id, enabled);
    }
  }
}

void ModListViewActions::setCategories(
    const QModelIndexList& selected, const QModelIndex& ref,
    const std::vector<std::pair<int, bool>>& categories) const
{
  ModInfo::Ptr refMod = ModInfo::getByIndex(ref.data(ModList::IndexRole).toInt());
  if (selected.size() > 1) {
    for (auto& idx : selected) {
      if (idx.row() != ref.row()) {
        setCategoriesIf(ModInfo::getByIndex(idx.data(ModList::IndexRole).toInt()),
                        refMod, categories);
      }
    }
    setCategories(refMod, categories);
  } else if (!selected.isEmpty()) {
    // for single mod selections, just do a replace
    setCategories(refMod, categories);
  }

  for (auto& idx : selected) {
    m_core.modList()->notifyChange(idx.data(ModList::IndexRole).toInt());
  }

  // reset the selection manually - still needed
  auto viewIndices = m_view->indexModelToView(selected);
  for (auto& idx : viewIndices) {
    m_view->selectionModel()->select(idx, QItemSelectionModel::Select |
                                              QItemSelectionModel::Rows);
  }
}

void ModListViewActions::setPrimaryCategory(const QModelIndexList& selected,
                                            int category, bool force)
{
  for (auto& idx : selected) {
    ModInfo::Ptr info = ModInfo::getByIndex(idx.data(ModList::IndexRole).toInt());
    if (force || info->categorySet(category)) {
      info->setCategory(category, true);
      info->setPrimaryCategory(category);
    }
  }

  // reset the selection manually - still needed
  auto viewIndices = m_view->indexModelToView(selected);
  for (auto& idx : viewIndices) {
    m_view->selectionModel()->select(idx, QItemSelectionModel::Select |
                                              QItemSelectionModel::Rows);
  }
}

void ModListViewActions::openExplorer(const QModelIndexList& index) const
{
  for (auto& idx : index) {
    ModInfo::Ptr info = ModInfo::getByIndex(idx.data(ModList::IndexRole).toInt());
    if (!info->isForeign()) {
      shell::Explore(info->absolutePath());
    }
  }
}

void ModListViewActions::restoreBackup(const QModelIndex& index) const
{
  QRegularExpression backupRegEx("(.*)_backup[0-9]*$");
  ModInfo::Ptr modInfo = ModInfo::getByIndex(index.data(ModList::IndexRole).toInt());
  auto match           = backupRegEx.match(modInfo->name());
  if (match.hasMatch()) {
    QString regName = match.captured(1);
    QDir modDir(QDir::fromNativeSeparators(m_core.settings().paths().mods()));
    if (!modDir.exists(regName) ||
        (QMessageBox::question(
             m_parent, tr("Overwrite?"),
             tr("This will replace the existing mod \"%1\". Continue?").arg(regName),
             QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)) {
      if (modDir.exists(regName) &&
          !shellDelete(QStringList(modDir.absoluteFilePath(regName)))) {
        reportError(tr("failed to remove mod \"%1\"").arg(regName));
      } else {
        QString destinationPath =
            QDir::fromNativeSeparators(m_core.settings().paths().mods()) + "/" +
            regName;
        if (!modDir.rename(modInfo->absolutePath(), destinationPath)) {
          reportError(tr("failed to rename \"%1\" to \"%2\"")
                          .arg(modInfo->absolutePath())
                          .arg(destinationPath));
        }
        m_core.refresh();
        m_view->updateModCount();
      }
    }
  }
}

void ModListViewActions::moveOverwriteContentsTo(const QString& absolutePath) const
{
  ModInfo::Ptr overwriteInfo = ModInfo::getOverwrite();
  bool successful =
      shellMove((QDir::toNativeSeparators(overwriteInfo->absolutePath()) + "\\*"),
                (QDir::toNativeSeparators(absolutePath)), false, m_parent);

  if (successful) {
    MessageDialog::showMessage(tr("Move successful."), m_parent);
  } else {
    const auto e = GetLastError();
    log::error("Move operation failed: {}", formatSystemMessage(e));
  }

  m_core.refresh();
}

void ModListViewActions::createModFromOverwrite() const
{
  GuessedValue<QString> name;
  name.setFilter(&fixDirectoryName);

  while (name->isEmpty()) {
    bool ok;
    name.update(
        QInputDialog::getText(
            m_parent, tr("Create Mod..."),
            tr("This will move all files from overwrite into a new, regular mod.\n"
               "Please enter a name:"),
            QLineEdit::Normal, "", &ok),
        GUESS_USER);
    if (!ok) {
      return;
    }
  }

  if (m_core.modList()->getMod(name) != nullptr) {
    reportError(tr("A mod with this name already exists"));
    return;
  }

  const IModInterface* newMod = m_core.createMod(name);
  if (newMod == nullptr) {
    return;
  }

  moveOverwriteContentsTo(newMod->absolutePath());
}

void ModListViewActions::moveOverwriteContentToExistingMod() const
{
  QStringList mods;
  auto indexesByPriority = m_core.currentProfile()->getAllIndexesByPriority();
  for (auto& iter : indexesByPriority) {
    if ((iter.second != UINT_MAX)) {
      ModInfo::Ptr modInfo = ModInfo::getByIndex(iter.second);
      if (!modInfo->isSeparator() && !modInfo->isForeign() && !modInfo->isOverwrite()) {
        mods << modInfo->name();
      }
    }
  }

  ListDialog dialog(m_parent);
  dialog.setWindowTitle("Select a mod...");
  dialog.setChoices(mods);

  if (dialog.exec() == QDialog::Accepted) {
    QString result = dialog.getChoice();
    if (!result.isEmpty()) {

      QString modAbsolutePath;

      for (const auto& mod : m_core.modList()->allModsByProfilePriority()) {
        if (result.compare(mod) == 0) {
          ModInfo::Ptr modInfo = ModInfo::getByIndex(ModInfo::getIndex(mod));
          modAbsolutePath      = modInfo->absolutePath();
          break;
        }
      }

      if (modAbsolutePath.isNull()) {
        log::warn("Mod {} has not been found, for some reason", result);
        return;
      }

      moveOverwriteContentsTo(modAbsolutePath);
    }
  }
}

void ModListViewActions::clearOverwrite() const
{
  ModInfo::Ptr modInfo = ModInfo::getOverwrite();
  if (modInfo) {
    QDir overwriteDir(modInfo->absolutePath());
    if (QMessageBox::question(
            m_parent, tr("Are you sure?"),
            tr("About to recursively delete:\n") + overwriteDir.absolutePath(),
            QMessageBox::Ok | QMessageBox::Cancel) == QMessageBox::Ok) {
      QStringList delList;
      for (auto f :
           overwriteDir.entryList(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot))
        delList.push_back(overwriteDir.absoluteFilePath(f));
      if (shellDelete(delList, true)) {
        emit overwriteCleared();
        m_core.refresh();
      } else {
        const auto e = GetLastError();
        log::error("Delete operation failed: {}", formatSystemMessage(e));
      }
    }
  }
}

#include "modlistviewactions.h"

#include <QGridLayout>
#include <QGroupBox>
#include <QInputDialog>
#include <QLabel>

#include <log.h>
#include <report.h>

#include "categories.h"
#include "filedialogmemory.h"
#include "filterlist.h"
#include "listdialog.h"
#include "modinfodialog.h"
#include "modlist.h"
#include "modlistview.h"
#include "mainwindow.h"
#include "nexusinterface.h"
#include "nxmaccessmanager.h"
#include "savetextasdialog.h"
#include "organizercore.h"
#include "overwriteinfodialog.h"
#include "csvbuilder.h"
#include "shared/filesorigin.h"
#include "shared/directoryentry.h"
#include "shared/fileregister.h"
#include "directoryrefresher.h"

using namespace MOBase;
using namespace MOShared;

ModListViewActions::ModListViewActions(
  OrganizerCore& core, FilterList& filters, CategoryFactory& categoryFactory, MainWindow* mainWindow, ModListView* view) :
  QObject(view)
  , m_core(core)
  , m_filters(filters)
  , m_categories(categoryFactory)
  , m_main(mainWindow)
  , m_view(view)
{

}

void ModListViewActions::installMod(const QString& archivePath) const
{
  try {
    QString path = archivePath;
    if (path.isEmpty()) {
      QStringList extensions = m_core.installationManager()->getSupportedExtensions();
      for (auto iter = extensions.begin(); iter != extensions.end(); ++iter) {
        *iter = "*." + *iter;
      }

      path = FileDialogMemory::getOpenFileName("installMod", m_view, tr("Choose Mod"), QString(),
        tr("Mod Archive").append(QString(" (%1)").arg(extensions.join(" "))));
    }

    if (path.isEmpty()) {
      return;
    }
    else {
      m_core.installMod(path, false, nullptr, QString());
    }
  }
  catch (const std::exception& e) {
    reportError(e.what());
  }
}

void ModListViewActions::createEmptyMod(int modIndex) const
{
  GuessedValue<QString> name;
  name.setFilter(&fixDirectoryName);

  while (name->isEmpty()) {
    bool ok;
    name.update(QInputDialog::getText(m_view, tr("Create Mod..."),
      tr("This will create an empty mod.\n"
        "Please enter a name:"), QLineEdit::Normal, "", &ok),
      GUESS_USER);
    if (!ok) {
      return;
    }
  }

  if (m_core.modList()->getMod(name) != nullptr) {
    reportError(tr("A mod with this name already exists"));
    return;
  }

  int newPriority = -1;
  if (modIndex >= 0 && m_view->sortColumn() == ModList::COL_PRIORITY) {
    newPriority = m_core.currentProfile()->getModPriority(modIndex);
  }

  IModInterface* newMod = m_core.createMod(name);
  if (newMod == nullptr) {
    return;
  }

  m_core.refresh();

  if (newPriority >= 0) {
    m_core.modList()->changeModPriority(ModInfo::getIndex(name), newPriority);
  }
}

void ModListViewActions::createSeparator(int modIndex) const
{
  GuessedValue<QString> name;
  name.setFilter(&fixDirectoryName);
  while (name->isEmpty())
  {
    bool ok;
    name.update(QInputDialog::getText(m_view, tr("Create Separator..."),
      tr("This will create a new separator.\n"
        "Please enter a name:"), QLineEdit::Normal, "", &ok),
      GUESS_USER);
    if (!ok) { return; }
  }
  if (m_core.modList()->getMod(name) != nullptr)
  {
    reportError(tr("A separator with this name already exists"));
    return;
  }
  name->append("_separator");
  if (m_core.modList()->getMod(name) != nullptr)
  {
    return;
  }

  int newPriority = -1;
  if (modIndex >= 0 && m_view->sortColumn() == ModList::COL_PRIORITY)
  {
    newPriority = m_core.currentProfile()->getModPriority(modIndex);
  }

  if (m_core.createMod(name) == nullptr) { return; }
  m_core.refresh();

  if (newPriority >= 0)
  {
    m_core.modList()->changeModPriority(ModInfo::getIndex(name), newPriority);
  }

  if (auto c = m_core.settings().colors().previousSeparatorColor()) {
    ModInfo::getByIndex(ModInfo::getIndex(name))->setColor(*c);
  }
}

void ModListViewActions::checkModsForUpdates() const
{
  bool checkingModsForUpdate = false;
  if (NexusInterface::instance().getAccessManager()->validated()) {
    checkingModsForUpdate = ModInfo::checkAllForUpdate(&m_core.pluginContainer(), m_main);
    NexusInterface::instance().requestEndorsementInfo(m_main, QVariant(), QString());
    NexusInterface::instance().requestTrackingInfo(m_main, QVariant(), QString());
  } else {
    QString apiKey;
    if (GlobalSettings::nexusApiKey(apiKey)) {
      m_core.doAfterLogin([=] () { checkModsForUpdates(); });
      NexusInterface::instance().getAccessManager()->apiCheck(apiKey);
    } else {
      log::warn("{}", tr("You are not currently authenticated with Nexus. Please do so under Settings -> Nexus."));
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
    m_view->setFilterCriteria({{
        ModListSortProxy::TypeSpecial,
        CategoryFactory::UpdateAvailable,
        false}
    });

    m_filters.setSelection({{
      ModListSortProxy::TypeSpecial,
      CategoryFactory::UpdateAvailable,
      false
    }});
  }
}

void ModListViewActions::exportModListCSV() const
{
  QDialog selection(m_view);
  QGridLayout* grid = new QGridLayout;
  selection.setWindowTitle(tr("Export to csv"));

  QLabel* csvDescription = new QLabel();
  csvDescription->setText(tr("CSV (Comma Separated Values) is a format that can be imported in programs like Excel to create a spreadsheet.\nYou can also use online editors and converters instead."));
  grid->addWidget(csvDescription);

  QGroupBox* groupBoxRows = new QGroupBox(tr("Select what mods you want export:"));
  QRadioButton* all = new QRadioButton(tr("All installed mods"));
  QRadioButton* active = new QRadioButton(tr("Only active (checked) mods from your current profile"));
  QRadioButton* visible = new QRadioButton(tr("All currently visible mods in the mod list"));

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
  QCheckBox* mod_Note = new QCheckBox(tr("Notes_column"));
  QCheckBox* mod_Status = new QCheckBox(tr("Mod_Status"));
  mod_Status->setChecked(true);
  QCheckBox* primary_Category = new QCheckBox(tr("Primary_Category"));
  QCheckBox* nexus_ID = new QCheckBox(tr("Nexus_ID"));
  QCheckBox* mod_Nexus_URL = new QCheckBox(tr("Mod_Nexus_URL"));
  QCheckBox* mod_Version = new QCheckBox(tr("Mod_Version"));
  QCheckBox* install_Date = new QCheckBox(tr("Install_Date"));
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

  QPushButton* ok = new QPushButton("Ok");
  QPushButton* cancel = new QPushButton("Cancel");
  QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

  connect(buttons, SIGNAL(accepted()), &selection, SLOT(accept()));
  connect(buttons, SIGNAL(rejected()), &selection, SLOT(reject()));

  grid->addWidget(buttons);

  selection.setLayout(grid);


  if (selection.exec() == QDialog::Accepted) {

    unsigned int numMods = ModInfo::getNumMods();
    int selectedRowID = buttonGroupRows->checkedId();

    try {
      QBuffer buffer;
      buffer.open(QIODevice::ReadWrite);
      CSVBuilder builder(&buffer);
      builder.setEscapeMode(CSVBuilder::TYPE_STRING, CSVBuilder::QUOTE_ALWAYS);
      std::vector<std::pair<QString, CSVBuilder::EFieldType> > fields;
      if (mod_Priority->isChecked())
        fields.push_back(std::make_pair(QString("#Mod_Priority"), CSVBuilder::TYPE_STRING));
      if (mod_Status->isChecked())
        fields.push_back(std::make_pair(QString("#Mod_Status"), CSVBuilder::TYPE_STRING));
      if (mod_Name->isChecked())
        fields.push_back(std::make_pair(QString("#Mod_Name"), CSVBuilder::TYPE_STRING));
      if (mod_Note->isChecked())
        fields.push_back(std::make_pair(QString("#Note"), CSVBuilder::TYPE_STRING));
      if (primary_Category->isChecked())
        fields.push_back(std::make_pair(QString("#Primary_Category"), CSVBuilder::TYPE_STRING));
      if (nexus_ID->isChecked())
        fields.push_back(std::make_pair(QString("#Nexus_ID"), CSVBuilder::TYPE_INTEGER));
      if (mod_Nexus_URL->isChecked())
        fields.push_back(std::make_pair(QString("#Mod_Nexus_URL"), CSVBuilder::TYPE_STRING));
      if (mod_Version->isChecked())
        fields.push_back(std::make_pair(QString("#Mod_Version"), CSVBuilder::TYPE_STRING));
      if (install_Date->isChecked())
        fields.push_back(std::make_pair(QString("#Install_Date"), CSVBuilder::TYPE_STRING));
      if (download_File_Name->isChecked())
        fields.push_back(std::make_pair(QString("#Download_File_Name"), CSVBuilder::TYPE_STRING));

      builder.setFields(fields);

      builder.writeHeader();

      auto indexesByPriority = m_core.currentProfile()->getAllIndexesByPriority();
      for (auto& iter : indexesByPriority) {
        ModInfo::Ptr info = ModInfo::getByIndex(iter.second);
        bool enabled = m_core.currentProfile()->modEnabled(iter.second);
        if ((selectedRowID == 1) && !enabled) {
          continue;
        }
        else if ((selectedRowID == 2) && !m_view->isModVisible(iter.second)) {
          continue;
        }
        std::vector<ModInfo::EFlag> flags = info->getFlags();
        if ((std::find(flags.begin(), flags.end(), ModInfo::FLAG_OVERWRITE) == flags.end()) &&
          (std::find(flags.begin(), flags.end(), ModInfo::FLAG_BACKUP) == flags.end())) {
          if (mod_Priority->isChecked())
            builder.setRowField("#Mod_Priority", QString("%1").arg(iter.first, 4, 10, QChar('0')));
          if (mod_Status->isChecked())
            builder.setRowField("#Mod_Status", (enabled) ? "+" : "-");
          if (mod_Name->isChecked())
            builder.setRowField("#Mod_Name", info->name());
          if (mod_Note->isChecked())
            builder.setRowField("#Note", QString("%1").arg(info->comments().remove(',')));
          if (primary_Category->isChecked())
            builder.setRowField("#Primary_Category", (m_categories.categoryExists(info->primaryCategory())) ? m_categories.getCategoryNameByID(info->primaryCategory()) : "");
          if (nexus_ID->isChecked())
            builder.setRowField("#Nexus_ID", info->nexusId());
          if (mod_Nexus_URL->isChecked())
            builder.setRowField("#Mod_Nexus_URL", (info->nexusId() > 0) ? NexusInterface::instance().getModURL(info->nexusId(), info->gameName()) : "");
          if (mod_Version->isChecked())
            builder.setRowField("#Mod_Version", info->version().canonicalString());
          if (install_Date->isChecked())
            builder.setRowField("#Install_Date", info->creationTime().toString("yyyy/MM/dd HH:mm:ss"));
          if (download_File_Name->isChecked())
            builder.setRowField("#Download_File_Name", info->installationFile());

          builder.writeRow();
        }
      }

      SaveTextAsDialog saveDialog(m_view);
      saveDialog.setText(buffer.data());
      saveDialog.exec();
    }
    catch (const std::exception& e) {
      reportError(tr("export failed: %1").arg(e.what()));
    }
  }
}

void ModListViewActions::displayModInformation(const QString& modName, ModInfoTabIDs tab) const
{
  unsigned int index = ModInfo::getIndex(modName);
  if (index == UINT_MAX) {
    log::error("failed to resolve mod name {}", modName);
    return;
  }

  ModInfo::Ptr modInfo = ModInfo::getByIndex(index);
  displayModInformation(modInfo, index, tab);
}

void ModListViewActions::displayModInformation(unsigned int index, ModInfoTabIDs tab) const
{
  ModInfo::Ptr modInfo = ModInfo::getByIndex(index);
  displayModInformation(modInfo, index, tab);
}

void ModListViewActions::displayModInformation(ModInfo::Ptr modInfo, unsigned int modIndex, ModInfoTabIDs tab) const
{
  if (!m_core.modList()->modInfoAboutToChange(modInfo)) {
    log::debug("a different mod information dialog is open. If this is incorrect, please restart MO");
    return;
  }
  std::vector<ModInfo::EFlag> flags = modInfo->getFlags();
  if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_OVERWRITE) != flags.end()) {
    QDialog* dialog = m_main->findChild<QDialog*>("__overwriteDialog");
    try {
      if (dialog == nullptr) {
        dialog = new OverwriteInfoDialog(modInfo, m_main);
        dialog->setObjectName("__overwriteDialog");
      }
      else {
        qobject_cast<OverwriteInfoDialog*>(dialog)->setModInfo(modInfo);
      }

      dialog->show();
      dialog->raise();
      dialog->activateWindow();
      connect(dialog, SIGNAL(finished(int)), this, SLOT(overwriteClosed(int)));
    }
    catch (const std::exception& e) {
      reportError(tr("Failed to display overwrite dialog: %1").arg(e.what()));
    }
  }
  else {
    modInfo->saveMeta();

    ModInfoDialog dialog(m_main, &m_core, &m_core.pluginContainer(), modInfo);
    connect(&dialog, SIGNAL(originModified(int)), this, SLOT(originModified(int)));

    //Open the tab first if we want to use the standard indexes of the tabs.
    if (tab != ModInfoTabIDs::None) {
      dialog.selectTab(tab);
    }

    dialog.exec();

    modInfo->saveMeta();
    m_core.modList()->modInfoChanged(modInfo);
  }

  if (m_core.currentProfile()->modEnabled(modIndex)
    && !modInfo->hasFlag(ModInfo::FLAG_FOREIGN)) {
    FilesOrigin& origin = m_core.directoryStructure()->getOriginByName(ToWString(modInfo->name()));
    origin.enable(false);

    if (m_core.directoryStructure()->originExists(ToWString(modInfo->name()))) {
      FilesOrigin& origin = m_core.directoryStructure()->getOriginByName(ToWString(modInfo->name()));
      origin.enable(false);

      m_core.directoryRefresher()->addModToStructure(m_core.directoryStructure()
        , modInfo->name()
        , m_core.currentProfile()->getModPriority(modIndex)
        , modInfo->absolutePath()
        , modInfo->stealFiles()
        , modInfo->archives());
      DirectoryRefresher::cleanStructure(m_core.directoryStructure());
      m_core.directoryStructure()->getFileRegister()->sortOrigins();
      m_core.refreshLists();
    }
  }
}

void ModListViewActions::sendModsToTop(const QModelIndexList& index) const
{
  m_core.modList()->changeModsPriority(index, 0);
}

void ModListViewActions::sendModsToBottom(const QModelIndexList& index) const
{
  m_core.modList()->changeModsPriority(index, std::numeric_limits<int>::max());
}

void ModListViewActions::sendModsToPriority(const QModelIndexList& index) const
{
  bool ok;
  int priority = QInputDialog::getInt(m_view,
    tr("Set Priority"), tr("Set the priority of the selected mods"),
    0, 0, std::numeric_limits<int>::max(), 1, &ok);
  if (!ok) return;

  m_core.modList()->changeModsPriority(index, priority);
}

void ModListViewActions::sendModsToSeparator(const QModelIndexList& index) const
{
  QStringList separators;
  auto indexesByPriority = m_core.currentProfile()->getAllIndexesByPriority();
  for (auto iter = indexesByPriority.begin(); iter != indexesByPriority.end(); iter++) {
    if ((iter->second != UINT_MAX)) {
      ModInfo::Ptr modInfo = ModInfo::getByIndex(iter->second);
      if (modInfo->hasFlag(ModInfo::FLAG_SEPARATOR)) {
        separators << modInfo->name().chopped(10);  // Chops the "_separator" away from the name
      }
    }
  }

  ListDialog dialog(m_view);
  dialog.setWindowTitle("Select a separator...");
  dialog.setChoices(separators);

  if (dialog.exec() == QDialog::Accepted) {
    QString result = dialog.getChoice();
    if (!result.isEmpty()) {
      result += "_separator";

      int newPriority = std::numeric_limits<int>::max();
      bool foundSection = false;
      for (auto mod : m_core.modsSortedByProfilePriority(m_core.currentProfile())) {
        unsigned int modIndex = ModInfo::getIndex(mod);
        ModInfo::Ptr modInfo = ModInfo::getByIndex(modIndex);
        if (!foundSection && result.compare(mod) == 0) {
          foundSection = true;
        }
        else if (foundSection && modInfo->isSeparator()) {
          newPriority = m_core.currentProfile()->getModPriority(modIndex);
          break;
        }
      }

      if (index.size() == 1
        && m_core.currentProfile()->getModPriority(index[0].data(ModList::IndexRole).toInt()) < newPriority) {
        --newPriority;
      }

      m_core.modList()->changeModsPriority(index, newPriority);
    }
  }
}

#include "modlistviewactions.h"

#include <QGridLayout>
#include <QGroupBox>
#include <QInputDialog>
#include <QLabel>

#include <report.h>

#include "categories.h"
#include "filedialogmemory.h"
#include "filterlist.h"
#include "modlist.h"
#include "modlistview.h"
#include "nexusinterface.h"
#include "nxmaccessmanager.h"
#include "savetextasdialog.h"
#include "organizercore.h"
#include "csvbuilder.h"

using namespace MOBase;

ModListViewActions::ModListViewActions(
  OrganizerCore& core, FilterList& filters, CategoryFactory& categoryFactory, QObject* nxmReceiver, ModListView* view) :
  QObject(view)
  , m_core(core)
  , m_filters(filters)
  , m_categories(categoryFactory)
  , m_receiver(nxmReceiver)
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
    checkingModsForUpdate = ModInfo::checkAllForUpdate(&m_core.pluginContainer(), m_receiver);
    NexusInterface::instance().requestEndorsementInfo(m_receiver, QVariant(), QString());
    NexusInterface::instance().requestTrackingInfo(m_receiver, QVariant(), QString());
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

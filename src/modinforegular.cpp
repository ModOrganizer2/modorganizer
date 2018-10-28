#include "modinforegular.h"

#include "categories.h"
#include "messagedialog.h"
#include "report.h"
#include "scriptextender.h"

#include <QApplication>
#include <QDirIterator>
#include <QSettings>

#include <sstream>

using namespace MOBase;
using namespace MOShared;

namespace {
  //Arguably this should be a class static or we should be using FileString rather
  //than QString for the names. Or both.
  static bool ByName(const ModInfo::Ptr &LHS, const ModInfo::Ptr &RHS)
  {
    return QString::compare(LHS->name(), RHS->name(), Qt::CaseInsensitive) < 0;
  }
}

ModInfoRegular::ModInfoRegular(PluginContainer *pluginContainer, const IPluginGame *game, const QDir &path, DirectoryEntry **directoryStructure)
  : ModInfoWithConflictInfo(pluginContainer, directoryStructure)
  , m_Name(path.dirName())
  , m_Path(path.absolutePath())
  , m_Repository()
  , m_GameName(game->gameShortName())
  , m_IsAlternate(false)
  , m_Converted(false)
  , m_Validated(false)
  , m_MetaInfoChanged(false)
  , m_EndorsedState(ENDORSED_UNKNOWN)
  , m_NexusBridge(pluginContainer)
{
  testValid();
  m_CreationTime = QFileInfo(path.absolutePath()).created();
  // read out the meta-file for information
  readMeta();
  if (m_GameName.compare(game->gameShortName(), Qt::CaseInsensitive) != 0)
    if (!game->primarySources().contains(m_GameName, Qt::CaseInsensitive))
      m_IsAlternate = true;

  connect(&m_NexusBridge, SIGNAL(descriptionAvailable(QString,int,QVariant,QVariant))
          , this, SLOT(nxmDescriptionAvailable(QString,int,QVariant,QVariant)));
  connect(&m_NexusBridge, SIGNAL(endorsementToggled(QString,int,QVariant,QVariant))
          , this, SLOT(nxmEndorsementToggled(QString,int,QVariant,QVariant)));
  connect(&m_NexusBridge, SIGNAL(requestFailed(QString,int,int,QVariant,QString))
          , this, SLOT(nxmRequestFailed(QString,int,int,QVariant,QString)));
}


ModInfoRegular::~ModInfoRegular()
{
  try {
    saveMeta();
  } catch (const std::exception &e) {
    qCritical("failed to save meta information for \"%s\": %s",
              m_Name.toUtf8().constData(), e.what());
  }
}

bool ModInfoRegular::isEmpty() const
{
  QDirIterator iter(m_Path, QDir::NoDotAndDotDot | QDir::Files | QDir::Dirs);
  if (!iter.hasNext()) return true;
  iter.next();
  if ((iter.fileName() == "meta.ini") && !iter.hasNext()) return true;
  return false;
}


void ModInfoRegular::readMeta()
{
  QSettings metaFile(m_Path + "/meta.ini", QSettings::IniFormat);
  m_Notes            = metaFile.value("notes", "").toString();
  QString tempGameName = metaFile.value("gameName", m_GameName).toString();
  if (tempGameName.size()) m_GameName = tempGameName;
  m_NexusID          = metaFile.value("modid", -1).toInt();
  m_Version.parse(metaFile.value("version", "").toString());
  m_NewestVersion    = metaFile.value("newestVersion", "").toString();
  m_IgnoredVersion   = metaFile.value("ignoredVersion", "").toString();
  m_InstallationFile = metaFile.value("installationFile", "").toString();
  m_NexusDescription = metaFile.value("nexusDescription", "").toString();
  m_Repository       = metaFile.value("repository", "Nexus").toString();
  m_Converted        = metaFile.value("converted", false).toBool();
  m_Validated        = metaFile.value("validated", false).toBool();
  m_URL              = metaFile.value("url", "").toString();
  m_LastNexusQuery   = QDateTime::fromString(metaFile.value("lastNexusQuery", "").toString(), Qt::ISODate);
  m_Color            = metaFile.value("color",QColor()).value<QColor>();
  if (metaFile.contains("endorsed")) {
    if (metaFile.value("endorsed").canConvert<int>()) {
      switch (metaFile.value("endorsed").toInt()) {
        case ENDORSED_FALSE: m_EndorsedState = ENDORSED_FALSE; break;
        case ENDORSED_TRUE:  m_EndorsedState = ENDORSED_TRUE;  break;
        case ENDORSED_NEVER: m_EndorsedState = ENDORSED_NEVER; break;
        default: m_EndorsedState = ENDORSED_UNKNOWN; break;
      }
    } else {
      m_EndorsedState = metaFile.value("endorsed", false).toBool() ? ENDORSED_TRUE : ENDORSED_FALSE;
    }
  }

  QString categoriesString = metaFile.value("category", "").toString();

  QStringList categories = categoriesString.split(',', QString::SkipEmptyParts);
  for (QStringList::iterator iter = categories.begin(); iter != categories.end(); ++iter) {
    bool ok = false;
    int categoryID = iter->toInt(&ok);
    if (categoryID < 0) {
      // ignore invalid id
      continue;
    }
    if (ok && (categoryID != 0) && (CategoryFactory::instance().categoryExists(categoryID))) {
      m_Categories.insert(categoryID);
      if (iter == categories.begin()) {
        m_PrimaryCategory = categoryID;
      }
    }
  }

  int numFiles = metaFile.beginReadArray("installedFiles");
  for (int i = 0; i < numFiles; ++i) {
    metaFile.setArrayIndex(i);
    m_InstalledFileIDs.insert(std::make_pair(metaFile.value("modid").toInt(), metaFile.value("fileid").toInt()));
  }
  metaFile.endArray();

  m_MetaInfoChanged = false;
}

void ModInfoRegular::saveMeta()
{
  // only write meta data if the mod directory exists
  if (m_MetaInfoChanged && QFile::exists(absolutePath())) {
    QSettings metaFile(absolutePath().append("/meta.ini"), QSettings::IniFormat);
    if (metaFile.status() == QSettings::NoError) {
      std::set<int> temp = m_Categories;
      temp.erase(m_PrimaryCategory);
      metaFile.setValue("category", QString("%1").arg(m_PrimaryCategory) + "," + SetJoin(temp, ","));
      metaFile.setValue("newestVersion", m_NewestVersion.canonicalString());
      metaFile.setValue("ignoredVersion", m_IgnoredVersion.canonicalString());
      metaFile.setValue("version", m_Version.canonicalString());
      metaFile.setValue("installationFile", m_InstallationFile);
      metaFile.setValue("repository", m_Repository);
      metaFile.setValue("gameName", m_GameName);
      metaFile.setValue("modid", m_NexusID);
      metaFile.setValue("notes", m_Notes);
      metaFile.setValue("nexusDescription", m_NexusDescription);
      metaFile.setValue("url", m_URL);
      metaFile.setValue("lastNexusQuery", m_LastNexusQuery.toString(Qt::ISODate));
      metaFile.setValue("converted", m_Converted);
      metaFile.setValue("validated", m_Validated);
      metaFile.setValue("color", m_Color);
      if (m_EndorsedState != ENDORSED_UNKNOWN) {
        metaFile.setValue("endorsed", m_EndorsedState);
      }

      metaFile.beginWriteArray("installedFiles");
      int idx = 0;
      for (auto iter = m_InstalledFileIDs.begin(); iter != m_InstalledFileIDs.end(); ++iter) {
        metaFile.setArrayIndex(idx++);
        metaFile.setValue("modid", iter->first);
        metaFile.setValue("fileid", iter->second);
      }
      metaFile.endArray();

      metaFile.sync(); // sync needs to be called to ensure the file is created

      if (metaFile.status() == QSettings::NoError) {
        m_MetaInfoChanged = false;
      } else {
        reportError(tr("failed to write %1/meta.ini: error %2").arg(absolutePath()).arg(metaFile.status()));
      }
    } else {
      reportError(tr("failed to write %1/meta.ini: error %2").arg(absolutePath()).arg(metaFile.status()));
    }
  }
}


bool ModInfoRegular::updateAvailable() const
{
  if (m_IgnoredVersion.isValid() && (m_IgnoredVersion == m_NewestVersion)) {
    return false;
  }
  return m_NewestVersion.isValid() && (m_Version < m_NewestVersion);
}


bool ModInfoRegular::downgradeAvailable() const
{
  if (m_IgnoredVersion.isValid() && (m_IgnoredVersion == m_NewestVersion)) {
    return false;
  }
  return m_NewestVersion.isValid() && (m_NewestVersion < m_Version);
}


void ModInfoRegular::nxmDescriptionAvailable(QString, int, QVariant, QVariant resultData)
{
  QVariantMap result = resultData.toMap();
  setNewestVersion(VersionInfo(result["version"].toString()));
  setNexusDescription(result["description"].toString());

  if ((m_EndorsedState != ENDORSED_NEVER) && (result.contains("voted_by_user"))) {
    setEndorsedState(result["voted_by_user"].toBool() ? ENDORSED_TRUE : ENDORSED_FALSE);
  }
  m_LastNexusQuery = QDateTime::currentDateTime();
  //m_MetaInfoChanged = true;
  saveMeta();
  emit modDetailsUpdated(true);
}


void ModInfoRegular::nxmEndorsementToggled(QString, int, QVariant, QVariant resultData)
{
  m_EndorsedState = resultData.toBool() ? ENDORSED_TRUE : ENDORSED_FALSE;
  m_MetaInfoChanged = true;
  saveMeta();
  emit modDetailsUpdated(true);
}


void ModInfoRegular::nxmRequestFailed(QString, int, int, QVariant userData, const QString &errorMessage)
{
  QString fullMessage = errorMessage;
  if (userData.canConvert<int>() && (userData.toInt() == 1)) {
    fullMessage += "\nNexus will reject endorsements within 15 Minutes of a failed attempt, the error message may be misleading.";
  }
  if (QApplication::activeWindow() != nullptr) {
    MessageDialog::showMessage(fullMessage, QApplication::activeWindow());
  }
  emit modDetailsUpdated(false);
}


bool ModInfoRegular::updateNXMInfo()
{
  if (m_NexusID > 0) {
    m_NexusBridge.requestDescription(m_GameName, m_NexusID, QVariant());
    return true;
  }
  return false;
}


void ModInfoRegular::setCategory(int categoryID, bool active)
{
  m_MetaInfoChanged = true;

  if (active) {
    m_Categories.insert(categoryID);
    if (m_PrimaryCategory == -1) {
      m_PrimaryCategory = categoryID;
    }
  } else {
    std::set<int>::iterator iter = m_Categories.find(categoryID);
    if (iter != m_Categories.end()) {
      m_Categories.erase(iter);
    }
    if (categoryID == m_PrimaryCategory) {
      if (m_Categories.size() == 0) {
        m_PrimaryCategory = -1;
      } else {
        m_PrimaryCategory = *(m_Categories.begin());
      }
    }
  }
}


bool ModInfoRegular::setName(const QString &name)
{
  if (name.contains('/') || name.contains('\\')) {
    return false;
  }

  QString newPath = m_Path.mid(0).replace(m_Path.length() - m_Name.length(), m_Name.length(), name);
  QDir modDir(m_Path.mid(0, m_Path.length() - m_Name.length()));

  if (m_Name.compare(name, Qt::CaseInsensitive) == 0) {
    QString tempName = name;
    tempName.append("_temp");
    while (modDir.exists(tempName)) {
      tempName.append("_");
    }
    if (!modDir.rename(m_Name, tempName)) {
      return false;
    }
    if (!modDir.rename(tempName, name)) {
      qCritical("rename to final name failed after successful rename to intermediate name");
      modDir.rename(tempName, m_Name);
      return false;
    }
  } else {
    if (!shellRename(modDir.absoluteFilePath(m_Name), modDir.absoluteFilePath(name))) {
      qCritical("failed to rename mod %s (errorcode %d)",
                qPrintable(name), ::GetLastError());
      return false;
    }
  }

  std::map<QString, unsigned int>::iterator nameIter = s_ModsByName.find(m_Name);
  if (nameIter != s_ModsByName.end()) {
    unsigned int index = nameIter->second;
    s_ModsByName.erase(nameIter);

    m_Name = name;
    m_Path = newPath;

    s_ModsByName[m_Name] = index;

    std::sort(s_Collection.begin(), s_Collection.end(), ModInfo::ByName);
    updateIndices();
  } else { // otherwise mod isn't registered yet?
    m_Name = name;
    m_Path = newPath;
  }

  return true;
}

void ModInfoRegular::setNotes(const QString &notes)
{
  m_Notes = notes;
  m_MetaInfoChanged = true;
}

void ModInfoRegular::setGameName(QString gameName)
{
  m_GameName = gameName;
  m_MetaInfoChanged = true;
}

void ModInfoRegular::setNexusID(int modID)
{
  m_NexusID = modID;
  m_MetaInfoChanged = true;
}

void ModInfoRegular::setVersion(const VersionInfo &version)
{
  m_Version = version;
  m_MetaInfoChanged = true;
}

void ModInfoRegular::setNewestVersion(const VersionInfo &version)
{
  if (version != m_NewestVersion) {
    m_NewestVersion = version;
    m_MetaInfoChanged = true;
  }
}

void ModInfoRegular::setNexusDescription(const QString &description)
{
  if (qHash(description) != qHash(m_NexusDescription)) {
    m_NexusDescription = description;
    m_MetaInfoChanged = true;
  }
}

void ModInfoRegular::setEndorsedState(EEndorsedState endorsedState)
{
  if (endorsedState != m_EndorsedState) {
    m_EndorsedState = endorsedState;
    m_MetaInfoChanged = true;
  }
}

void ModInfoRegular::setInstallationFile(const QString &fileName)
{
  m_InstallationFile = fileName;
  m_MetaInfoChanged = true;
}

void ModInfoRegular::addNexusCategory(int categoryID)
{
  m_Categories.insert(CategoryFactory::instance().resolveNexusID(categoryID));
}

void ModInfoRegular::setIsEndorsed(bool endorsed)
{
  if (m_EndorsedState != ENDORSED_NEVER) {
    m_EndorsedState = endorsed ? ENDORSED_TRUE : ENDORSED_FALSE;
    m_MetaInfoChanged = true;
  }
}


void ModInfoRegular::setNeverEndorse()
{
  m_EndorsedState = ENDORSED_NEVER;
  m_MetaInfoChanged = true;
}


void ModInfoRegular::setColor(QColor color)
{
  m_Color = color;
  m_MetaInfoChanged = true;
}

QColor ModInfoRegular::getColor() 
{
  return m_Color;
}

bool ModInfoRegular::remove()
{
  m_MetaInfoChanged = false;
  return shellDelete(QStringList(absolutePath()), true);
}

void ModInfoRegular::endorse(bool doEndorse)
{
  if (doEndorse != (m_EndorsedState == ENDORSED_TRUE)) {
    m_NexusBridge.requestToggleEndorsement(m_GameName, getNexusID(), doEndorse, QVariant(1));
  }
}

void ModInfoRegular::markConverted(bool converted)
{
  m_Converted = converted;
  m_MetaInfoChanged = true;
  saveMeta();
  emit modDetailsUpdated(true);
}

void ModInfoRegular::markValidated(bool validated)
{
  m_Validated = validated;
  m_MetaInfoChanged = true;
  saveMeta();
  emit modDetailsUpdated(true);
}

QString ModInfoRegular::absolutePath() const
{
  return m_Path;
}

void ModInfoRegular::ignoreUpdate(bool ignore)
{
  if (ignore) {
    m_IgnoredVersion = m_NewestVersion;
  } else {
    m_IgnoredVersion.clear();
  }
  m_MetaInfoChanged = true;
}


std::vector<ModInfo::EFlag> ModInfoRegular::getFlags() const
{
  std::vector<ModInfo::EFlag> result = ModInfoWithConflictInfo::getFlags();
  if ((m_NexusID > 0) && (endorsedState() == ENDORSED_FALSE)) {
    result.push_back(ModInfo::FLAG_NOTENDORSED);
  }
  if (!isValid() && !m_Validated) {
    result.push_back(ModInfo::FLAG_INVALID);
  }
  if (m_Notes.length() != 0) {
    result.push_back(ModInfo::FLAG_NOTES);
  }
  if (m_PluginSelected) {
    result.push_back(ModInfo::FLAG_PLUGIN_SELECTED);
  }
  if (m_IsAlternate && !m_Converted) {
    result.push_back(ModInfo::FLAG_ALTERNATE_GAME);
  }
  return result;
}


std::vector<ModInfo::EContent> ModInfoRegular::getContents() const
{
  QTime now = QTime::currentTime();
  if (m_LastContentCheck.isNull() || (m_LastContentCheck.secsTo(now) > 60)) {
    m_CachedContent.clear();
    QDir dir(absolutePath());
    if (dir.entryList(QStringList() << "*.esp" << "*.esm" << "*.esl").size() > 0) {
      m_CachedContent.push_back(CONTENT_PLUGIN);
    }
    if (dir.entryList(QStringList() << "*.bsa" << "*.ba2").size() > 0) {
      m_CachedContent.push_back(CONTENT_BSA);
    }
    //use >1 for ini files since there is meta.ini in all mods already.
    if (dir.entryList(QStringList() << "*.ini").size() > 1) {
      m_CachedContent.push_back(CONTENT_INI);
    }

    if (dir.entryList(QStringList() << "*.modgroups").size() > 0) {
      m_CachedContent.push_back(CONTENT_MODGROUP);
    }

    ScriptExtender *extender = qApp->property("managed_game")
                                   .value<IPluginGame *>()
                                   ->feature<ScriptExtender>();

    if (extender != nullptr) {
      QString sePluginPath = extender->PluginPath();
      if (dir.exists(sePluginPath)) {
        m_CachedContent.push_back(CONTENT_SKSEFILES);
        QDir sePluginDir(absolutePath() + "/" + sePluginPath);
        if (sePluginDir.entryList(QStringList() << "*.dll").size() > 0) {
          m_CachedContent.push_back(CONTENT_SKSE);
        }
      }  
    }
    if (dir.exists("textures") || dir.exists("icons") || dir.exists("bookart"))
      m_CachedContent.push_back(CONTENT_TEXTURE);
    if (dir.exists("meshes"))
      m_CachedContent.push_back(CONTENT_MESH);
    if (dir.exists("interface") || dir.exists("menus"))
      m_CachedContent.push_back(CONTENT_INTERFACE);
    if (dir.exists("music") || dir.exists("sound"))
      m_CachedContent.push_back(CONTENT_SOUND);
    if (dir.exists("scripts"))
      m_CachedContent.push_back(CONTENT_SCRIPT);
    if (dir.exists("SkyProc Patchers"))
      m_CachedContent.push_back(CONTENT_SKYPROC);
    if (dir.exists("MCM"))
      m_CachedContent.push_back(CONTENT_MCM);

    m_LastContentCheck = QTime::currentTime();
  }

  return m_CachedContent;

}


int ModInfoRegular::getHighlight() const
{
  if (!isValid() && !m_Validated)
    return HIGHLIGHT_INVALID;
  auto flags = getFlags();
  if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_PLUGIN_SELECTED) != flags.end())
    return HIGHLIGHT_PLUGIN;
  return HIGHLIGHT_NONE;
}


QString ModInfoRegular::getDescription() const
{
  if (!isValid()  && !m_Validated) {
    return tr("%1 contains no esp/esm/esl and no asset (textures, meshes, interface, ...) directory").arg(name());
  } else {
    const std::set<int> &categories = getCategories();
    std::wostringstream categoryString;
    categoryString << ToWString(tr("Categories: <br>"));
    CategoryFactory &categoryFactory = CategoryFactory::instance();
    for (std::set<int>::const_iterator catIter = categories.begin();
         catIter != categories.end(); ++catIter) {
      if (catIter != categories.begin()) {
        categoryString << " , ";
      }
      categoryString << "<span style=\"white-space: nowrap;\"><i>" << ToWString(categoryFactory.getCategoryName(categoryFactory.getCategoryIndex(*catIter))) << "</font></span>";
    }

    return ToQString(categoryString.str());
  }
}

QString ModInfoRegular::notes() const
{
  return m_Notes;
}

QDateTime ModInfoRegular::creationTime() const
{
  return m_CreationTime;
}

QString ModInfoRegular::getNexusDescription() const
{
  return m_NexusDescription;
}

QString ModInfoRegular::repository() const
{
  return m_Repository;
}

ModInfoRegular::EEndorsedState ModInfoRegular::endorsedState() const
{
  return m_EndorsedState;
}

QDateTime ModInfoRegular::getLastNexusQuery() const
{
  return m_LastNexusQuery;
}

void ModInfoRegular::setURL(QString const &url)
{
  m_URL = url;
  m_MetaInfoChanged = true;
}

QString ModInfoRegular::getURL() const
{
  return m_URL;
}



QStringList ModInfoRegular::archives() const
{
  QStringList result;
  QDir dir(this->absolutePath());
  for (const QString &archive : dir.entryList(QStringList({ "*.bsa", "*.ba2" }))) {
    result.append(this->absolutePath() + "/" + archive);
  }
  return result;
}

void ModInfoRegular::addInstalledFile(int modId, int fileId)
{
  m_InstalledFileIDs.insert(std::make_pair(modId, fileId));
  m_MetaInfoChanged = true;
}

std::vector<QString> ModInfoRegular::getIniTweaks() const
{
  QString metaFileName = absolutePath().append("/meta.ini");
  QSettings metaFile(metaFileName, QSettings::IniFormat);

  std::vector<QString> result;

  int numTweaks = metaFile.beginReadArray("INI Tweaks");

  if (numTweaks != 0) {
    qDebug("%d active ini tweaks in %s",
           numTweaks, QDir::toNativeSeparators(metaFileName).toUtf8().constData());
  }

  for (int i = 0; i < numTweaks; ++i) {
    metaFile.setArrayIndex(i);
    QString filename = absolutePath().append("/INI Tweaks/").append(metaFile.value("name").toString());
    result.push_back(filename);
  }
  metaFile.endArray();
  return result;
}

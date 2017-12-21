/*
Copyright (C) 2016 Sebastian Herbord. All rights reserved.

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


#include "instancemanager.h"
#include "selectiondialog.h"
#include <utility.h>
#include <appconfig.h>
#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>
#include <QInputDialog>
#include <QMessageBox>
#include <cstdint>


static const char COMPANY_NAME[]     = "Tannin";
static const char APPLICATION_NAME[] = "Mod Organizer";
static const char INSTANCE_KEY[]     = "CurrentInstance";



InstanceManager::InstanceManager()
  : m_AppSettings(COMPANY_NAME, APPLICATION_NAME)
{
}

InstanceManager &InstanceManager::instance()
{
  static InstanceManager s_Instance;
  return s_Instance;
}

void InstanceManager::overrideInstance(const QString& instanceName)
{
  m_overrideInstanceName = instanceName;
  m_overrideInstance = true;
}


QString InstanceManager::currentInstance() const
{
  if (m_overrideInstance)
    return m_overrideInstanceName;
  else
    return m_AppSettings.value(INSTANCE_KEY, "").toString();
}

void InstanceManager::clearCurrentInstance()
{
  setCurrentInstance("");
  m_Reset = true;
}

void InstanceManager::setCurrentInstance(const QString &name)
{
  m_AppSettings.setValue(INSTANCE_KEY, name);
}


QString InstanceManager::queryInstanceName() const
{
  QString instanceId;
  while (instanceId.isEmpty()) {
    QInputDialog dialog;
    // would be neat if we could take the names from the game plugins but
    // the required initialization order requires the ini file to be
    // available *before* we load plugins
    dialog.setComboBoxItems({ "Oblivion", "Skyrim", "SkyrimSE", "Fallout 3",
                              "Fallout NV", "Fallout 4" });
    dialog.setComboBoxEditable(true);
    dialog.setWindowTitle(QObject::tr("Enter Instance Name"));
    dialog.setLabelText(QObject::tr("Name"));
    if (dialog.exec() == QDialog::Rejected) {
      throw MOBase::MyException(QObject::tr("Canceled"));
    }
    instanceId = dialog.textValue().replace(QRegExp("[^0-9a-zA-Z ]"), "");
  }
  return instanceId;
}


QString InstanceManager::chooseInstance(const QStringList &instanceList) const
{
  enum class Special : uint8_t {
    NewInstance,
    Portable
  };

  SelectionDialog selection(
      QString("<h3>%1</h3><br>%2")
          .arg(QObject::tr("Choose Instance"))
          .arg(QObject::tr(
              "Each Instance is a full set of MO data files (mods, "
              "downloads, profiles, configuration, ...). Use multiple "
              "instances for different games. If your MO folder is "
              "writable, you can also store a single instance locally (called "
              "a portable install).")),
      nullptr);
  selection.disableCancel();
  for (const QString &instance : instanceList) {
    selection.addChoice(instance, "", instance);
  }

  selection.addChoice(QIcon(":/MO/gui/add"), QObject::tr("New"),
                      QObject::tr("Create a new instance."),
                      static_cast<uint8_t>(Special::NewInstance));

  if (QFileInfo(qApp->applicationDirPath()).isWritable()) {
    selection.addChoice(QIcon(":/MO/gui/package"), QObject::tr("Portable"),
                        QObject::tr("Use MO folder for data."),
                        static_cast<uint8_t>(Special::Portable));
  }

  if (selection.exec() == QDialog::Rejected) {
    qDebug("rejected");
    throw MOBase::MyException(QObject::tr("Canceled"));
  }

  QVariant choice = selection.getChoiceData();

  if (choice.type() == QVariant::String) {
    return choice.toString();
  } else {
    switch (choice.value<uint8_t>()) {
      case Special::NewInstance: return queryInstanceName();
      case Special::Portable: return QString();
      default: throw std::runtime_error("invalid selection");
    }
  }
}


QString InstanceManager::instancePath() const
{
  return QDir::fromNativeSeparators(
        QStandardPaths::writableLocation(QStandardPaths::DataLocation));
}


QStringList InstanceManager::instances() const
{
  return QDir(instancePath()).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
}


bool InstanceManager::portableInstall() const
{
  return QFile::exists(qApp->applicationDirPath() + "/" +
                       QString::fromStdWString(AppConfig::iniFileName()));
}


void InstanceManager::createDataPath(const QString &dataPath) const
{
  if (!QDir(dataPath).exists()) {
    if (!QDir().mkpath(dataPath)) {
      throw MOBase::MyException(
            QObject::tr("failed to create %1").arg(dataPath));
    } else {
      QMessageBox::information(
          nullptr, QObject::tr("Data directory created"),
          QObject::tr("New data directory created at %1. If you don't want to "
                      "store a lot of data there, reconfigure the storage "
                      "directories via settings.").arg(dataPath));
    }
  }
}


QString InstanceManager::determineDataPath()
{
  QString instanceId = currentInstance();
  if (instanceId.isEmpty() && !m_Reset && (m_overrideInstance || portableInstall()))
  {
    // startup, apparently using portable mode before
    return qApp->applicationDirPath();
  }

  QString dataPath = QDir::fromNativeSeparators(
        QStandardPaths::writableLocation(QStandardPaths::DataLocation)
        + "/" + instanceId);


  if (!m_overrideInstance && (instanceId.isEmpty() || !QFileInfo::exists(dataPath))) {
    instanceId = chooseInstance(instances());
    setCurrentInstance(instanceId);
    if (!instanceId.isEmpty()) {
      dataPath = QDir::fromNativeSeparators(
        QStandardPaths::writableLocation(QStandardPaths::DataLocation)
        + "/" + instanceId);
    }
  }

  if (instanceId.isEmpty()) {
    return qApp->applicationDirPath();
  } else {
    createDataPath(dataPath);

    return dataPath;
  }
}


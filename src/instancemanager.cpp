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


static const char COMPANY_NAME[]     = "Tannin";
static const char APPLICATION_NAME[] = "Mod Organizer";
static const char INSTANCE_KEY[]     = "CurrentInstance";



InstanceManager::InstanceManager()
  : m_AppSettings(COMPANY_NAME, APPLICATION_NAME)
{
}


QString InstanceManager::currentInstance() const
{
  return m_AppSettings.value(INSTANCE_KEY, "").toString();
}

void InstanceManager::clearCurrentInstance()
{
  setCurrentInstance("");
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
    dialog.setComboBoxItems({ "Oblivion", "Skyrim", "Fallout 3",
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
  SelectionDialog selection(QObject::tr("Choose Instance"), nullptr);
  selection.disableCancel();
  for (const QString &instance : instanceList) {
    selection.addChoice(instance, "", instance);
  }

  selection.addChoice(QObject::tr("New"),
                      QObject::tr("Create a new instance."),
                      "");
  if (selection.exec() == QDialog::Rejected) {
    qDebug("rejected");
    throw MOBase::MyException(QObject::tr("Canceled"));
  }

  QString choice = selection.getChoiceData().toString();

  if (choice.isEmpty()) {
    return queryInstanceName();
  } else {
    return choice;
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


InstanceManager::InstallationMode InstanceManager::queryInstallMode() const
{
  SelectionDialog selection(QObject::tr("Installation Mode"), nullptr);
  selection.disableCancel();
  selection.addChoice(QObject::tr("Portable"),
                      QObject::tr("Everything in one directory, only one game per installation."),
                      0);
  selection.addChoice(QObject::tr("Regular"),
                      QObject::tr("Data in separate directory, multiple games supported."),
                      1);
  if (selection.exec() == QDialog::Rejected) {
    throw MOBase::MyException(QObject::tr("Canceled"));
  }

  switch (selection.getChoiceData().toInt()) {
    case 0:  return InstallationMode::PORTABLE;
    default: return InstallationMode::REGULAR;
  }
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

  if (instanceId.isEmpty() && !portableInstall()) {
    // no portable install and no selected instance

    QStringList instanceList = instances();

    if (instanceList.size() == 0) {
      if (QFileInfo(qApp->applicationDirPath()).isWritable()) {
        switch (queryInstallMode()) {
          case InstallationMode::PORTABLE: {
            instanceId = QString();
          } break;
          case InstallationMode::REGULAR: {
            instanceId = queryInstanceName();
          } break;
        }
      } else {
        instanceId = queryInstanceName();
      }
    } else {
      // don't offer portable instance if we can't set one up.
      instanceId = chooseInstance(instanceList);
    }
  }

  if (instanceId.isEmpty()) {
    qDebug("portable mode");
    return qApp->applicationDirPath();
  } else {
    setCurrentInstance(instanceId);

    QString dataPath = QDir::fromNativeSeparators(
          QStandardPaths::writableLocation(QStandardPaths::DataLocation)
          + "/" + instanceId);

    createDataPath(dataPath);

    return dataPath;
  }
}


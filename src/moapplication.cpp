/*
Copyright (C) 2012 Sebastian Herbord. All rights reserved.

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

#include "moapplication.h"
#include "report.h"
#include <utility.h>
#include <appconfig.h>
#include <QFile>
#include <QStringList>
#include <QPlastiqueStyle>
#include <QCleanlooksStyle>


MOApplication::MOApplication(int argc, char **argv)
  : QApplication(argc, argv)
{
  connect(&m_StyleWatcher, SIGNAL(fileChanged(QString)), SLOT(updateStyle(QString)));
  m_DefaultStyle = style()->objectName();
}


bool MOApplication::setStyleFile(const QString &styleName)
{
  // remove all files from watch
  QStringList currentWatch = m_StyleWatcher.files();
  if (currentWatch.count() != 0) {
    m_StyleWatcher.removePaths(currentWatch);
  }
  // set new stylesheet or clear it
  if (styleName.length() != 0) {
    QString styleSheetName = applicationDirPath() + "/" + MOBase::ToQString(AppConfig::stylesheetsPath()) + "/" + styleName;
    if (QFile::exists(styleSheetName)) {
      m_StyleWatcher.addPath(styleSheetName);
      updateStyle(styleSheetName);
    } else {
      updateStyle(styleName);
    }
  } else {
    setStyleSheet("");
  }
  return true;
}


bool MOApplication::notify(QObject *receiver, QEvent *event)
{
  try {
    return QApplication::notify(receiver, event);
  } catch (const std::exception &e) {
    qCritical("uncaught exception in handler (object %s, eventtype %d): %s",
              receiver->objectName().toUtf8().constData(), event->type(), e.what());
    reportError(tr("an error occured: %1").arg(e.what()));
    return false;
  } catch (...) {
    qCritical("uncaught non-std exception in handler (object %s, eventtype %d)",
              receiver->objectName().toUtf8().constData(), event->type());
    reportError(tr("an error occured"));
    return false;
  }
}


void MOApplication::updateStyle(const QString &fileName)
{
  if (fileName == "Plastique") {
    setStyle(new QPlastiqueStyle);
    setStyleSheet("");
  } else if (fileName == "Cleanlooks") {
    setStyle(new QCleanlooksStyle);
    setStyleSheet("");
  } else {
    setStyle(m_DefaultStyle);
    if (QFile::exists(fileName)) {
      setStyleSheet(QString("file:///%1").arg(fileName));
    } else {
      qWarning("invalid stylesheet: %s", qPrintable(fileName));
    }
  }
}

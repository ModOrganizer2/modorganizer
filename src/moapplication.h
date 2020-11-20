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

#ifndef MOAPPLICATION_H
#define MOAPPLICATION_H

class Settings;
class MOMultiProcess;
class Instance;
class PluginContainer;

namespace MOBase { class IPluginGame; }
namespace cl { class CommandLine; }

class MOApplication : public QApplication
{
  Q_OBJECT

public:
  MOApplication(cl::CommandLine& cl, int& argc, char** argv);

  // sets up everything, creates the main window and runs it
  //
  int run(MOMultiProcess& multiProcess);

  virtual bool notify(QObject* receiver, QEvent* event);

public slots:
  bool setStyleFile(const QString& style);

private slots:
  void updateStyle(const QString& fileName);

private:
  QFileSystemWatcher m_StyleWatcher;
  QString m_DefaultStyle;
  cl::CommandLine& m_cl;

  int doOneRun(MOMultiProcess& multiProcess);

  std::optional<Instance> getCurrentInstance();
  std::optional<int> setupInstanceLoop(Instance& currentInstance, PluginContainer& pc);
  void purgeOldFiles();
  void resetForRestart();
};


class MOSplash
{
public:
  MOSplash(
    const Settings& settings, const QString& dataPath,
    const MOBase::IPluginGame* game);

  void close();

private:
  std::unique_ptr<QSplashScreen> ss_;

  QString getSplashPath(
    const Settings& settings, const QString& dataPath,
    const MOBase::IPluginGame* game) const;
};

#endif // MOAPPLICATION_H

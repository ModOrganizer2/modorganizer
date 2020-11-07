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

#include "mainwindow.h"
#include "singleinstance.h"
#include "loglist.h"
#include "selectiondialog.h"
#include "moapplication.h"
#include "tutorialmanager.h"
#include "nxmaccessmanager.h"
#include "instancemanager.h"
#include "instancemanagerdialog.h"
#include "createinstancedialog.h"
#include "createinstancedialogpages.h"
#include "organizercore.h"
#include "env.h"
#include "envmodule.h"
#include "commandline.h"
#include "sanitychecks.h"
#include "shared/util.h"
#include "shared/appconfig.h"
#include "shared/error_report.h"
#include <imoinfo.h>
#include <report.h>
#include <usvfs.h>
#include <log.h>
#include <utility.h>

using namespace MOBase;
using namespace MOShared;

thread_local LPTOP_LEVEL_EXCEPTION_FILTER g_prevExceptionFilter = nullptr;
thread_local std::terminate_handler g_prevTerminateHandler = nullptr;

LONG WINAPI onUnhandledException(_EXCEPTION_POINTERS* ptrs)
{
  const std::wstring& dumpPath = OrganizerCore::crashDumpsPath();

  const int r = CreateMiniDump(
    ptrs, OrganizerCore::getGlobalCrashDumpsType(), dumpPath.c_str());

  if (r == 0) {
    log::error("ModOrganizer has crashed, crash dump created.");
  } else {
    log::error(
      "ModOrganizer has crashed, CreateMiniDump failed ({}, error {}).",
      r, GetLastError());
  }

  if (g_prevExceptionFilter && ptrs)
    return g_prevExceptionFilter(ptrs);
  else
    return EXCEPTION_CONTINUE_SEARCH;
}

void onTerminate() noexcept
{
  __try
  {
    // force an exception to get a valid stack trace for this thread
    *(int*)0 = 42;
  }
  __except
    (
      onUnhandledException(GetExceptionInformation()), EXCEPTION_EXECUTE_HANDLER
      )
  {
  }

  if (g_prevTerminateHandler) {
    g_prevTerminateHandler();
  } else {
    std::abort();
  }
}

void setExceptionHandlers()
{
  g_prevExceptionFilter = SetUnhandledExceptionFilter(onUnhandledException);
  g_prevTerminateHandler = std::set_terminate(onTerminate);
}

int forwardToPrimary(SingleInstance& instance, const cl::CommandLine& cl)
{
  if (cl.shortcut().isValid()) {
    instance.sendMessage(cl.shortcut().toString());
  } else if (cl.nxmLink()) {
    instance.sendMessage(*cl.nxmLink());
  } else {
    QMessageBox::information(
      nullptr, QObject::tr("Mod Organizer"),
      QObject::tr("An instance of Mod Organizer is already running"));
  }

  return 0;
}


int main(int argc, char *argv[])
{
  MOShared::SetThisThreadName("main");

  cl::CommandLine cl;
  if (auto r=cl.run(GetCommandLineW())) {
    return *r;
  }

  initLogging();

  QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
  MOApplication app(cl, argc, argv);

  SingleInstance instance(cl.multiple());
  if (instance.ephemeral()) {
    return forwardToPrimary(instance, cl);
  }

  return app.run(instance);
}

#include "commandline.h"
#include "env.h"
#include "instancemanager.h"
#include "loglist.h"
#include "moapplication.h"
#include "multiprocess.h"
#include "organizercore.h"
#include "shared/util.h"
#include "thememanager.h"
#include "thread_utils.h"
#include <log.h>
#include <report.h>

using namespace MOBase;

thread_local LPTOP_LEVEL_EXCEPTION_FILTER g_prevExceptionFilter = nullptr;
thread_local std::terminate_handler g_prevTerminateHandler      = nullptr;

int run(int argc, char* argv[]);

int main(int argc, char* argv[])
{
  const int r = run(argc, argv);
  std::cout << "mod organizer done\n";
  return r;
}

int run(int argc, char* argv[])
{
  MOShared::SetThisThreadName("main");
  setExceptionHandlers();

  cl::CommandLine cl;
  if (auto r = cl.process(GetCommandLineW())) {
    return *r;
  }

  initLogging();

  // must be after logging
  TimeThis tt("main() multiprocess");

  MOApplication app(argc, argv);

  // check if the command line wants to run something right now
  if (auto r = cl.runPostApplication(app)) {
    return *r;
  }

  // check if there's another process running
  MOMultiProcess multiProcess(cl.multiple());

  if (multiProcess.ephemeral()) {
    // this is not the primary process

    if (cl.forwardToPrimary(multiProcess)) {
      // but there's something on the command line that could be forwarded to
      // it, so just exit
      return 0;
    }

    QMessageBox::information(
        nullptr, QObject::tr("Mod Organizer"),
        QObject::tr("An instance of Mod Organizer is already running"));

    return 1;
  }

  // check if the command line wants to run something right now
  if (auto r = cl.runPostMultiProcess(multiProcess)) {
    return *r;
  }

  tt.stop();

  // stuff that's done only once, even if MO restarts in the loop below
  app.firstTimeSetup(multiProcess);

  // force the "Select instance" dialog on startup, only for first loop or when
  // the current instance cannot be used
  bool pick = cl.pick();

  // MO runs in a loop because it can be restarted in several ways, such as
  // when switching instances or changing some settings
  for (;;) {
    try {
      auto& m = InstanceManager::singleton();

      if (cl.instance()) {
        m.overrideInstance(*cl.instance());
      }

      if (cl.profile()) {
        m.overrideProfile(*cl.profile());
      }

      // set up plugins, OrganizerCore, etc.
      {
        const auto r = app.setup(multiProcess, pick);
        pick         = false;

        if (r == RestartExitCode || r == ReselectExitCode) {
          // resets things when MO is "restarted"
          app.resetForRestart();

          // don't reprocess command line
          cl.clear();

          if (r == ReselectExitCode) {
            pick = true;
          }

          continue;
        } else if (r != 0) {
          // something failed, quit
          return r;
        }
      }

      // check if the command line wants to run something right now
      if (auto r = cl.runPostOrganizer(app.core())) {
        return *r;
      }

      // run the main window
      const auto r = app.run(multiProcess);

      if (r == RestartExitCode) {
        // resets things when MO is "restarted"
        app.resetForRestart();

        // don't reprocess command line
        cl.clear();

        continue;
      }

      return r;
    } catch (const std::exception& e) {
      reportError(e.what());
      return 1;
    }
  }
}

LONG WINAPI onUnhandledException(_EXCEPTION_POINTERS* ptrs)
{
  const auto path = OrganizerCore::getGlobalCoreDumpPath();
  const auto type = OrganizerCore::getGlobalCoreDumpType();

  const auto r = env::coredump(path.empty() ? nullptr : path.c_str(), type);

  if (r) {
    log::error("ModOrganizer has crashed, core dump created.");
  } else {
    log::error("ModOrganizer has crashed, core dump failed");
  }

  // g_prevExceptionFilter somehow sometimes point to this function, making this
  // recurse and create hundreds of core dump, not sure why
  if (g_prevExceptionFilter && ptrs && g_prevExceptionFilter != onUnhandledException)
    return g_prevExceptionFilter(ptrs);
  else
    return EXCEPTION_CONTINUE_SEARCH;
}

void onTerminate() noexcept
{
  __try {
    // force an exception to get a valid stack trace for this thread
    *(int*)0 = 42;
  } __except (onUnhandledException(GetExceptionInformation()),
              EXCEPTION_EXECUTE_HANDLER) {
  }

  if (g_prevTerminateHandler) {
    g_prevTerminateHandler();
  } else {
    std::abort();
  }
}

void setExceptionHandlers()
{
  if (g_prevExceptionFilter) {
    // already called
    return;
  }

  g_prevExceptionFilter  = SetUnhandledExceptionFilter(onUnhandledException);
  g_prevTerminateHandler = std::set_terminate(onTerminate);
}

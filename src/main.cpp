#include "singleinstance.h"
#include "loglist.h"
#include "moapplication.h"
#include "organizercore.h"
#include "commandline.h"
#include "env.h"
#include "thread_utils.h"
#include "shared/util.h"
#include <log.h>

using namespace MOBase;

thread_local LPTOP_LEVEL_EXCEPTION_FILTER g_prevExceptionFilter = nullptr;
thread_local std::terminate_handler g_prevTerminateHandler = nullptr;

int forwardToPrimary(SingleInstance& instance, const cl::CommandLine& cl);

int main(int argc, char *argv[])
{
  TimeThis tt("main()");

  MOShared::SetThisThreadName("main");
  setExceptionHandlers();

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

  tt.stop();

  return app.run(instance);
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
  if (g_prevExceptionFilter) {
    // already called
    return;
  }

  g_prevExceptionFilter = SetUnhandledExceptionFilter(onUnhandledException);
  g_prevTerminateHandler = std::set_terminate(onTerminate);
}

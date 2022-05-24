#include "commandline.h"
#include "env.h"
#include "instancemanager.h"
#include "loglist.h"
#include "messagedialog.h"
#include "multiprocess.h"
#include "organizercore.h"
#include "shared/appconfig.h"
#include "shared/util.h"
#include <log.h>
#include <report.h>

namespace cl
{

using namespace MOBase;

std::string pad_right(std::string s, std::size_t n, char c = ' ')
{
  if (s.size() < n)
    s.append(n - s.size(), c);

  return s;
}

// formats the list of pairs in two columns
//
std::string table(const std::vector<std::pair<std::string, std::string>>& v,
                  std::size_t indent, std::size_t spacing)
{
  std::size_t longest = 0;

  for (auto&& p : v)
    longest = std::max(longest, p.first.size());

  std::string s;

  for (auto&& p : v) {
    if (!s.empty())
      s += "\n";

    s += std::string(indent, ' ') + pad_right(p.first, longest) + " " +
         std::string(spacing, ' ') + p.second;
  }

  return s;
}

CommandLine::CommandLine() : m_command(nullptr)
{
  createOptions();

  add<RunCommand, ReloadPluginCommand, DownloadFileCommand, RefreshCommand,
      CrashDumpCommand, LaunchCommand>();
}

std::optional<int> CommandLine::process(const std::wstring& line)
{
  try {
    auto args = po::split_winmain(line);
    if (!args.empty()) {
      // remove program name
      args.erase(args.begin());
    }

    // parsing the first part of the command line, including global options and
    // command name, but not the rest, which will be collected below

    auto parsed = po::wcommand_line_parser(args)
                      .options(m_allOptions)
                      .positional(m_positional)
                      .allow_unregistered()
                      .run();

    po::store(parsed, m_vm);
    po::notify(m_vm);

    // collect options past the command name
    auto opts = po::collect_unrecognized(parsed.options, po::include_positional);

    if (m_vm.count("command")) {
      // there's a word as the first argument; this may be a command name or
      // an old style exe name/binary

      const auto commandName = m_vm["command"].as<std::string>();

      // look for the command by name first
      for (auto&& c : m_commands) {
        if (c->name() == commandName) {
          // this is a command

          // remove the command name itself
          opts.erase(opts.begin());

          try {
            // legacy commands handle their own parsing, such as 'launch'; don't
            // attempt to parse anything here
            if (!c->legacy()) {
              // parse the the remainder of the command line according to the
              // command's options
              po::wcommand_line_parser parser(opts);

              auto co = c->allOptions();
              parser.options(co);

              auto pos = c->positional();
              parser.positional(pos);

              parsed = parser.run();

              po::store(parsed, m_vm);

              if (m_vm.count("help")) {
                env::Console console;
                std::cout << usage(c.get()) << "\n";
                return 0;
              }

              // must be below the help check because it throws if required
              // positional arguments are missing
              po::notify(m_vm);
            }

            c->set(line, m_vm, opts);
            m_command = c.get();

            return runEarly();
          } catch (po::error& e) {
            env::Console console;

            std::cerr << e.what() << "\n" << usage(c.get()) << "\n";

            return 1;
          }
        }
      }
    }

    // the first word on the command line is not a valid command, try the other
    // stuff; this is used in setupCore() below when called from
    // MOApplication::doOneRun()

    // look for help
    if (m_vm.count("help")) {
      env::Console console;
      std::cout << usage() << "\n";
      return 0;
    }

    if (!opts.empty()) {
      const auto qs = QString::fromStdWString(opts[0]);

      if (qs.startsWith("--")) {
        // assume that for something like `ModOrganizer.exe --bleh`, it's just
        // a bad option instead of an executable that starts with "--"
        env::Console console;
        std::cerr << "\nUnrecognized option " << qs.toStdString() << "\n";

        return 1;
      }

      // try as an moshortcut://
      m_shortcut = qs;

      if (!m_shortcut.isValid()) {
        // not a shortcut, try a link
        if (isNxmLink(qs)) {
          m_nxmLink = qs;
        } else {
          // assume an executable name/binary
          m_executable = qs;
        }
      }

      // remove the shortcut/nxm/executable
      opts.erase(opts.begin());

      for (auto&& o : opts) {
        m_untouched.push_back(QString::fromStdWString(o));
      }
    }

    return {};
  } catch (po::error& e) {
    env::Console console;

    std::cerr << e.what() << "\n" << usage() << "\n";

    return 1;
  }
}

bool CommandLine::forwardToPrimary(MOMultiProcess& multiProcess)
{
  if (m_shortcut.isValid()) {
    multiProcess.sendMessage(m_shortcut.toString());
  } else if (m_nxmLink) {
    multiProcess.sendMessage(*m_nxmLink);
  } else if (m_command && m_command->canForwardToPrimary()) {
    multiProcess.sendMessage(QString::fromWCharArray(GetCommandLineW()));
  } else {
    return false;
  }

  return true;
}

std::optional<int> CommandLine::runEarly()
{
  if (m_vm.count("logs")) {
    // in loglist.h
    logToStdout(true);
  }

  if (m_command) {
    return m_command->runEarly();
  }

  return {};
}

std::optional<int> CommandLine::runPostApplication(MOApplication& a)
{
  // handle -i with no arguments
  if (m_vm.count("instance") && m_vm["instance"].as<std::string>() == "") {
    env::Console c;

    if (auto i = InstanceManager::singleton().currentInstance()) {
      std::cout << i->displayName().toStdString() << "\n";
    } else {
      std::cout << "no instance configured\n";
    }

    return 0;
  }

  if (m_command) {
    return m_command->runPostApplication(a);
  }

  return {};
}

std::optional<int> CommandLine::runPostMultiProcess(MOMultiProcess& mp)
{
  if (m_command) {
    return m_command->runPostMultiProcess(mp);
  }

  return {};
}

std::optional<int> CommandLine::runPostOrganizer(OrganizerCore& core)
{
  if (m_shortcut.isValid()) {
    if (m_shortcut.hasExecutable()) {
      try {
        // make sure MO doesn't exit even if locking is disabled, ForceWait and
        // PreventExit will do that
        core.processRunner()
            .setFromShortcut(m_shortcut)
            .setWaitForCompletion(ProcessRunner::ForCommandLine, UILocker::PreventExit)
            .run();

        return 0;
      } catch (std::exception&) {
        // user was already warned
        return 1;
      }
    }
  } else if (m_nxmLink) {
    log::debug("starting download from command line: {}", *m_nxmLink);
    core.downloadRequestedNXM(*m_nxmLink);
  } else if (m_executable) {
    const QString exeName = *m_executable;
    log::debug("starting {} from command line", exeName);

    try {
      // pass the remaining parameters to the binary
      //
      // make sure MO doesn't exit even if locking is disabled, ForceWait and
      // PreventExit will do that
      core.processRunner()
          .setFromFileOrExecutable(exeName, m_untouched)
          .setWaitForCompletion(ProcessRunner::ForCommandLine, UILocker::PreventExit)
          .run();

      return 0;
    } catch (const std::exception& e) {
      reportError(QObject::tr("failed to start application: %1").arg(e.what()));
      return 1;
    }
  } else if (m_command) {
    return m_command->runPostOrganizer(core);
  }

  return {};
}

void CommandLine::clear()
{
  m_vm.clear();
  m_shortcut = {};
  m_nxmLink  = {};
}

void CommandLine::createOptions()
{
  m_visibleOptions.add_options()("help", "show this message")

      ("multiple", "allow multiple MO processes to run; see below")

          ("pick", "show the select instance dialog on startup")

              ("logs", "duplicates the logs to stdout")

                  ("instance,i", po::value<std::string>()->implicit_value(""),
                   "use the given instance (defaults to last used)")

                      ("profile,p", po::value<std::string>(),
                       "use the given profile (defaults to last used)");

  po::options_description options;
  options.add_options()("command", po::value<std::string>(), "command")(
      "subargs", po::value<std::vector<std::string>>(), "args");

  // one command name, followed by any arguments for that command
  m_positional.add("command", 1).add("subargs", -1);

  m_allOptions.add(m_visibleOptions);
  m_allOptions.add(options);
}

std::string CommandLine::usage(const Command* c) const
{
  std::ostringstream oss;

  oss << "\n"
      << "Usage:\n";

  if (c) {
    oss << "  ModOrganizer.exe [global-options] " << c->usageLine() << "\n\n";

    const std::string more = c->moreInfo();
    if (!more.empty()) {
      oss << more << "\n\n";
    }

    oss << "Command options:\n" << c->visibleOptions() << "\n";
  } else {
    oss << "  ModOrganizer.exe [options] [[command] [command-options]]\n"
        << "\n"
        << "Commands:\n";

    // name and description for all commands
    std::vector<std::pair<std::string, std::string>> v;
    for (auto&& c : m_commands) {
      // don't show legacy commands
      if (c->legacy()) {
        continue;
      }

      v.push_back({c->name(), c->description()});
    }

    oss << table(v, 2, 4) << "\n"
        << "\n";
  }

  oss << "Global options:\n" << m_visibleOptions << "\n";

  // show the more text unless this is usage for a specific command
  if (!c) {
    oss << "\n" << more() << "\n";
  }

  return oss.str();
}

bool CommandLine::pick() const
{
  return (m_vm.count("pick") > 0);
}

bool CommandLine::multiple() const
{
  return (m_vm.count("multiple") > 0);
}

std::optional<QString> CommandLine::profile() const
{
  if (m_vm.count("profile")) {
    return QString::fromStdString(m_vm["profile"].as<std::string>());
  }

  return {};
}

std::optional<QString> CommandLine::instance() const
{
  // note that moshortcut:// overrides -i

  if (m_shortcut.isValid() && m_shortcut.hasInstance()) {
    return m_shortcut.instanceName();
  } else if (m_vm.count("instance")) {
    return QString::fromStdString(m_vm["instance"].as<std::string>());
  }

  return {};
}

const MOShortcut& CommandLine::shortcut() const
{
  return m_shortcut;
}

std::optional<QString> CommandLine::nxmLink() const
{
  return m_nxmLink;
}

std::optional<QString> CommandLine::executable() const
{
  return m_executable;
}

const QStringList& CommandLine::untouched() const
{
  return m_untouched;
}

std::string CommandLine::more() const
{
  return "Multiple processes\n"
         "  A note on terminology: 'instance' can either mean an MO process\n"
         "  that's running on the system, or a set of mods and profiles managed\n"
         "  by MO. To avoid confusion, the term 'process' is used below for the\n"
         "  former.\n"
         "  \n"
         "  --multiple can be used to allow multiple MO processes to run\n"
         "  simultaneously. This is unsupported and can create all sorts of weird\n"
         "  problems. To minimize these:\n"
         "  \n"
         "    1) Never have multiple MO processes running that manage the same\n"
         "       game instance.\n"
         "    2) If an executable is launched from an MO process, only this\n"
         "       process may launch executables until all processes are \n"
         "       terminated.\n"
         "  \n"
         "  It is recommended to close _all_ MO processes as soon as multiple\n"
         "  processes become unnecessary.";
}

Command::Meta::Meta(std::string n, std::string d, std::string u, std::string m)
    : name(n), description(d), usage(u), more(m)
{}

std::string Command::name() const
{
  return meta().name;
}

std::string Command::description() const
{
  return meta().description;
}

std::string Command::usageLine() const
{
  return name() + " " + meta().usage;
}

std::string Command::moreInfo() const
{
  return meta().more;
}

po::options_description Command::allOptions() const
{
  po::options_description d;

  d.add(visibleOptions());
  d.add(getInternalOptions());

  return d;
}

po::options_description Command::visibleOptions() const
{
  po::options_description d(getVisibleOptions());

  d.add_options()("help", "shows this message");

  return d;
}

po::positional_options_description Command::positional() const
{
  return getPositional();
}

bool Command::legacy() const
{
  return false;
}

po::options_description Command::getVisibleOptions() const
{
  // no-op
  return {};
}

po::options_description Command::getInternalOptions() const
{
  // no-op
  return {};
}

po::positional_options_description Command::getPositional() const
{
  // no-op
  return {};
}

void Command::set(const std::wstring& originalLine, po::variables_map vm,
                  std::vector<std::wstring> untouched)
{
  m_original  = originalLine;
  m_vm        = vm;
  m_untouched = untouched;
}

std::optional<int> Command::runEarly()
{
  return {};
}

std::optional<int> Command::runPostApplication(MOApplication& a)
{
  return {};
}

std::optional<int> Command::runPostMultiProcess(MOMultiProcess&)
{
  return {};
}

std::optional<int> Command::runPostOrganizer(OrganizerCore&)
{
  return {};
}

bool Command::canForwardToPrimary() const
{
  return false;
}

const std::wstring& Command::originalCmd() const
{
  return m_original;
}

const po::variables_map& Command::vm() const
{
  return m_vm;
}

const std::vector<std::wstring>& Command::untouched() const
{
  return m_untouched;
}

po::options_description CrashDumpCommand::getVisibleOptions() const
{
  po::options_description d;

  d.add_options()("type", po::value<std::string>()->default_value("mini"),
                  "mini|data|full");

  return d;
}

Command::Meta CrashDumpCommand::meta() const
{
  return {"crashdump", "writes a crashdump for a running process of MO", "[options]",
          ""};
}

std::optional<int> CrashDumpCommand::runEarly()
{
  env::Console console;

  const auto typeString = vm()["type"].as<std::string>();
  const auto type       = env::coreDumpTypeFromString(typeString);

  // dump
  const auto b = env::coredumpOther(type);
  if (!b) {
    std::wcerr << L"\n>>>> a minidump file was not written\n\n";
  }

  std::wcerr << L"Press enter to continue...";
  std::wcin.get();

  return (b ? 0 : 1);
}

Command::Meta LaunchCommand::meta() const
{
  return {"launch", "(internal, do not use)", "", ""};
}

bool LaunchCommand::legacy() const
{
  return true;
}

std::optional<int> LaunchCommand::runEarly()
{
  // needs at least the working directory and process name
  if (untouched().size() < 2) {
    return 1;
  }

  std::vector<std::wstring> arg;
  auto args = UntouchedCommandLineArguments(2, arg);

  return SpawnWaitProcess(arg[1].c_str(), args);
}

int LaunchCommand::SpawnWaitProcess(LPCWSTR workingDirectory, LPCWSTR commandLine)
{
  PROCESS_INFORMATION pi{0};
  STARTUPINFO si{0};
  si.cb                        = sizeof(si);
  std::wstring commandLineCopy = commandLine;

  if (!CreateProcessW(NULL, &commandLineCopy[0], NULL, NULL, FALSE, 0, NULL,
                      workingDirectory, &si, &pi)) {
    // A bit of a problem where to log the error message here, at least this way you can
    // get the message using a either DebugView or a live debugger:
    std::wostringstream ost;
    ost << L"CreateProcess failed: " << commandLine << ", " << GetLastError();
    OutputDebugStringW(ost.str().c_str());
    return -1;
  }

  WaitForSingleObject(pi.hProcess, INFINITE);

  DWORD exitCode = (DWORD)-1;
  ::GetExitCodeProcess(pi.hProcess, &exitCode);
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  return static_cast<int>(exitCode);
}

// Parses the first parseArgCount arguments of the current process command line and
// returns them in parsedArgs, the rest of the command line is returned untouched.
LPCWSTR
LaunchCommand::UntouchedCommandLineArguments(int parseArgCount,
                                             std::vector<std::wstring>& parsedArgs)
{
  LPCWSTR cmd = GetCommandLineW();
  LPCWSTR arg = nullptr;  // to skip executable name
  for (; parseArgCount >= 0 && *cmd; ++cmd) {
    if (*cmd == '"') {
      int escaped = 0;
      for (++cmd; *cmd && (*cmd != '"' || escaped % 2 != 0); ++cmd)
        escaped = *cmd == '\\' ? escaped + 1 : 0;
    }
    if (*cmd == ' ') {
      if (arg)
        if (cmd - 1 > arg && *arg == '"' && *(cmd - 1) == '"')
          parsedArgs.push_back(std::wstring(arg + 1, cmd - 1));
        else
          parsedArgs.push_back(std::wstring(arg, cmd));
      arg = cmd + 1;
      --parseArgCount;
    }
  }
  return cmd;
}

Command::Meta RunCommand::meta() const
{
  return {"run", "runs a program, file or a configured executable", "[options] NAME",

          "Runs a program or a file with the virtual filesystem. If NAME is a path\n"
          "to a non-executable file, the program that is associated with the file\n"
          "extension is run instead. With -e, NAME must refer to the name of an\n"
          "executable in the instance (for example, \"SKSE\")."};
}

po::options_description RunCommand::getVisibleOptions() const
{
  po::options_description d;

  d.add_options()("executable,e",
                  po::value<bool>()->default_value(false)->zero_tokens(),
                  "the name is a configured executable name")(
      "arguments,a", po::value<std::string>(), "override arguments")(
      "cwd,c", po::value<std::string>(), "override working directory");

  return d;
}

po::options_description RunCommand::getInternalOptions() const
{
  po::options_description d;

  d.add_options()("NAME", po::value<std::string>()->required(),
                  "program or executable name");

  return d;
}

po::positional_options_description RunCommand::getPositional() const
{
  po::positional_options_description d;

  d.add("NAME", 1);

  return d;
}

bool RunCommand::canForwardToPrimary() const
{
  return true;
}

std::optional<int> RunCommand::runPostOrganizer(OrganizerCore& core)
{
  const auto program = QString::fromStdString(vm()["NAME"].as<std::string>());

  try {
    // make sure MO doesn't exit even if locking is disabled, ForceWait and
    // PreventExit will do that
    auto p = core.processRunner();

    if (vm()["executable"].as<bool>()) {
      const auto& exes = *core.executablesList();

      // case sensitive
      auto itor = exes.find(program, true);
      if (itor == exes.end()) {
        // case insensitive
        itor = exes.find(program, false);

        if (itor == exes.end()) {
          // not found
          reportError(
              QObject::tr("Executable '%1' not found in instance '%2'.")
                  .arg(program)
                  .arg(InstanceManager::singleton().currentInstance()->displayName()));

          return 1;
        }
      }

      p.setFromExecutable(*itor);
    } else {
      p.setFromFile(nullptr, QFileInfo(program));
    }

    if (vm().count("arguments")) {
      p.setArguments(QString::fromStdString(vm()["arguments"].as<std::string>()));
    }

    if (vm().count("cwd")) {
      p.setCurrentDirectory(QString::fromStdString(vm()["cwd"].as<std::string>()));
    }

    p.setWaitForCompletion(ProcessRunner::ForCommandLine, UILocker::PreventExit);

    const auto r = p.run();
    if (r == ProcessRunner::Error) {
      reportError(
          QObject::tr("Failed to run '%1'. The logs might have more information.")
              .arg(program));

      return 1;
    }

    return 0;
  } catch (const std::exception& e) {
    reportError(
        QObject::tr("Failed to run '%1'. The logs might have more information. %2")
            .arg(program)
            .arg(e.what()));

    return 1;
  }
}

Command::Meta ReloadPluginCommand::meta() const
{
  return {"reload-plugin", "reloads the given plugin", "PLUGIN", ""};
}

po::options_description ReloadPluginCommand::getInternalOptions() const
{
  po::options_description d;

  d.add_options()("PLUGIN", po::value<std::string>()->required(), "plugin name");

  return d;
}

po::positional_options_description ReloadPluginCommand::getPositional() const
{
  po::positional_options_description d;

  d.add("PLUGIN", 1);

  return d;
}

bool ReloadPluginCommand::canForwardToPrimary() const
{
  return true;
}

std::optional<int> ReloadPluginCommand::runPostOrganizer(OrganizerCore& core)
{
  const QString name = QString::fromStdString(vm()["PLUGIN"].as<std::string>());

  QString filepath =
      QDir(qApp->applicationDirPath() + "/" + ToQString(AppConfig::pluginPath()))
          .absoluteFilePath(name);

  // TODO: reload extension, not plugin
  log::debug("reloading plugin from {}", filepath);
  // core.pluginManager().reloadPlugin(filepath);

  return {};
}

Command::Meta DownloadFileCommand::meta() const
{
  return {"download", "downloads a file", "URL", ""};
}

po::options_description DownloadFileCommand::getInternalOptions() const
{
  po::options_description d;

  d.add_options()("URL", po::value<std::string>()->required(), "file URL");

  return d;
}

po::positional_options_description DownloadFileCommand::getPositional() const
{
  po::positional_options_description d;

  d.add("URL", 1);

  return d;
}

bool DownloadFileCommand::canForwardToPrimary() const
{
  return true;
}

std::optional<int> DownloadFileCommand::runPostOrganizer(OrganizerCore& core)
{
  const QString url = QString::fromStdString(vm()["URL"].as<std::string>());

  if (!url.startsWith("https://")) {
    reportError(QObject::tr("Download URL must start with https://"));
    return 1;
  }

  log::debug("starting direct download from command line: {}", url.toStdString());
  MessageDialog::showMessage(QObject::tr("Download started"), qApp->activeWindow(),
                             false);
  core.downloadManager()->startDownloadURLs(QStringList() << url);

  return {};
}

Command::Meta RefreshCommand::meta() const
{
  return {"refresh", "refreshes MO (same as F5)", "", ""};
}

bool RefreshCommand::canForwardToPrimary() const
{
  return true;
}

std::optional<int> RefreshCommand::runPostOrganizer(OrganizerCore& core)
{
  core.refresh();
  return {};
}

}  // namespace cl

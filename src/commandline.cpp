#include "commandline.h"
#include "env.h"
#include "shared/util.h"

namespace cl
{

CommandLine::CommandLine()
{
  createOptions();
  m_commands.push_back(std::make_unique<CrashDumpCommand>());
  m_commands.push_back(std::make_unique<LaunchCommand>());
}

std::optional<int> CommandLine::run(const std::wstring& line)
{
  try
  {
    auto args = po::split_winmain(line);
    if (!args.empty()) {
      // remove program name
      args.erase(args.begin());
    }

    auto parsed = po::wcommand_line_parser(args)
      .options(m_allOptions)
      .positional(m_positional)
      .allow_unregistered()
      .run();

    po::store(parsed, m_vm);

    auto opts = po::collect_unrecognized(
      parsed.options, po::include_positional);


    if (m_vm.count("command")) {
      const auto commandName = m_vm["command"].as<std::string>();

      for (auto&& c : m_commands) {
        if (c->name() == commandName) {
          // remove the command name itself
          opts.erase(opts.begin());

          auto co = c->options();
          co.add_options()
            ("help", "shows this message");

          po::wcommand_line_parser parser(opts);
          parser.options(co);

          if (c->allow_unregistered()) {
            parser.allow_unregistered();
          }

          parsed = parser.run();

          po::store(parsed, m_vm);

          if (m_vm.count("help")) {
            env::Console console;
            std::cout << usage(c.get()) << "\n";
            return 0;
          }

          return c->run(line, m_vm, opts);
        }
      }
    }

    if (m_vm.count("help")) {
      env::Console console;
      std::cout << usage() << "\n";
      return 0;
    }

    if (!opts.empty()) {
      const auto qs = QString::fromStdWString(opts[0]);
      m_shortcut = qs;

      if (!m_shortcut.isValid()) {
        if (isNxmLink(qs)) {
          m_nxmLink = qs;
        } else {
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
  }
  catch(po::error& e)
  {
    env::Console console;

    std::cerr
      << e.what() << "\n"
      << usage() << "\n";

    return 1;
  }
}

void CommandLine::clear()
{
  m_vm.clear();
  m_shortcut = {};
  m_nxmLink = {};
}

void CommandLine::createOptions()
{
  m_visibleOptions.add_options()
    ("help",      "show this message")
    ("multiple",  "allow multiple instances of MO to run; see below")
    ("instance,i", po::value<std::string>(), "use the given instance (defaults to last used)")
    ("profile,p", po::value<std::string>(), "use the given profile (defaults to last used)");

  po::options_description options;
  options.add_options()
    ("command", po::value<std::string>(), "command")
    ("subargs", po::value<std::vector<std::string> >(), "args");

  m_positional
    .add("command", 1)
    .add("subargs", -1);

  m_allOptions.add(m_visibleOptions);
  m_allOptions.add(options);
}

std::string CommandLine::usage(const Command* c) const
{
  std::ostringstream oss;

  oss
    << "\n"
    << "Usage:\n";

  if (c) {
    oss
      << "  ModOrganizer.exe [options] " << c->name() << " [command-options]\n"
      << "\n"
      << "Command options:\n"
      << c->options() << "\n";
  } else {
    oss
      << "  ModOrganizer.exe [options] [[command] [command-options]]\n"
      << "\n"
      << "Commands:\n";

    for (auto&& c : m_commands) {
      oss << "  " << c->name() << "    " << c->description() << "\n";
    }

    oss << "\n";
  }

  oss
    << "Global options:\n"
    << m_visibleOptions << "\n";

  if (!c) {
    oss << "\n" << more() << "\n";
  }

  return oss.str();
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
  if (m_shortcut.isValid() && m_shortcut.hasInstance()) {
    return m_shortcut.instance();
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
  return
    "Multiple instances\n"
    "  --multiple can be used to allow multiple instances of MO to run\n"
    "  simultaneously. This is unsupported and can create all sorts of weird\n"
    "  problems. To minimize the problems:\n"
    "  \n"
    "    1) Never have multiple MO instances opened that manage the same\n"
    "       game instance.\n"
    "    2) If an executable is launched from an instance, only this\n"
    "       instance may launch executables until all instances are closed.\n"
    "  \n"
    "  It is recommended to close _all_ instances of MO as soon as multiple\n"
    "  instances become unnecessary.";
}


std::string Command::name() const
{
  return meta().name;
}

std::string Command::description() const
{
  return meta().description;
}

bool Command::allow_unregistered() const
{
  return false;
}

po::options_description Command::options() const
{
  return doOptions();
}

po::options_description Command::doOptions() const
{
  // no-op
  return {};
}

std::string Command::usage() const
{
  std::ostringstream oss;

  oss
    << "\n"
    << "Usage:\n"
    << "    ModOrganizer.exe [options] [[command] [command-options]]\n"
    << "\n"
    << "Options:\n"
    << options() << "\n";

  return oss.str();
}

std::optional<int> Command::run(
  const std::wstring& originalLine,
  po::variables_map vm,
  std::vector<std::wstring> untouched)
{
  m_original = originalLine;
  m_vm = vm;
  m_untouched = untouched;

  return doRun();
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


po::options_description CrashDumpCommand::doOptions() const
{
  po::options_description d;

  d.add_options()
    ("type", po::value<std::string>()->default_value("mini"), "mini|data|full");

  return d;
}

Command::Meta CrashDumpCommand::meta() const
{
  return {"crashdump", "writes a crashdump for a running process of MO"};
}

std::optional<int> CrashDumpCommand::doRun()
{
  env::Console console;

  const auto typeString = vm()["type"].as<std::string>();
  const auto type = env::coreDumpTypeFromString(typeString);

  // dump
  const auto b = env::coredumpOther(type);
  if (!b) {
    std::wcerr << L"\n>>>> a minidump file was not written\n\n";
  }

  std::wcerr << L"Press enter to continue...";
  std::wcin.get();

  return (b ? 0 : 1);
}


bool LaunchCommand::allow_unregistered() const
{
  return true;
}

po::options_description LaunchCommand::doOptions() const
{
  return {};
}

Command::Meta LaunchCommand::meta() const
{
  return {"launch", ""};
}

std::optional<int> LaunchCommand::doRun()
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
  PROCESS_INFORMATION pi{ 0 };
  STARTUPINFO si{ 0 };
  si.cb = sizeof(si);
  std::wstring commandLineCopy = commandLine;

  if (!CreateProcessW(NULL, &commandLineCopy[0], NULL, NULL, FALSE, 0, NULL, workingDirectory, &si, &pi)) {
    // A bit of a problem where to log the error message here, at least this way you can get the message
    // using a either DebugView or a live debugger:
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

// Parses the first parseArgCount arguments of the current process command line and returns
// them in parsedArgs, the rest of the command line is returned untouched.
LPCWSTR LaunchCommand::UntouchedCommandLineArguments(
  int parseArgCount, std::vector<std::wstring>& parsedArgs)
{
  LPCWSTR cmd = GetCommandLineW();
  LPCWSTR arg = nullptr; // to skip executable name
  for (; parseArgCount >= 0 && *cmd; ++cmd)
  {
    if (*cmd == '"') {
      int escaped = 0;
      for (++cmd; *cmd && (*cmd != '"' || escaped % 2 != 0); ++cmd)
        escaped = *cmd == '\\' ? escaped + 1 : 0;
    }
    if (*cmd == ' ') {
      if (arg)
        if (cmd-1 > arg && *arg == '"' && *(cmd-1) == '"')
          parsedArgs.push_back(std::wstring(arg+1, cmd-1));
        else
          parsedArgs.push_back(std::wstring(arg, cmd));
      arg = cmd + 1;
      --parseArgCount;
    }
  }
  return cmd;
}

} // namespace

#include "commandline.h"
#include "env.h"

namespace cl
{

CommandLine::CommandLine()
{
  createOptions();
  m_commands.push_back(std::make_unique<CrashDumpCommand>());
}

int CommandLine::run(int argc, char** argv)
{
  try
  {
    po::variables_map vm;

    auto parsed = po::command_line_parser(argc, argv)
      .options(m_allOptions)
      .positional(m_positional)
      .allow_unregistered()
      .run();

    po::store(parsed, vm);

    if (vm.count("command")) {
      const auto cmd = vm["command"].as<std::string>();

      for (auto&& c : m_commands) {
        if (c->name() == cmd) {
          auto co = c->options();
          co.add_options()
            ("help", "shows this message");

          auto opts = po::collect_unrecognized(
            parsed.options, po::include_positional);

          // remove the command name itself
          opts.erase(opts.begin());

          parsed = po::command_line_parser(opts)
            .options(co)
            .run();

          po::store(parsed, vm);

          if (vm.count("help")) {
            env::Console console;
            std::cout << usage(c.get()) << "\n";
            return 0;
          }

          return c->run(vm);
        }
      }
    }

    if (vm.count("help")) {
      env::Console console;
      std::cout << usage() << "\n";
      return 0;
    }

    return -1;
  }
  catch(po::error& e)
  {
    env::Console console;
    std::cerr << e.what() << "\n";
    std::cerr << usage() << "\n";
    return 1;
  }
}

void CommandLine::createOptions()
{
  m_visibleOptions.add_options()
    ("help", "shows this message");

  po::options_description options;
  options.add_options()
    ("command", po::value<std::string>(), "command to execute");

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

  return oss.str();
}


std::string Command::name() const
{
  return meta().name;
}

std::string Command::description() const
{
  return meta().description;
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

int Command::run(po::variables_map& vm)
{
  return doRun(vm);
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

int CrashDumpCommand::doRun(po::variables_map& vm)
{
  env::Console console;

  const auto typeString = vm["type"].as<std::string>();
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

} // namespace

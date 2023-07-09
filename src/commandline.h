#ifndef MODORGANIZER_COMMANDLINE_INCLUDED
#define MODORGANIZER_COMMANDLINE_INCLUDED
#include "moshortcut.h"
#include <memory>
#include <vector>

class OrganizerCore;
class MOApplication;
class MOMultiProcess;

namespace cl
{

namespace po = boost::program_options;

// base class for all commands
//
class Command
{
public:
  virtual ~Command() = default;

  // command name, used on the command line to invoke it; this is meta().name
  //
  std::string name() const;

  // command short description, shown in general help; this is
  // meta().description
  //
  std::string description() const;

  // usage line, puts together the name and whatever meta().usage is
  //
  std::string usageLine() const;

  // shown after the usage line; this is meta().more
  //
  std::string moreInfo() const;

  // returns all options for this command, including hidden ones; used to parse
  // the command line
  //
  po::options_description allOptions() const;

  // returns visible options, used to display usage
  //
  po::options_description visibleOptions() const;

  // returns positional arguments
  //
  po::positional_options_description positional() const;

  // when this returns true, the command line is not parsed at all and doRun()
  // is expected to use untouched()
  //
  virtual bool legacy() const;

  // remembers the given values
  //
  void set(const std::wstring& originalLine, po::variables_map vm,
           std::vector<std::wstring> untouched);

  // called as soon as the command line has been parsed; return something to
  // exit immediately
  //
  virtual std::optional<int> runEarly();

  // called as soon as the MOApplication has been created, which is also the
  // first time where Qt stuff is available
  //
  virtual std::optional<int> runPostApplication(MOApplication& a);

  // called as soon as the multi process checks have confirmed that this is
  // a primary instance; return something to exit immediately
  //
  virtual std::optional<int> runPostMultiProcess(MOMultiProcess& mp);

  // called after the OrganizerCore has been initialized; return something to
  // exit immediatley
  //
  virtual std::optional<int> runPostOrganizer(OrganizerCore& core);

  // whether this command can be forwarded to the primary instance if one is
  // already running
  //
  // if an instance is already running and this returns true, the whole command
  // line is sent as a message to the primary instance, which will construct a
  // new CommandLine and call runPostOrganizer() with it
  //
  // if an instance is already running and this returns false, the "an instance
  // is already running" error dialog will be shown
  //
  // if this is the primary instance, this function is not called
  //
  virtual bool canForwardToPrimary() const;

protected:
  // meta information about this command, returned by derived classes
  //
  struct Meta
  {
    std::string name, description, usage, more;

    Meta(std::string name, std::string description, std::string usage,
         std::string more);
  };

  // meta
  //
  virtual Meta meta() const = 0;

  // returns visible options specific to this command
  //
  virtual po::options_description getVisibleOptions() const;

  // returns hidden options specific to this command
  //
  virtual po::options_description getInternalOptions() const;

  // returns positional arguments specific to this command
  //
  virtual po::positional_options_description getPositional() const;

  // returns the original command line
  //
  const std::wstring& originalCmd() const;

  // variables
  //
  const po::variables_map& vm() const;

  // returns unparsed options, only used by launch
  //
  const std::vector<std::wstring>& untouched() const;

private:
  std::wstring m_original;
  po::variables_map m_vm;
  std::vector<std::wstring> m_untouched;
};

// generates a crash dump for another MO process
//
class CrashDumpCommand : public Command
{
protected:
  po::options_description getVisibleOptions() const override;
  Meta meta() const override;
  std::optional<int> runEarly() override;
};

// this is the `launch` command used when starting a process from within the
// virtualized directory, see processrunner.cpp
//
// it has its own parsing of the command line to extract the argument after
// `launch` and use it as the cwd of the process, but pass the remaining
// arguments verbatim
//
// this is very old code that should probably never be changed
//
// note that it's actually buggy; in particular, it doesn't handle multiple
// whitespace between arguments
//
class LaunchCommand : public Command
{
public:
  bool legacy() const override;

protected:
  Meta meta() const override;
  std::optional<int> runEarly() override;

  int SpawnWaitProcess(LPCWSTR workingDirectory, LPCWSTR commandLine);

  LPCWSTR UntouchedCommandLineArguments(int parseArgCount,
                                        std::vector<std::wstring>& parsedArgs);
};

// runs a program or an executable
//
class RunCommand : public Command
{
protected:
  Meta meta() const override;

  po::options_description getVisibleOptions() const override;
  po::options_description getInternalOptions() const override;
  po::positional_options_description getPositional() const override;

  bool canForwardToPrimary() const override;
  std::optional<int> runPostOrganizer(OrganizerCore& core) override;
};

// reloads the given plugin
//
class ReloadPluginCommand : public Command
{
protected:
  Meta meta() const override;

  po::options_description getInternalOptions() const override;
  po::positional_options_description getPositional() const override;

  bool canForwardToPrimary() const override;
  std::optional<int> runPostOrganizer(OrganizerCore& core) override;
};

// refreshes mo
//
class RefreshCommand : public Command
{
protected:
  Meta meta() const override;
  bool canForwardToPrimary() const override;
  std::optional<int> runPostOrganizer(OrganizerCore& core) override;
};

// parses the command line and runs any given command
//
// the command line used to support a few commands but with no real conventions;
// those are mostly preserved for backwards compatibility, but deprecated:
//
//   - moshortcut:// for desktop/taskbar shortcuts, may contain an instance name
//     and a configured executable or arbitrary binary (still used in MO for
//     shortcuts)
//
//   - nxm:// links
//
//   - the name of a configured executable or path to binary, followed by
//     arbitrary parameters, forwarded to the program
//
// any command added CommandLine will unfortunately break any executable with
// the same name: `ModOrganizer.exe run` used to launch a program named "run"
// but will now execute the command "run"
//
// if moshortcut:// is detected and has an instance, it will override -i if both
// are given
//
//
// the command is used in two phases:
//
//    1) run() is called in main() shortly after startup; it parses the command
//       line and runs the given command, if any
//
//    2) if the command did not request to exit, the instance is loaded
//       in main() et al. and setupCore() is called; it handles moshortcut,
//       nxm links and starting executables/binaries
//
// either can return an exit code, which will make MO exit immediately
//
class CommandLine
{
public:
  CommandLine();

  // parses the given command line and calls runEarly() on the appropriate
  // command, if any
  //
  // returns an empty optional if execution should continue, or a return code
  // if MO must quit
  //
  std::optional<int> process(const std::wstring& line);

  // called as soon as the MOApplication has been created; this handles a few
  // global actions and forwards to the command, if any
  //
  std::optional<int> runPostApplication(MOApplication& a);

  // calls Command::runPostMultiProcess() on the command, if any
  //
  std::optional<int> runPostMultiProcess(MOMultiProcess& mp);

  // calls Command::runPostOrganizer() on the command, if any
  //
  // if MO wasn't invoked with a valid command, this also handles moshortcut,
  // nxm links and starting processes
  //
  std::optional<int> runPostOrganizer(OrganizerCore& core);

  // called when this instance is not the primary one
  //
  // if a command was executed and the command returns true for
  // canForwardToPrimary(), this forwards the command line as an external
  // message and returns true
  //
  // if the command returns false for canForwardToPrimary(), returns false
  //
  // if there was no valid command on the command line, this also forwards
  // moshortcut and nxm links
  //
  // if there was no command, moshortcut or nxm links, this returns false, which
  // will end up displaying an error message about an instance already running
  //
  bool forwardToPrimary(MOMultiProcess& multiProcess);

  // clears parsed options, used when MO is "restarted" so the options aren't
  // processed again
  //
  void clear();

  // global usage string plus usage for the given command, if any
  //
  std::string usage(const Command* c = nullptr) const;

  // whether --pick was given
  //
  bool pick() const;

  // whether --multiple was given
  //
  bool multiple() const;

  // profile override (-p)
  //
  std::optional<QString> profile() const;

  // instance override (-i)
  //
  std::optional<QString> instance() const;

  // returns the data parsed from an moshortcut:// option, if any
  //
  const MOShortcut& shortcut() const;

  // returns the nxm:// link, if any
  //
  std::optional<QString> nxmLink() const;

  // returns the executable/binary, if any
  //
  std::optional<QString> executable() const;

  // returns the list of arguments, excluding moshortcut or executable name;
  // deprecated, only use with executable()
  //
  const QStringList& untouched() const;

private:
  po::options_description m_visibleOptions, m_allOptions;
  po::positional_options_description m_positional;
  std::vector<std::unique_ptr<Command>> m_commands;
  po::variables_map m_vm;
  MOShortcut m_shortcut;
  std::optional<QString> m_nxmLink;
  std::optional<QString> m_executable;
  QStringList m_untouched;
  Command* m_command;

  void createOptions();
  std::string more() const;

  template <class... Ts>
  void add()
  {
    (m_commands.push_back(std::make_unique<Ts>()), ...);
  }

  std::optional<int> runEarly();
};

}  // namespace cl

#endif  // MODORGANIZER_COMMANDLINE_INCLUDED

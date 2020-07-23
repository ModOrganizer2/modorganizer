#pragma once

#include "moshortcut.h"
#include <vector>
#include <memory>

namespace cl
{

namespace po = boost::program_options;


class Command
{
public:
  virtual ~Command() = default;

  std::string name() const;
  std::string description() const;
  std::string usageLine() const;

  po::options_description allOptions() const;
  po::options_description visibleOptions() const;
  po::positional_options_description positional() const;
  std::string usage() const;

  virtual bool allow_unregistered() const;

  std::optional<int> run(
    const std::wstring& originalLine,
    po::variables_map vm,
    std::vector<std::wstring> untouched);

protected:
  struct Meta
  {
    std::string name, description;
  };

  virtual std::string getUsageLine() const;
  virtual po::options_description getVisibleOptions() const;
  virtual po::options_description getInternalOptions() const;
  virtual po::positional_options_description getPositional() const;

  virtual Meta meta() const = 0;
  virtual std::optional<int> doRun() = 0;

  const std::wstring& originalCmd() const;
  const po::variables_map& vm() const;
  const std::vector<std::wstring>& untouched() const;

private:
  std::wstring m_original;
  po::variables_map m_vm;
  std::vector<std::wstring> m_untouched;
};


class CrashDumpCommand : public Command
{
protected:
  po::options_description getVisibleOptions() const override;
  Meta meta() const override;
  std::optional<int> doRun() override;
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
  bool allow_unregistered() const override;

protected:
  Meta meta() const override;
  std::optional<int> doRun() override;

  int SpawnWaitProcess(LPCWSTR workingDirectory, LPCWSTR commandLine);

  LPCWSTR UntouchedCommandLineArguments(
    int parseArgCount, std::vector<std::wstring>& parsedArgs);
};


class ExeCommand : public Command
{
protected:
  std::string getUsageLine() const override;
  po::options_description getVisibleOptions() const override;
  po::options_description getInternalOptions() const override;
  po::positional_options_description getPositional() const override;
  Meta meta() const override;
  std::optional<int> doRun() override;
};


class RunCommand : public Command
{
protected:
  po::options_description getOptions() const;
  Meta meta() const override;
  std::optional<int> doRun() override;
};


class CommandLine
{
public:
  CommandLine();

  std::optional<int> run(const std::wstring& line);
  void clear();

  std::string usage(const Command* c=nullptr) const;

  bool multiple() const;
  std::optional<QString> profile() const;
  std::optional<QString> instance() const;

  const MOShortcut& shortcut() const;
  std::optional<QString> nxmLink() const;
  std::optional<QString> executable() const;

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

  void createOptions();
  std::string more() const;
};

} // namespace

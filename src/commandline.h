#pragma once

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

  po::options_description options() const;
  std::string usage() const;

  int run(po::variables_map& vm);

protected:
  struct Meta
  {
    std::string name, description;
  };

  virtual po::options_description doOptions() const;
  virtual Meta meta() const = 0;
  virtual int doRun(po::variables_map& vm) = 0;
};


class CrashDumpCommand : public Command
{
protected:
  po::options_description doOptions() const;
  Meta meta() const override;
  int doRun(po::variables_map& vm) override;
};


class CommandLine
{
public:
  CommandLine();

  int run(int argc, char** argv);
  std::string usage(const Command* c=nullptr) const;

private:
  po::options_description m_visibleOptions, m_allOptions;
  po::positional_options_description m_positional;
  std::vector<std::unique_ptr<Command>> m_commands;

  void createOptions();
};

} // namespace

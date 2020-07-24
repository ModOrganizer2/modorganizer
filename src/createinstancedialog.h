#ifndef MODORGANIZER_CREATEINSTANCEDIALOG_INCLUDED
#define MODORGANIZER_CREATEINSTANCEDIALOG_INCLUDED

#include <QDialog>

namespace MOBase { class IPluginGame; }
namespace Ui { class CreateInstanceDialog; };
namespace cid { class Page; }

class PluginContainer;

class CreateInstanceDialog : public QDialog
{
  Q_OBJECT

public:
  enum Types
  {
    NoType = 0,
    Global,
    Portable
  };

  struct Paths
  {
    QString base;
    QString downloads;
    QString mods;
    QString profiles;
    QString overwrite;
    QString ini;

    auto operator<=>(const Paths&) const = default;
  };

  struct CreationInfo
  {
    Types type;
    MOBase::IPluginGame* game;
    QString gameLocation;
    QString gameEdition;
    QString instanceName;
    QString dataPath;
    QString iniPath;
    Paths paths;
  };


  explicit CreateInstanceDialog(
    const PluginContainer& pc, QWidget *parent = nullptr);

  ~CreateInstanceDialog();

  Ui::CreateInstanceDialog* getUI();
  const PluginContainer& pluginContainer();

  void next();
  void back();
  void selectPage(std::size_t i);
  void changePage(int d);
  void finish();

  void updateNavigation();
  bool isOnLastPage() const;

  Types instanceType() const;
  MOBase::IPluginGame* game() const;
  QString gameLocation() const;
  QString gameEdition() const;
  QString instanceName() const;
  QString dataPath() const;
  Paths paths() const;

  CreationInfo creationInfo() const;

private:
  std::unique_ptr<Ui::CreateInstanceDialog> ui;
  const PluginContainer& m_pc;
  std::vector<std::unique_ptr<cid::Page>> m_pages;
  QString m_originalNext;

  template <class T>
  T getSelected(T (cid::Page::*mf)() const) const
  {
    for (auto&& p : m_pages) {
      const auto t = (p.get()->*mf)();
      if (t != T()) {
        return t;
      }
    }

    return T();
  }

  void logCreation(const QString& s);
  void logCreation(const std::wstring& s);
};

#endif // MODORGANIZER_CREATEINSTANCEDIALOG_INCLUDED

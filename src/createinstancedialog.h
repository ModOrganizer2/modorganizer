#ifndef MODORGANIZER_CREATEINSTANCEDIALOG_INCLUDED
#define MODORGANIZER_CREATEINSTANCEDIALOG_INCLUDED

#include <QDialog>

namespace MOBase { class IPluginGame; }
namespace Ui { class CreateInstanceDialog; };
namespace cid { class Page; }

class PluginContainer;
class Settings;

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
    QString gameVariant;
    QString instanceName;
    QString dataPath;
    QString iniPath;
    Paths paths;
  };


  explicit CreateInstanceDialog(
    const PluginContainer& pc, Settings* s, QWidget *parent = nullptr);

  ~CreateInstanceDialog();

  Ui::CreateInstanceDialog* getUI();

  const PluginContainer& pluginContainer();
  Settings* settings();

  template <class Page>
  void setSinglePage()
  {
    for (auto&& p : m_pages) {
      if (auto* tp=dynamic_cast<Page*>(p.get())) {
        tp->setSkip(false);
      } else {
        p->setSkip(true);
      }
    }

    setSinglePageImpl();
  }

  template <class Page>
  Page* getPage()
  {
    for (auto&& p : m_pages) {
      if (auto* tp=dynamic_cast<Page*>(p.get())) {
        return tp;
      }
    }

    return nullptr;
  }

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
  QString gameVariant() const;
  QString instanceName() const;
  QString dataPath() const;
  Paths paths() const;
  bool switching() const;

  CreationInfo creationInfo() const;

private:
  std::unique_ptr<Ui::CreateInstanceDialog> ui;
  const PluginContainer& m_pc;
  Settings* m_settings;
  std::vector<std::unique_ptr<cid::Page>> m_pages;
  QString m_originalNext;
  bool m_switching;
  bool m_singlePage;


  void setSinglePageImpl();

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

  bool canNext() const;
  bool canBack() const;
};

#endif // MODORGANIZER_CREATEINSTANCEDIALOG_INCLUDED

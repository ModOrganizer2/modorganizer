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

  Types selectedType() const;
  MOBase::IPluginGame* selectedGame() const;
  QString instanceName() const;

private:
  std::unique_ptr<Ui::CreateInstanceDialog> ui;
  const PluginContainer& m_pc;
  std::vector<std::unique_ptr<cid::Page>> m_pages;
  QString m_originalNext;
};

#endif // MODORGANIZER_CREATEINSTANCEDIALOG_INCLUDED

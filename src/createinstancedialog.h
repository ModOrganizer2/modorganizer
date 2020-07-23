#ifndef MODORGANIZER_CREATEINSTANCEDIALOG_INCLUDED
#define MODORGANIZER_CREATEINSTANCEDIALOG_INCLUDED

#include <QDialog>

namespace Ui { class CreateInstanceDialog; };
namespace cid { class Page; }

class PluginContainer;

class CreateInstanceDialog : public QDialog
{
  Q_OBJECT

public:
  explicit CreateInstanceDialog(
    const PluginContainer& pc, QWidget *parent = nullptr);

  ~CreateInstanceDialog();

  Ui::CreateInstanceDialog* getUI();
  const PluginContainer& pluginContainer();

  void next();
  void back();

private:
  std::unique_ptr<Ui::CreateInstanceDialog> ui;
  const PluginContainer& m_pc;
  std::vector<std::unique_ptr<cid::Page>> m_pages;

  void updateNavigationButtons();
};

#endif // MODORGANIZER_CREATEINSTANCEDIALOG_INCLUDED

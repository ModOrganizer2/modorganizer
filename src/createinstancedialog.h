#ifndef MODORGANIZER_CREATEINSTANCEDIALOG_INCLUDED
#define MODORGANIZER_CREATEINSTANCEDIALOG_INCLUDED

#include <QDialog>

namespace Ui { class CreateInstanceDialog; };
namespace cid { class Page; }

class CreateInstanceDialog : public QDialog
{
  Q_OBJECT

public:
  explicit CreateInstanceDialog(QWidget *parent = nullptr);

  ~CreateInstanceDialog();

  Ui::CreateInstanceDialog* getUI();

  void next();
  void back();

private:
  std::unique_ptr<Ui::CreateInstanceDialog> ui;
  std::vector<std::unique_ptr<cid::Page>> m_pages;

  void updateNavigationButtons();
};

#endif // MODORGANIZER_CREATEINSTANCEDIALOG_INCLUDED

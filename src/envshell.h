#ifndef ENV_SHELL_H
#define ENV_SHELL_H

#include "env.h"
#include <QFileInfo>
#include <QPoint>

namespace env
{

class ShellMenu
{
public:
  ShellMenu() = default;

  // noncopyable
  ShellMenu(const ShellMenu&) = delete;
  ShellMenu& operator=(const ShellMenu&) = delete;
  ShellMenu(ShellMenu&&) = default;
  ShellMenu& operator=(ShellMenu&&) = default;

  void addFile(QFileInfo fi);

  void exec(QWidget* parent, const QPoint& pos);
  HMENU getMenu();

private:
  std::vector<QFileInfo> m_files;
  COMPtr<IContextMenu> m_cm;
  HMenuPtr m_menu;

  void createMenu();

  COMPtr<IShellItem> createShellItem(const std::wstring& path);
  COMPtr<IPersistIDList> getPersistIDList(IShellItem* item);
  CoTaskMemPtr<LPITEMIDLIST> getIDList(IPersistIDList* pidlist);
  std::vector<LPCITEMIDLIST> createIdls(const std::vector<QFileInfo>& files);
  COMPtr<IShellItemArray> createItemArray(std::vector<LPCITEMIDLIST>& idls);
  COMPtr<IContextMenu> createContextMenu(IShellItemArray* array);
  HMenuPtr createMenu(IContextMenu* cm);
  HMenuPtr createDummyMenu(const QString& what);

  int runMenu(QMainWindow* mw, IContextMenu* cm, HMENU menu, const QPoint& p);
  void invoke(QMainWindow* mw, const QPoint& p, int cmd, IContextMenu* cm);
};


class ShellMenuCollection
{
public:
  void add(QString name, ShellMenu m);
  void exec(QWidget* parent, const QPoint& pos);

private:
  struct MenuInfo
  {
    QString name;
    ShellMenu menu;
  };

  std::vector<MenuInfo> m_menus;
};

} // namespace

#endif // ENV_SHELL_H

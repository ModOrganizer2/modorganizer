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
  ShellMenu(QMainWindow* mw);

  // noncopyable
  ShellMenu(const ShellMenu&)            = delete;
  ShellMenu& operator=(const ShellMenu&) = delete;
  ShellMenu(ShellMenu&&)                 = default;
  ShellMenu& operator=(ShellMenu&&)      = default;

  void addFile(QFileInfo fi);
  int fileCount() const;

  void exec(const QPoint& pos);
  HMENU getMenu();
  bool wndProc(HWND hwnd, UINT m, WPARAM wp, LPARAM lp, LRESULT* out);
  void invoke(const QPoint& p, int cmd);

private:
  QMainWindow* m_mw;
  std::vector<QFileInfo> m_files;
  COMPtr<IContextMenu> m_cm;
  COMPtr<IContextMenu2> m_cm2;
  COMPtr<IContextMenu3> m_cm3;
  HMenuPtr m_menu;

  void create();

  std::vector<LPCITEMIDLIST> createIdls(const std::vector<QFileInfo>& files);
  COMPtr<IShellItemArray> createItemArray(std::vector<LPCITEMIDLIST>& idls);

  void createContextMenu(IShellItemArray* array);
  void createPopupMenu(IContextMenu* cm);

  COMPtr<IShellItem> createShellItem(const std::wstring& path);
  COMPtr<IPersistIDList> getPersistIDList(IShellItem* item);
  CoTaskMemPtr<LPITEMIDLIST> getIDList(IPersistIDList* pidlist);
  HMenuPtr createDummyMenu(const QString& what);

  void onMenuSelect(HWND hwnd, HMENU hmenu, int item, HMENU hmenuPopup, UINT flags);
};

class ShellMenuCollection
{
public:
  ShellMenuCollection(QMainWindow* mw);

  void addDetails(QString s);
  void add(QString name, ShellMenu m);

  void exec(const QPoint& pos);

private:
  struct MenuInfo
  {
    QString name;
    ShellMenu menu;
  };

  QMainWindow* m_mw;
  std::vector<QString> m_details;
  std::vector<MenuInfo> m_menus;
  MenuInfo* m_active;

  bool wndProc(HWND hwnd, UINT m, WPARAM wp, LPARAM lp, LRESULT* out);

  void onMenuSelect(HWND hwnd, HMENU hmenu, int item, HMENU hmenuPopup, UINT flags);
};

}  // namespace env

#endif  // ENV_SHELL_H

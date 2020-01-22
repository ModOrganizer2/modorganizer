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
  void addFile(QFileInfo fi);
  void exec(QWidget* parent, const QPoint& pos);

private:
  std::vector<QFileInfo> m_files;

  QMainWindow* getMainWindow(QWidget* w);
  COMPtr<IShellItem> createShellItem(const std::wstring& path);
  COMPtr<IPersistIDList> getPersistIDList(IShellItem* item);
  CoTaskMemPtr<LPITEMIDLIST> getIDList(IPersistIDList* pidlist);
  std::vector<LPCITEMIDLIST> createIdls(const std::vector<QFileInfo>& files);
  COMPtr<IShellItemArray> createItemArray(std::vector<LPCITEMIDLIST>& idls);
  COMPtr<IContextMenu> createContextMenu(IShellItemArray* array);
  HMenuPtr createMenu(IContextMenu* cm);
  int runMenu(QMainWindow* mw, IContextMenu* cm, HMENU menu, const QPoint& p);
  void invoke(QMainWindow* mw, const QPoint& p, int cmd, IContextMenu* cm);
};

} // namespace

#endif // ENV_SHELL_H

#ifndef ENV_SHELL_H
#define ENV_SHELL_H

#include <QFileInfo>
#include <QPoint>

namespace env
{

void showShellMenu(
  QWidget* parent, const QFileInfo& file, const QPoint& pos);

void showShellMenu(
  QWidget* parent, const std::vector<QFileInfo>& files, const QPoint& pos);

}

#endif // ENV_SHELL_H

#include "pluginflagicondelegate.h"
#include "pluginlist.h"
#include <QList>


PluginFlagIconDelegate::PluginFlagIconDelegate(QObject *parent)
  : IconDelegate(parent)
{
}

QList<QIcon> PluginFlagIconDelegate::getIcons(const QModelIndex &index) const
{
  QList<QIcon> result;
  foreach (const QVariant &var, index.data(Qt::UserRole + 1).toList()) {
    result.append(var.value<QIcon>());
  }
  return result;
}

size_t PluginFlagIconDelegate::getNumIcons(const QModelIndex &index) const
{
  return index.data(Qt::UserRole + 1).toList().count();
}


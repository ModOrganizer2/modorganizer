#ifndef PLUGINFLAGICONDELEGATE_H
#define PLUGINFLAGICONDELEGATE_H

#include "icondelegate.h"

class PluginFlagIconDelegate : public IconDelegate
{
public:
  PluginFlagIconDelegate(QObject *parent = NULL);

  // IconDelegate interface
private:
  virtual QList<QIcon> getIcons(const QModelIndex &index) const;
  virtual size_t getNumIcons(const QModelIndex &index) const;
};

#endif // PLUGINFLAGICONDELEGATE_H

#ifndef MODFLAGICONDELEGATE_H
#define MODFLAGICONDELEGATE_H

#include "icondelegate.h"

class ModFlagIconDelegate : public IconDelegate
{
public:
  explicit ModFlagIconDelegate(QObject *parent = 0);
private:
  virtual QList<QIcon> getIcons(const QModelIndex &index) const;
  virtual size_t getNumIcons(const QModelIndex &index) const;

  QIcon getFlagIcon(ModInfo::EFlag flag) const;

};

#endif // MODFLAGICONDELEGATE_H

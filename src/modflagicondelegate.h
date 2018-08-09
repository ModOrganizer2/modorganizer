#ifndef MODFLAGICONDELEGATE_H
#define MODFLAGICONDELEGATE_H

#include "icondelegate.h"

class ModFlagIconDelegate : public IconDelegate
{
public:
  explicit ModFlagIconDelegate(QObject *parent = 0);
  virtual QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const;
private:
  virtual QList<QString> getIcons(const QModelIndex &index) const;
  virtual size_t getNumIcons(const QModelIndex &index) const;

  QString getFlagIcon(ModInfo::EFlag flag) const;
private:
  static ModInfo::EFlag m_ConflictFlags[4];
  static ModInfo::EFlag m_ArchiveLooseConflictFlags[3];
  static ModInfo::EFlag m_ArchiveConflictFlags[3];
};

#endif // MODFLAGICONDELEGATE_H

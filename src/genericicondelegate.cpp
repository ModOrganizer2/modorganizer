#include "genericicondelegate.h"
#include "pluginlist.h"
#include <QList>


GenericIconDelegate::GenericIconDelegate(QObject *parent, int role, int logicalIndex, int compactSize)
  : IconDelegate(parent)
  , m_Role(role)
  , m_LogicalIndex(logicalIndex)
  , m_CompactSize(compactSize)
  , m_Compact(false)
{
}

void GenericIconDelegate::columnResized(int logicalIndex, int, int newSize)
{
  if (logicalIndex == m_LogicalIndex) {
    m_Compact = newSize < m_CompactSize;
  }
}

QList<QIcon> GenericIconDelegate::getIcons(const QModelIndex &index) const
{
  QList<QIcon> result;
  if (index.isValid()) {
    foreach (const QVariant &var, index.data(m_Role).toList()) {
      QIcon icon = var.value<QIcon>();
      if (!m_Compact || !icon.isNull()) {
        result.append(icon);
      }
    }
  }
  return result;
}

size_t GenericIconDelegate::getNumIcons(const QModelIndex &index) const
{
  return index.data(m_Role).toList().count();
}

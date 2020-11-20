#include "genericicondelegate.h"
#include "pluginlist.h"

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

QList<QString> GenericIconDelegate::getIcons(const QModelIndex &index) const
{
  QList<QString> result;
  if (index.isValid()) {
    for (const QVariant &var : index.data(m_Role).toList()) {
      if (!m_Compact || !var.toString().isEmpty()) {
        result.append(var.toString());
      }
    }
  }
  return result;
}

size_t GenericIconDelegate::getNumIcons(const QModelIndex &index) const
{
  return index.data(m_Role).toList().count();
}

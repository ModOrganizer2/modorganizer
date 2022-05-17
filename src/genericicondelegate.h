#ifndef GENERICICONDELEGATE_H
#define GENERICICONDELEGATE_H

#include "icondelegate.h"

/**
 * @brief an icon delegate that takes the list of icons from a user-defines data role
 */
class GenericIconDelegate : public IconDelegate
{
  Q_OBJECT
public:
  /**
   * @brief constructor
   * @param parent parent object
   * @param role role of the itemmodel from which the icon list can be queried (as a
   * QVariantList)
   * @param logicalIndex logical index within the model. This is part of a "hack".
   * Normally "empty" icons will be allocated the same space as a regular icon. This way
   * the model can use empty icons as spacers and thus align same icons horizontally.
   *                     Now, if you set the logical Index to a valid column and connect
   * the columnResized slot to the sectionResized signal of the view, the delegate will
   * turn off this behaviour if the column is smaller than "compactSize"
   * @param compactSize see explanation of logicalIndex
   */
  GenericIconDelegate(QTreeView* parent, int role = Qt::UserRole + 1,
                      int logicalIndex = -1, int compactSize = 150);

private:
  virtual QList<QString> getIcons(const QModelIndex& index) const;
  virtual size_t getNumIcons(const QModelIndex& index) const;

private:
  int m_Role;
  int m_LogicalIndex;
  int m_CompactSize;
  bool m_Compact;
};

#endif  // GENERICICONDELEGATE_H

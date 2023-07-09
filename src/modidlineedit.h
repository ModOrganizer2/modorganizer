#ifndef MODIDLINEEDIT_H
#define MODIDLINEEDIT_H

#include <QLineEdit>

class ModIDLineEdit : public QLineEdit
{
  Q_OBJECT

public:
  explicit ModIDLineEdit(QWidget* parent = 0);
  explicit ModIDLineEdit(const QString& text, QWidget* parent = 0);

public:
  virtual bool event(QEvent* event) override;

signals:
  void linkClicked(QString);
};

#endif  // MODIDLINEEDIT_H

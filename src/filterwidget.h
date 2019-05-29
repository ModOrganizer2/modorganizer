#ifndef FILTERWIDGET_H
#define FILTERWIDGET_H

class FilterWidget
{
public:
  FilterWidget();

  void set(QLineEdit* edit);
  void buddy(QWidget* w);

  void clear();

private:
  QLineEdit* m_edit;
  QToolButton* m_clear;
  QWidget* m_buddy;

  void createClear();
  void hookEvents();
  void repositionClearButton();

  void onTextChanged();
  void onResized();
};

#endif // FILTERWIDGET_H

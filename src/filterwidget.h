#ifndef FILTERWIDGET_H
#define FILTERWIDGET_H

class EventFilter;

class FilterWidget
{
public:
  std::function<void ()> changed;

  FilterWidget();

  void set(QLineEdit* edit);
  void clear();

  bool matches(const QString& s) const;

private:
  QLineEdit* m_edit;
  EventFilter* m_eventFilter;
  QToolButton* m_clear;
  QString m_text;

  void unhook();
  void createClear();
  void hookEvents();
  void repositionClearButton();

  void onTextChanged();
  void onResized();
};

#endif // FILTERWIDGET_H

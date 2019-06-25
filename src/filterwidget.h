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

  bool matches(std::function<bool (const QString& what)> pred) const;

private:
  QLineEdit* m_edit;
  EventFilter* m_eventFilter;
  QToolButton* m_clear;
  QString m_text;
  QList<QList<QString>> m_compiled;

  void unhook();
  void createClear();
  void hookEvents();
  void repositionClearButton();

  void onTextChanged();
  void onResized();

  void compile();
};

#endif // FILTERWIDGET_H

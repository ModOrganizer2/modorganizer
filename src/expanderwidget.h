#ifndef EXPANDERWIDGET_H
#define EXPANDERWIDGET_H

#include <QToolButton>

/* Takes a QToolButton and a widget and creates an expandable widget.
**/
class ExpanderWidget
{
public:
  /** empty expander, use set()
  **/
  ExpanderWidget();

  /** see set()
  **/
  ExpanderWidget(QToolButton* button, QWidget* content);

  /** @brief sets the button and content widgets to use
  * the button will be given an arrow icon, clicking it will toggle the
  * visibility of the given widget
  * @param button the button that toggles the content
  * @param content the widget that will be shown or hidden
  * @param opened initial state, defaults to closed
  **/
  void set(QToolButton* button, QWidget* content, bool opened=false);

  /** either opens or closes the expander depending on the current state
  **/
  void toggle();

  /** sets the current state of the expander
  **/
  void toggle(bool b);

  /** returns whether the expander is currently opened
  **/
  bool opened() const;

private:
  QToolButton* m_button;
  QWidget* m_content;
  bool opened_;
};

#endif // EXPANDERWIDGET_H

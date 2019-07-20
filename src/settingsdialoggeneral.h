#ifndef SETTINGSDIALOGGENERAL_H
#define SETTINGSDIALOGGENERAL_H

#include "settingsdialog.h"
#include "settings.h"

class GeneralSettingsTab : public SettingsTab
{
public:
  GeneralSettingsTab(Settings *m_parent, SettingsDialog &m_dialog);

  void update();

private:
  QColor m_OverwritingColor;
  QColor m_OverwrittenColor;
  QColor m_OverwritingArchiveColor;
  QColor m_OverwrittenArchiveColor;
  QColor m_ContainsColor;
  QColor m_ContainedColor;

  void addLanguages();
  void addStyles();
  void resetDialogs();
  void setButtonColor(QPushButton *button, const QColor &color);

  QColor getOverwritingColor() { return m_OverwritingColor; }
  QColor getOverwrittenColor() { return m_OverwrittenColor; }
  QColor getOverwritingArchiveColor() { return m_OverwritingArchiveColor; }
  QColor getOverwrittenArchiveColor() { return m_OverwrittenArchiveColor; }
  QColor getContainsColor() { return m_ContainsColor; }
  QColor getContainedColor() { return m_ContainedColor; }

  void setOverwritingColor(QColor col) { m_OverwritingColor = col; }
  void setOverwrittenColor(QColor col) { m_OverwrittenColor = col; }
  void setOverwritingArchiveColor(QColor col) { m_OverwritingArchiveColor = col; }
  void setOverwrittenArchiveColor(QColor col) { m_OverwrittenArchiveColor = col; }
  void setContainsColor(QColor col) { m_ContainsColor = col; }
  void setContainedColor(QColor col) { m_ContainedColor = col; }

  void on_overwritingArchiveBtn_clicked();
  void on_overwritingBtn_clicked();
  void on_overwrittenArchiveBtn_clicked();
  void on_overwrittenBtn_clicked();
  void on_containedBtn_clicked();
  void on_containsBtn_clicked();

  void on_categoriesBtn_clicked();
  void on_resetColorsBtn_clicked();
  void on_resetDialogsButton_clicked();
};

#endif // SETTINGSDIALOGGENERAL_H

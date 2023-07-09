#ifndef COLORTABLE_H
#define COLORTABLE_H

#include <QTableWidget>

class Settings;

// a QTableWidget to view and modify color settings
//
class ColorTable : public QTableWidget
{
public:
  ColorTable(QWidget* parent = nullptr);

  // adds colors to the table from the settings
  //
  void load(Settings& s);

  // resets the colors to their default values; commitColors() must be called
  // to save them
  //
  void resetColors();

  // commits any changes
  //
  void commitColors();

private:
  Settings* m_settings;

  void addColor(const QString& text, const QColor& defaultColor,
                std::function<QColor()> get, std::function<void(const QColor&)> commit);

  void onColorActivated();
};

#endif  // COLORTABLE_H

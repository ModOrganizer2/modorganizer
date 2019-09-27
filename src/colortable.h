#ifndef COLORTABLE_H
#define COLORTABLE_H

#include <QTableWidget>

class Settings;

class ColorTable : public QTableWidget
{
public:
  ColorTable(QWidget* parent=nullptr);

  void load(Settings& s);
  void resetColors();
  void commitColors();

private:
  Settings* m_settings;

  void addColor(
    const QString& text, const QColor& defaultColor,
    std::function<QColor ()> get,
    std::function<void (const QColor&)> commit);

  void onColorActivated();
};

#endif // COLORTABLE_H

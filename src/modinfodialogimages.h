#ifndef MODINFODIALOGIMAGES_H
#define MODINFODIALOGIMAGES_H

#include "modinfodialogtab.h"

class ScalableImage : public QWidget
{
  Q_OBJECT;

public:
  ScalableImage(QImage image={});

  void setImage(QImage image);
  const QImage& image() const;

  bool hasHeightForWidth() const;
  int heightForWidth(int w) const;

signals:
    void clicked(const QImage& image);

protected:
  void paintEvent(QPaintEvent* e) override;
  void mousePressEvent(QMouseEvent* e) override;

private:
  QImage m_original, m_scaled;
  int m_border;
};


class ImagesTab : public ModInfoDialogTab
{
  Q_OBJECT;

public:
  ImagesTab(
    OrganizerCore& oc, PluginContainer& plugin,
    QWidget* parent, Ui::ModInfoDialog* ui, int index);

  void clear() override;
  bool feedFile(const QString& rootPath, const QString& fullPath) override;

private:
  ScalableImage* m_image;

  void add(const QString& fullPath);
  void onClicked(const QImage& image);
};

#endif // MODINFODIALOGIMAGES_H

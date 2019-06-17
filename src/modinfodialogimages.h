#ifndef MODINFODIALOGIMAGES_H
#define MODINFODIALOGIMAGES_H

#include "modinfodialog.h"

class ThumbnailButton : public QPushButton
{
  Q_OBJECT;

public:
  ThumbnailButton(const QString& fullPath, QImage image);
  const QImage& image() const;

signals:
    void open(const QImage& image);

private:
  const QImage m_original;
};


class ImagesTab : public ModInfoDialogTab
{
public:
  ImagesTab(Ui::ModInfoDialog* ui);

  void clear() override;
  bool feedFile(const QString& rootPath, const QString& fullPath) override;

private:
  Ui::ModInfoDialog* ui;

  void add(const QString& fullPath);
  void onOpen(const QImage& image);
};

#endif // MODINFODIALOGIMAGES_H

#ifndef MODINFODIALOGIMAGES_H
#define MODINFODIALOGIMAGES_H

#include "modinfodialogtab.h"
#include <QScrollArea>

class ImagesTab;

class ImagesScrollArea : public QScrollArea
{
  Q_OBJECT;

public:
  using QScrollArea::QScrollArea;
  void setTab(ImagesTab* tab);

protected:
  void resizeEvent(QResizeEvent* e) override;

private:
  ImagesTab* m_tab = nullptr;
};


class ImagesThumbnails : public QWidget
{
  Q_OBJECT;

public:
  using QWidget::QWidget;
  void setTab(ImagesTab* tab);

protected:
  void paintEvent(QPaintEvent* e) override;
  void mousePressEvent(QMouseEvent* e) override;

private:
  ImagesTab* m_tab = nullptr;
};


class ScalableImage : public QWidget
{
  Q_OBJECT;

public:
  ScalableImage(QString path={});

  void setImage(const QString& path);
  void setImage(QImage image);
  void clear();

  bool hasHeightForWidth() const;
  int heightForWidth(int w) const;

protected:
  void paintEvent(QPaintEvent* e) override;

private:
  QString m_path;
  QImage m_original, m_scaled;
  int m_border;
};


class ImagesTab : public ModInfoDialogTab
{
  Q_OBJECT;
  friend class ImagesScrollArea;
  friend class ImagesThumbnails;

public:
  ImagesTab(
    OrganizerCore& oc, PluginContainer& plugin,
    QWidget* parent, Ui::ModInfoDialog* ui, int id);

  void clear() override;
  bool feedFile(const QString& rootPath, const QString& fullPath) override;
  void update() override;

private:
  struct File
  {
    QString path;
    QImage original, thumbnail;
    bool failed = false;

    File(QString path)
      : path(std::move(path))
    {
    }
  };

  ScalableImage* m_image;
  std::vector<File> m_files;
  int m_margins, m_padding, m_border;

  void scrollAreaResized(const QSize& s);
  void paintThumbnails(QPaintEvent* e);
  void thumbnailsMouseEvent(QMouseEvent* e);

  bool needsReload(const File& file, const QSize& thumbSize) const;
  QSize scaledImageSize(const QSize& originalSize, const QSize& thumbSize) const;
};

#endif // MODINFODIALOGIMAGES_H

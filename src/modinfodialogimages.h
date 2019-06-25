#ifndef MODINFODIALOGIMAGES_H
#define MODINFODIALOGIMAGES_H

#include "modinfodialogtab.h"
#include "filterwidget.h"
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

  struct PaintContext
  {
    QPainter painter;
    int thumbSize;
    QRect topRect;

    PaintContext(QWidget* w)
      : painter(w), thumbSize(0)
    {
    }
  };

  ScalableImage* m_image;
  std::vector<File> m_files;
  std::vector<File*> m_filteredFiles;
  std::vector<QString> m_supportedFormats;
  int m_margins, m_padding, m_border;
  const File* m_selection;
  FilterWidget m_filter;

  void getSupportedFormats();
  void select(const File* file);

  void scrollAreaResized(const QSize& s);
  void paintThumbnails(QPaintEvent* e);
  void thumbnailsMouseEvent(QMouseEvent* e);
  void onExplore();
  void onFilterChanged();

  int calcThumbSize(int availableWidth) const;
  int calcWidgetHeight(int availableWidth) const;
  QRect calcTopThumbRect(int thumbSize) const;
  std::pair<std::size_t, std::size_t> calcVisibleRange(
    int top, int bottom, int thumbSize) const;

  QRect calcBorderRect(const QRect& topRect, int thumbSize, std::size_t i) const;
  QRect calcImageRect(const QRect& topRect, int thumbSize, std::size_t i) const;
  QSize calcScaledImageSize(
    const QSize& originalSize, const QSize& imageSize) const;

  void paintThumbnail(PaintContext& cx, std::size_t i);
  void paintThumbnailBorder(PaintContext& cx, std::size_t i);
  void paintThumbnailImage(PaintContext& cx, std::size_t i);

  const File* fileAtPos(const QPoint& p) const;

  std::size_t fileCount() const;
  const File* getFile(std::size_t i) const;
  File* getFile(std::size_t i);

  void filterImages();
  bool needsReload(const File& file, const QSize& imageSize) const;
  void reload(File& file, const QSize& imageSize);
  void resizeWidget();
};

#endif // MODINFODIALOGIMAGES_H

#ifndef MODINFODIALOGIMAGES_H
#define MODINFODIALOGIMAGES_H

#include "modinfodialogtab.h"
#include "filterwidget.h"
#include <QScrollBar>

class ImagesTab;

// vertical scrollbar, this is only to handle wheel events to scroll by one
// instead of the system's scroll setting
//
class ImagesScrollbar : public QScrollBar
{
public:
  using QScrollBar::QScrollBar;
  void setTab(ImagesTab* tab);

protected:
  // forwards to ImagesTab::thumbnailAreaWheelEvent()
  //
  void wheelEvent(QWheelEvent* event) override;

private:
  ImagesTab* m_tab = nullptr;
};


// widget inside the scroller, calls ImagesTab::paintThumbnailArea() when
// needed and also forwards mouse clicks and tooltip events
//
class ImagesThumbnails : public QWidget
{
  Q_OBJECT;

public:
  using QWidget::QWidget;
  void setTab(ImagesTab* tab);

protected:
  // forwards to ImagesTab::paintThumbnailArea()
  //
  void paintEvent(QPaintEvent* e) override;

  // forwards to ImagesTab::thumbnailAreaMouseEvent()
  //
  void mousePressEvent(QMouseEvent* e) override;

  // forwards to ImagesTab::thumbnailAreaWheelEvent()
  //
  void wheelEvent(QWheelEvent* e);

  // forwards to ImagesTab::scrollAreaResized()
  //
  void resizeEvent(QResizeEvent* e) override;

  // forwards to ImagesTab::thumbnailAreaKeyPressEvent()
  //
  void keyPressEvent(QKeyEvent* e) override;

  // forwards to ImagesTab::showTooltip for tooltip events
  //
  bool event(QEvent* e) override;

private:
  ImagesTab* m_tab = nullptr;
};


// a widget that draws an image scaled to fit while keeping the aspect ratio
//
class ScalableImage : public QWidget
{
  Q_OBJECT;

public:
  ScalableImage(QString path={});

  // sets the image to draw
  void setImage(const QString& path);
  void setImage(QImage image);

  // removes the image, won't draw the border nor the image
  void clear();

  // tells the QWidget's layout manager this widget is always square
  bool hasHeightForWidth() const override;
  int heightForWidth(int w) const override;

  // sets the colors
  void setColors(const QColor& border, const QColor& background);

protected:
  void paintEvent(QPaintEvent* e) override;

private:
  QString m_path;
  QImage m_original, m_scaled;
  int m_border;
  QColor m_borderColor, m_backgroundColor;
};


// handles all the geometry calculations by ImagesTab for painting or handling
// mouse clicks
//
// a thumbnail looks like this:
//
//   +-----------------------+ <-- thumb rect
//   |   margins             |
//   |                       |
//   |   +-border--------+ <------ border rect
//   |   | padding       |   |
//   |   |               |   |
//   |   |   +-------+ <----------- image rect
//   |   |   |       |   |   |
//   |   |   | image |   |   |
//   |   |   |       |   |   |
//   |   |   +-------+   |   |
//   |   |               |   |
//   |   +---------------+   |
//   |                       |
//   +-----------------------+
//
//   spacing
//
//   +-----------------------+ <-- thumb rect
//   |   margins             |
//   |                       |
//     ....
//
//
class ImagesGeometry
{
public:
  // returned by indexAt() if the point is outside any possible thumbnail
  static constexpr std::size_t BadIndex =
    std::numeric_limits<std::size_t>::max();

  ImagesGeometry(
    const QSize& widgetSize, int margins, int border, int padding, int spacing);

  // returns the number of images fully visible in the widget
  //
  std::size_t fullyVisibleCount() const;

  // rectangle around the whole thumbnail
  //
  QRect thumbRect(std::size_t i) const;

  // rectangle of the border for the given thumbnail
  //
  QRect borderRect(std::size_t i) const;

  // rectangle of the image for the given thumbnail
  //
  QRect imageRect(std::size_t i) const;

  // returns the index of the image at the given point; this does not take into
  // account any scrolling, the image at the top of widget is always 0
  //
  std::size_t indexAt(const QPoint& p) const;

  // returns the size of the image that fits in imageRect() while keeping the
  // same aspect ratio as the given one
  //
  QSize scaledImageSize(const QSize& originalSize) const;

  // dumps stuff to qDebug()
  //
  void dump() const;

private:
  // size of the widget containing all the thumbnails
  const QSize m_widgetSize;

  // space outside the thumbnail border
  const int m_margins;

  // size of the border
  const int m_border;

  // space between the border and the image
  const int m_padding;

  // spacing between thumbnails
  const int m_spacing;

  // rectangle of the first thumbnail on top
  const QRect m_topRect;


  // calculates the top rectangle
  //
  QRect calcTopRect() const;
};


class ImagesTab : public ModInfoDialogTab
{
  Q_OBJECT;
  friend class ImagesScrollbar;
  friend class ImagesThumbnails;

public:
  ImagesTab(
    OrganizerCore& oc, PluginContainer& plugin,
    QWidget* parent, Ui::ModInfoDialog* ui, int id);

  void clear() override;
  bool feedFile(const QString& rootPath, const QString& fullPath) override;
  void update() override;
  void saveState(Settings& s) override;
  void restoreState(const Settings& s) override;

private:
  static constexpr std::size_t BadIndex =
    std::numeric_limits<std::size_t>::max();

  struct File
  {
    QString path;
    QImage original, thumbnail;
    bool failed = false;

    File(QString path)
      : path(std::move(path))
    {
    }

    void ensureOriginalLoaded()
    {
      if (original.isNull()) {
        if (!original.load(path)) {
          qCritical() << "failed to load image from " << path;
          failed = true;
        }
      }
    }
  };

  struct Colors
  {
    QColor border, background, selection;
  };

  struct PaintContext
  {
    mutable QPainter painter;
    ImagesGeometry geo;
    File* file;
    std::size_t thumbIndex;
    std::size_t fileIndex;

    PaintContext(QWidget* w, ImagesGeometry geo)
      : painter(w), geo(geo), file(nullptr), thumbIndex(0), fileIndex(0)
    {
    }
  };

  class Files
  {
  public:
    Files();

    void clear();

    void add(File f);
    void addFiltered(File* f);

    bool empty() const;
    std::size_t size() const;

    void switchToAll();
    void switchToFiltered();

    const File* get(std::size_t i) const;
    File* get(std::size_t i);
    std::size_t indexOf(const File* f) const;

    const File* selectedFile() const;
    File* selectedFile();
    std::size_t selectedIndex() const;
    void select(std::size_t i);

    std::vector<File>& allFiles();

    bool isFiltered() const;

  private:
    std::vector<File> m_allFiles;
    std::vector<File*> m_filteredFiles;
    std::size_t m_selection;
    bool m_filtered;
  };


  ScalableImage* m_image;
  std::vector<QString> m_supportedFormats;
  Files m_files;
  int m_margins, m_border, m_padding, m_spacing;
  FilterWidget m_filter;
  bool m_ddsAvailable, m_ddsEnabled;
  Colors m_colors;

  void getSupportedFormats();
  void enableDDS(bool b);

  void scrollAreaResized(const QSize& s);
  void paintThumbnailsArea(QPaintEvent* e);
  void thumbnailAreaMouseEvent(QMouseEvent* e);
  void thumbnailAreaWheelEvent(QWheelEvent* e);
  bool thumbnailAreaKeyPressEvent(QKeyEvent* e);
  void onScrolled();

  void showTooltip(QHelpEvent* e);
  void onExplore();
  void onShowDDS();
  void onFilterChanged();

  void select(std::size_t i);
  void moveSelection(int by);
  void ensureVisible(std::size_t i);

  std::size_t fileIndexAtPos(const QPoint& p) const;
  const File* fileAtPos(const QPoint& p) const;

  ImagesGeometry makeGeometry() const;

  void paintThumbnail(const PaintContext& cx);
  void paintThumbnailBackground(const PaintContext& cx);
  void paintThumbnailBorder(const PaintContext& cx);
  void paintThumbnailImage(const PaintContext& cx);

  void checkFiltering();
  void switchToAll();
  void switchToFiltered();
  bool needsReload(const ImagesGeometry& geo, const File& file) const;
  void reload(const ImagesGeometry& geo, File& file);
  void updateScrollbar();
};

#endif // MODINFODIALOGIMAGES_H

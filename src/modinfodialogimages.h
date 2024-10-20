#ifndef MODINFODIALOGIMAGES_H
#define MODINFODIALOGIMAGES_H

#include "filterwidget.h"
#include "modinfodialogtab.h"
#include "organizercore.h"
#include <QScrollBar>

using namespace MOBase;

class ImagesTab;

namespace ImagesTabHelpers
{

static constexpr std::size_t BadIndex = std::numeric_limits<std::size_t>::max();

// vertical scrollbar, this is only to handle wheel events to scroll by one
// instead of the system's scroll setting
//
class Scrollbar : public QScrollBar
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
class ThumbnailsWidget : public QWidget
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
  ScalableImage(QString path = {});

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

struct Theme
{
  QColor borderColor, backgroundColor, textColor;
  QColor highlightBackgroundColor, highlightTextColor;
  QFont font;
};

struct Metrics
{
  // space outside the thumbnail border
  int margins;

  // size of the border
  int border;

  // space between the border and the image
  int padding;

  // spacing between the thumbnail and the text
  int textSpacing;

  // height of the text
  int textHeight;

  // spacing between thumbnails
  int spacing;

  Metrics();
};

// handles all the geometry calculations by ImagesTab for painting or handling
// mouse clicks
//
// a thumbnail looks like this:
//
//   +-----------------------+ <--- thumb rect
//   |   margins             |
//   |                       |
//   |   +-border--------+ <------- border rect
//   |   | padding       |   |
//   |   |               |   |
//   |   |   +-------+ <----------- image rect
//   |   |   |       |   |   |
//   |   |   | image |   |   |
//   |   |   |       |   |   |
//   |   |   +-------+   |   |
//   |   |               |   |
//   |   +---------------+   |
//   |      text spacing     |
//   |   +---------------+ <------- text rect
//   |   |     text      |   |
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
class Geometry
{
public:
  Geometry(QSize widgetSize, Metrics metrics);

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

  // rectangle of the text for the given thumbnail
  //
  QRect textRect(std::size_t i) const;

  // rectangle that responds to selection: includes the border and extends down
  // to the text
  //
  QRect selectionRect(std::size_t i) const;

  // returns the index of the image at the given point; this does not take into
  // account any scrolling, the image at the top of widget is always 0
  //
  // returns BadIndex if there's no thumbnail at this point
  //
  std::size_t indexAt(const QPoint& p) const;

  // returns the size of the image that fits in imageRect() while keeping the
  // same aspect ratio as the given one
  //
  QSize scaledImageSize(const QSize& originalSize) const;

private:
  // size of the widget containing all the thumbnails
  const QSize m_widgetSize;

  // metrics
  const Metrics m_metrics;

  // rectangle of the first thumbnail on top
  const QRect m_topRect;

  // calculates the top rectangle
  //
  QRect calcTopRect() const;
};

class File
{
public:
  File(QString path);

  void ensureOriginalLoaded();

  const QString& path() const;
  const QString& filename() const;
  const QImage& original() const;
  const QImage& thumbnail() const;
  bool failed() const;

  void loadIfNeeded(const Geometry& geo);

private:
  QString m_path;
  mutable QString m_filename;
  QImage m_original, m_thumbnail;
  bool m_failed;

  bool needsLoad(const Geometry& geo) const;
  void load(const Geometry& geo);
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

struct PaintContext
{
  mutable QPainter painter;
  Geometry geo;
  File* file;
  std::size_t thumbIndex;
  std::size_t fileIndex;

  PaintContext(QWidget* w, Geometry geo);
};

}  // namespace ImagesTabHelpers

class ImagesTab : public ModInfoDialogTab
{
  Q_OBJECT;
  friend class ImagesTabHelpers::Scrollbar;
  friend class ImagesTabHelpers::ThumbnailsWidget;

public:
  ImagesTab(ModInfoDialogTabContext cx);

  void clear() override;
  bool feedFile(const QString& rootPath, const QString& fullPath) override;
  void update() override;
  void saveState(Settings& s) override;
  void restoreState(const Settings& s) override;

private:
  enum class Visibility
  {
    Ignore = 0,
    Full,
    Partial
  };

  using ScalableImage = ImagesTabHelpers::ScalableImage;
  using Files         = ImagesTabHelpers::Files;
  using File          = ImagesTabHelpers::File;
  using Theme         = ImagesTabHelpers::Theme;
  using Metrics       = ImagesTabHelpers::Metrics;
  using PaintContext  = ImagesTabHelpers::PaintContext;
  using Geometry      = ImagesTabHelpers::Geometry;

  ScalableImage* m_image;
  std::vector<QString> m_supportedFormats;
  Files m_files;
  FilterWidget m_filter;
  bool m_ddsAvailable, m_ddsEnabled;
  Theme m_theme;
  Metrics m_metrics;

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
  void onPreviewButton();
  void onFilterChanged();

  void select(std::size_t i, Visibility v = Visibility::Full);
  void moveSelection(int by);
  void ensureVisible(std::size_t i, Visibility v);

  std::size_t fileIndexAtPos(const QPoint& p) const;
  const File* fileAtPos(const QPoint& p) const;

  Geometry makeGeometry() const;

  void paintThumbnail(const PaintContext& cx);
  void paintThumbnailBackground(const PaintContext& cx);
  void paintThumbnailBorder(const PaintContext& cx);
  void paintThumbnailImage(const PaintContext& cx);
  void paintThumbnailText(const PaintContext& cx);

  void checkFiltering();
  void switchToAll();
  void switchToFiltered();
  void updateScrollbar();
};

#endif  // MODINFODIALOGIMAGES_H

#include "modinfodialogimages.h"
#include "settings.h"
#include "ui_modinfodialog.h"
#include "utility.h"
#include <log.h>

using namespace MOBase;
using namespace ImagesTabHelpers;

QSize resizeWithAspectRatio(const QSize& original, const QSize& available)
{
  const auto ratio =
      std::min({1.0, static_cast<double>(available.width()) / original.width(),
                static_cast<double>(available.height()) / original.height()});

  const QSize scaledSize(static_cast<int>(std::round(original.width() * ratio)),
                         static_cast<int>(std::round(original.height() * ratio)));

  return scaledSize;
}

QRect centeredRect(const QRect& rect, const QSize& size)
{
  return QRect((rect.left() + rect.width() / 2) - size.width() / 2,
               (rect.top() + rect.height() / 2) - size.height() / 2, size.width(),
               size.height());
}

QString dimensionString(const QSize& s)
{
  return QString::fromUtf8("%1 \xc3\x97 %2").arg(s.width()).arg(s.height());
}

ImagesTab::ImagesTab(ModInfoDialogTabContext cx)
    : ModInfoDialogTab(std::move(cx)), m_image(new ScalableImage),
      m_ddsAvailable(false), m_ddsEnabled(false)
{
  getSupportedFormats();

  auto* ly = new QVBoxLayout(ui->imagesImage);
  ly->setContentsMargins({0, 0, 0, 0});
  ly->addWidget(m_image);

  delete ui->imagesThumbnails->layout();

  ui->tabImagesSplitter->setSizes({128, 1});
  ui->tabImagesSplitter->setStretchFactor(0, 0);
  ui->tabImagesSplitter->setStretchFactor(1, 1);

  ui->previewPluginButton->setEnabled(false);

  ui->imagesThumbnails->setTab(this);

  ui->imagesScrollerVBar->setTab(this);
  connect(ui->imagesScrollerVBar, &QScrollBar::valueChanged, [&] {
    onScrolled();
  });

  ui->imagesShowDDS->setEnabled(m_ddsAvailable);

  m_filter.setEdit(ui->imagesFilter);
  connect(&m_filter, &FilterWidget::changed, [&] {
    onFilterChanged();
  });

  connect(ui->imagesExplore, &QAbstractButton::clicked, [&] {
    onExplore();
  });
  connect(ui->imagesShowDDS, &QCheckBox::toggled, [&] {
    onShowDDS();
  });
  connect(ui->previewPluginButton, &QAbstractButton::clicked, [&] {
    onPreviewButton();
  });

  ui->imagesShowDDS->setEnabled(m_ddsAvailable);

  ui->imagesThumbnails->setAutoFillBackground(false);
  ui->imagesThumbnails->setAttribute(Qt::WA_OpaquePaintEvent, true);

  {
    auto list = std::make_unique<QListWidget>();
    parentWidget()->style()->polish(list.get());

    m_theme.borderColor     = QColor(Qt::black);
    m_theme.backgroundColor = QColor(Qt::black);
    m_theme.textColor       = list->palette().color(QPalette::WindowText);

    m_theme.highlightBackgroundColor = list->palette().color(QPalette::Highlight);
    m_theme.highlightTextColor       = list->palette().color(QPalette::HighlightedText);

    m_theme.font = list->font();

    const QFontMetrics fm(m_theme.font);
    m_metrics.textHeight = fm.height();

    m_image->setColors(m_theme.borderColor, m_theme.backgroundColor);
  }
}

void ImagesTab::clear()
{
  m_files.clear();
  ui->imagesScrollerVBar->setValue(0);
  select(BadIndex);
  setHasData(false);
}

bool ImagesTab::feedFile(const QString& rootPath, const QString& fullPath)
{
  for (const auto& ext : m_supportedFormats) {
    if (fullPath.endsWith(ext, Qt::CaseInsensitive)) {
      m_files.add({fullPath});
      return true;
    }
  }

  return false;
}

void ImagesTab::update()
{
  checkFiltering();
  updateScrollbar();

  // visibility needs to be rechecked here because the scrollbar configuration
  // may have changed in updateScrollbar(), in which case any ensureVisible()
  // calls in checkFiltering() might have been incorrect
  if (m_files.selectedIndex() != BadIndex) {
    ensureVisible(m_files.selectedIndex(), Visibility::Partial);
  }

  ui->imagesThumbnails->update();

  setHasData(m_files.size() > 0);
}

void ImagesTab::saveState(Settings& s)
{
  s.widgets().saveChecked(ui->imagesShowDDS);
  s.geometry().saveState(ui->tabImagesSplitter);
}

void ImagesTab::restoreState(const Settings& s)
{
  s.widgets().restoreChecked(ui->imagesShowDDS);
  s.geometry().restoreState(ui->tabImagesSplitter);
}

void ImagesTab::checkFiltering()
{
  if (m_filter.empty() && m_ddsEnabled) {
    // no filtering needed

    if (m_files.isFiltered()) {
      // was filtered, needs switch
      switchToAll();
    }
  } else {
    // filtering is needed
    switchToFiltered();
  }
}

void ImagesTab::switchToAll()
{
  // remember selection
  const auto* oldSelection = m_files.selectedFile();

  // switch
  m_files.switchToAll();

  // reselect old
  if (oldSelection) {
    select(m_files.indexOf(oldSelection));
  } else {
    select(BadIndex);
  }
}

void ImagesTab::switchToFiltered()
{
  // remember old selection, will be checked when building the filtered list
  // below
  const auto* oldSelection = m_files.selectedFile();
  std::size_t newSelection = BadIndex;

  // switch, also clears list
  m_files.switchToFiltered();

  const bool hasTextFilter = !m_filter.empty();

  for (File& f : m_files.allFiles()) {
    if (hasTextFilter) {
      // check filter widget
      const auto m = m_filter.matches([&](const QRegularExpression& regex) {
        return regex.match(f.filename()).hasMatch();
      });

      if (!m) {
        // no match, skip
        continue;
      }
    }

    if (!m_ddsEnabled) {
      // skip .dds files
      if (f.filename().endsWith(".dds", Qt::CaseInsensitive)) {
        continue;
      }
    }

    if (&f == oldSelection) {
      // found the old selection, remember its index
      newSelection = m_files.size();
    }

    m_files.addFiltered(&f);
  }

  // reselect old, or clear if it wasn't found
  select(newSelection);
}

void ImagesTab::getSupportedFormats()
{
  m_ddsAvailable = false;

  for (const auto& entry : QImageReader::supportedImageFormats()) {
    QString s(entry);
    if (s.isNull() || s.isEmpty()) {
      continue;
    }

    // used to enable the checkbox
    if (s.compare("dds", Qt::CaseInsensitive) == 0) {
      m_ddsAvailable = true;
    }

    // make sure it starts with a dot
    if (s[0] != '.') {
      s = '.' + s;
    }

    m_supportedFormats.emplace_back(std::move(s));
  }
}

void ImagesTab::select(std::size_t i, Visibility v)
{
  m_files.select(i);

  if (auto* f = m_files.selectedFile()) {
    // when jumping elsewhere in the list, such as with page down/up, the file
    // might not be visible yet, which means it hasn't been loaded and would
    // pass a null image in setImage() below
    f->ensureOriginalLoaded();

    ui->imagesPath->setText(QDir::toNativeSeparators(f->path()));
    ui->imagesExplore->setEnabled(true);
    if (plugins().previewGenerator().previewSupported(
            QFileInfo(f->path()).suffix().toLower()))
      ui->previewPluginButton->setEnabled(true);
    else
      ui->previewPluginButton->setEnabled(false);
    ui->imagesSize->setText(dimensionString(f->original().size()));

    if (f->original().isNull()) {
      m_image->clear();

      QImage image(300, 100, QImage::Format_RGBA64);
      QPainter paint;
      paint.begin(&image);
      paint.fillRect(0, 0, 300, 100, QBrush(QColor(0, 0, 0, 255)));
      paint.setPen(m_theme.textColor);
      paint.setFont(m_theme.font);
      paint.drawImage(QPoint(150 - 16, 50 - 20 - 16), QImage(":/MO/gui/warning"));
      const auto flags = Qt::AlignHCenter | Qt::AlignVCenter | Qt::TextWordWrap;
      paint.drawText(0, 46, 300, 54, flags,
                     "This image format is not supported by Qt, but the preview plugin "
                     "may be able to display it. Use the button above.");
      paint.end();

      m_image->setImage(image);
    } else
      m_image->setImage(f->original());
    ensureVisible(i, v);
  } else {
    ui->imagesPath->clear();
    ui->imagesExplore->setEnabled(false);
    ui->previewPluginButton->setEnabled(false);
    ui->imagesSize->clear();
    m_image->clear();
  }

  ui->imagesThumbnails->update();
}

void ImagesTab::moveSelection(int by)
{
  if (m_files.empty()) {
    return;
  }

  auto i = m_files.selectedIndex();
  if (i == BadIndex) {
    i = 0;
  }

  if (by > 0) {
    // moving down
    i += static_cast<std::size_t>(by);

    if (i >= m_files.size()) {
      i = (m_files.size() - 1);
    }
  } else if (by < 0) {
    // moving up
    const auto abs_by = static_cast<std::size_t>(std::abs(by));

    if (abs_by > i) {
      i = 0;
    } else {
      i -= abs_by;
    }
  }

  select(i);
}

void ImagesTab::ensureVisible(std::size_t i, Visibility v)
{
  if (v == Visibility::Ignore) {
    return;
  }

  const auto geo = makeGeometry();

  const auto fullyVisible     = geo.fullyVisibleCount();
  const auto partiallyVisible = fullyVisible + 1;

  const auto first = ui->imagesScrollerVBar->value();
  const auto last =
      (v == Visibility::Full ? first + fullyVisible : first + partiallyVisible);

  if (i < first) {
    // go up
    ui->imagesScrollerVBar->setValue(static_cast<int>(i));
  } else if (i >= last) {
    // go down

    if (i >= fullyVisible) {
      ui->imagesScrollerVBar->setValue(static_cast<int>(i - fullyVisible + 1));
    }
  }
}

std::size_t ImagesTab::fileIndexAtPos(const QPoint& p) const
{
  const auto geo = makeGeometry();

  // this is the index relative to the top
  const auto offset = geo.indexAt(p);
  if (offset == BadIndex) {
    return BadIndex;
  }

  const auto first = ui->imagesScrollerVBar->value();
  if (first < 0) {
    return BadIndex;
  }

  const auto i = static_cast<std::size_t>(first) + offset;
  if (i >= m_files.size()) {
    return BadIndex;
  }

  return i;
}

const File* ImagesTab::fileAtPos(const QPoint& p) const
{
  const auto i = fileIndexAtPos(p);
  if (i >= m_files.size()) {
    return nullptr;
  }

  return m_files.get(i);
}

Geometry ImagesTab::makeGeometry() const
{
  return Geometry(ui->imagesThumbnails->size(), m_metrics);
}

void ImagesTab::paintThumbnailsArea(QPaintEvent* e)
{
  PaintContext cx(ui->imagesThumbnails, makeGeometry());

  cx.painter.fillRect(ui->imagesThumbnails->rect(),
                      ui->imagesThumbnails->palette().color(QPalette::Window));

  const auto visible = cx.geo.fullyVisibleCount() + 1;
  const auto first   = ui->imagesScrollerVBar->value();

  for (std::size_t i = 0; i < visible; ++i) {
    const auto fileIndex = first + i;
    auto* file           = m_files.get(fileIndex);
    if (!file) {
      break;
    }

    cx.file       = file;
    cx.thumbIndex = i;
    cx.fileIndex  = fileIndex;

    paintThumbnail(cx);
  }
}

void ImagesTab::paintThumbnail(const PaintContext& cx)
{
  paintThumbnailBackground(cx);
  paintThumbnailBorder(cx);
  paintThumbnailImage(cx);
  paintThumbnailText(cx);
}

void ImagesTab::paintThumbnailBackground(const PaintContext& cx)
{
  if (m_files.selectedIndex() == cx.fileIndex) {
    const auto rect = cx.geo.thumbRect(cx.thumbIndex);
    cx.painter.fillRect(rect, m_theme.highlightBackgroundColor);
  }
}

void ImagesTab::paintThumbnailBorder(const PaintContext& cx)
{
  auto borderRect = cx.geo.borderRect(cx.thumbIndex);

  // rects don't include the bottom right corner, but drawRect() does, so
  // resize it
  borderRect.setRight(borderRect.right() - 1);
  borderRect.setBottom(borderRect.bottom() - 1);

  cx.painter.setPen(m_theme.borderColor);
  cx.painter.drawRect(borderRect);
}

void ImagesTab::paintThumbnailImage(const PaintContext& cx)
{
  if (cx.file->failed()) {
    return;
  }

  cx.file->loadIfNeeded(cx.geo);

  const auto imageRect       = cx.geo.imageRect(cx.thumbIndex);
  const auto scaledThumbRect = centeredRect(imageRect, cx.file->thumbnail().size());

  cx.painter.fillRect(scaledThumbRect, m_theme.backgroundColor);
  cx.painter.drawImage(scaledThumbRect, cx.file->thumbnail());
}

void ImagesTab::paintThumbnailText(const PaintContext& cx)
{
  const auto tr = cx.geo.textRect(cx.thumbIndex);

  if (cx.fileIndex == m_files.selectedIndex()) {
    cx.painter.setPen(m_theme.highlightTextColor);
  } else {
    cx.painter.setPen(m_theme.textColor);
  }

  cx.painter.setFont(m_theme.font);

  QFontMetrics fm(m_theme.font);

  const auto text = fm.elidedText(cx.file->filename(), Qt::ElideRight, tr.width());

  const auto flags = Qt::AlignHCenter | Qt::AlignVCenter | Qt::TextSingleLine;

  cx.painter.drawText(tr, flags, text);
}

void ImagesTab::scrollAreaResized(const QSize&)
{
  updateScrollbar();
}

void ImagesTab::thumbnailAreaMouseEvent(QMouseEvent* e)
{
  if (e->button() != Qt::LeftButton) {
    return;
  }

  const auto i = fileIndexAtPos(e->pos());
  if (i != BadIndex) {
    // the only way to click on a thumbnail is if it's already visible, so the
    // only thing that can happen is a click on a partially visible thumbnail,
    // which would scroll so it is fully visible, and that's just annoying
    select(i, Visibility::Ignore);
  }
}

void ImagesTab::thumbnailAreaWheelEvent(QWheelEvent* e)
{
  const auto d = (e->angleDelta() / 8).y();

  ui->imagesScrollerVBar->setValue(ui->imagesScrollerVBar->value() + (d > 0 ? -1 : 1));
}

bool ImagesTab::thumbnailAreaKeyPressEvent(QKeyEvent* e)
{
  switch (e->key()) {
  case Qt::Key_Down: {
    moveSelection(ui->imagesScrollerVBar->singleStep());
    return true;
  }

  case Qt::Key_Up: {
    moveSelection(-ui->imagesScrollerVBar->singleStep());
    return true;
  }

  case Qt::Key_PageDown: {
    moveSelection(ui->imagesScrollerVBar->pageStep());
    return true;
  }

  case Qt::Key_PageUp: {
    moveSelection(-ui->imagesScrollerVBar->pageStep());
    return true;
  }

  case Qt::Key_Home: {
    select(0);
    return true;
  }

  case Qt::Key_End: {
    if (!m_files.empty()) {
      select(m_files.size() - 1);
    }

    return true;
  }
  }

  return false;
}

void ImagesTab::onScrolled()
{
  ui->imagesThumbnails->update();
}

void ImagesTab::showTooltip(QHelpEvent* e)
{
  const auto* f = fileAtPos(e->pos());
  if (!f) {
    QToolTip::hideText();
    e->ignore();
    return;
  }

  const auto s = QString("%1 (%2)")
                     .arg(QDir::toNativeSeparators(f->path()))
                     .arg(dimensionString(f->original().size()));

  QToolTip::showText(e->globalPos(), s, ui->imagesThumbnails);
}

void ImagesTab::onExplore()
{
  if (auto* f = m_files.selectedFile()) {
    shell::Explore(f->path());
  }
}

void ImagesTab::onShowDDS()
{
  const auto b = ui->imagesShowDDS->isChecked();
  if (b != m_ddsEnabled) {
    m_ddsEnabled = b;
    update();
  }
}

void ImagesTab::onPreviewButton()
{
  core().previewFileWithAlternatives(parentWidget(), m_files.selectedFile()->path());
}

void ImagesTab::onFilterChanged()
{
  update();
}

void ImagesTab::updateScrollbar()
{
  if (m_files.size() == 0) {
    ui->imagesScrollerVBar->setRange(0, 0);
    ui->imagesScrollerVBar->setEnabled(false);
    return;
  }

  const auto geo           = makeGeometry();
  const auto availableSize = ui->imagesThumbnails->size();
  const auto fullyVisible  = geo.fullyVisibleCount();

  if (fullyVisible >= m_files.size()) {
    ui->imagesScrollerVBar->setRange(0, 0);
    ui->imagesScrollerVBar->setEnabled(false);
  } else {
    const auto d = m_files.size() - fullyVisible;
    ui->imagesScrollerVBar->setRange(0, static_cast<int>(d));
    ui->imagesScrollerVBar->setSingleStep(1);
    ui->imagesScrollerVBar->setPageStep(static_cast<int>(fullyVisible - 1));
    ui->imagesScrollerVBar->setEnabled(true);
  }
}

namespace ImagesTabHelpers
{

void Scrollbar::setTab(ImagesTab* tab)
{
  m_tab = tab;
}

void Scrollbar::wheelEvent(QWheelEvent* e)
{
  if (m_tab) {
    m_tab->thumbnailAreaWheelEvent(e);
  }
}

void ThumbnailsWidget::setTab(ImagesTab* tab)
{
  m_tab = tab;
}

void ThumbnailsWidget::paintEvent(QPaintEvent* e)
{
  if (m_tab) {
    m_tab->paintThumbnailsArea(e);
  }
}

void ThumbnailsWidget::mousePressEvent(QMouseEvent* e)
{
  if (m_tab) {
    m_tab->thumbnailAreaMouseEvent(e);
  }
}

void ThumbnailsWidget::wheelEvent(QWheelEvent* e)
{
  if (m_tab) {
    m_tab->thumbnailAreaWheelEvent(e);
  }
}

void ThumbnailsWidget::resizeEvent(QResizeEvent* e)
{
  if (m_tab) {
    m_tab->scrollAreaResized(e->size());
  }
}

void ThumbnailsWidget::keyPressEvent(QKeyEvent* e)
{
  if (m_tab) {
    if (m_tab->thumbnailAreaKeyPressEvent(e)) {
      return;
    }
  }

  QWidget::keyPressEvent(e);
}

bool ThumbnailsWidget::event(QEvent* e)
{
  if (e->type() == QEvent::ToolTip) {
    m_tab->showTooltip(static_cast<QHelpEvent*>(e));
    return true;
  }

  return QWidget::event(e);
}

ScalableImage::ScalableImage(QString path) : m_path(std::move(path)), m_border(1)
{
  auto sp = sizePolicy();
  sp.setHeightForWidth(true);
  setSizePolicy(sp);
}

void ScalableImage::setImage(const QString& path)
{
  m_path     = path;
  m_original = {};
  m_scaled   = {};

  update();
}

void ScalableImage::setImage(QImage image)
{
  m_path.clear();
  m_original = std::move(image);
  m_scaled   = {};

  update();
}

void ScalableImage::clear()
{
  setImage(QImage());
}

bool ScalableImage::hasHeightForWidth() const
{
  return true;
}

int ScalableImage::heightForWidth(int w) const
{
  return w;
}

void ScalableImage::setColors(const QColor& border, const QColor& background)
{
  m_borderColor     = border;
  m_backgroundColor = background;
}

void ScalableImage::paintEvent(QPaintEvent* e)
{
  if (m_original.isNull()) {
    if (m_path.isNull()) {
      return;
    }

    m_original.load(m_path);

    if (m_original.isNull()) {
      return;
    }
  }

  const QRect widgetRect = rect();
  const QRect imageRect = widgetRect.adjusted(m_border, m_border, -m_border, -m_border);

  const QSize scaledSize = resizeWithAspectRatio(m_original.size(), imageRect.size());

  if (m_scaled.isNull() || m_scaled.size() != scaledSize) {
    m_scaled = m_original.scaled(scaledSize.width(), scaledSize.height(),
                                 Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
  }

  const QRect drawBorderRect = widgetRect.adjusted(0, 0, -1, -1);
  const QRect drawImageRect  = centeredRect(imageRect, m_scaled.size());

  QPainter painter(this);

  // border
  painter.setPen(m_borderColor);
  painter.drawRect(drawBorderRect);

  // background
  painter.fillRect(drawImageRect, m_backgroundColor);

  // image
  painter.drawImage(drawImageRect, m_scaled);
}

Metrics::Metrics()
    : margins(3), border(1), padding(0), spacing(5), textSpacing(2), textHeight(0)
{}

Geometry::Geometry(QSize widgetSize, Metrics metrics)
    : m_widgetSize(widgetSize), m_metrics(metrics), m_topRect(calcTopRect())
{}

QRect Geometry::calcTopRect() const
{
  const auto thumbWidth = m_widgetSize.width();
  const auto& m         = m_metrics;

  const auto imageSize =
      thumbWidth - (m.margins * 2) - (m.border * 2) - (m.padding * 2);

  const auto thumbHeight = m.margins + m.border + m.padding + imageSize + m.padding +
                           m.border + m.textSpacing + m.textHeight + m.margins;

  return {0, 0, thumbWidth, thumbHeight};
}

std::size_t Geometry::fullyVisibleCount() const
{
  const auto r = thumbRect(0);

  const auto thumbWithSpacing = r.height() + m_metrics.spacing;
  const auto visible          = (m_widgetSize.height() / thumbWithSpacing);

  return static_cast<std::size_t>(visible);
}

QRect Geometry::thumbRect(std::size_t i) const
{
  // rect for the top thumbnail
  QRect r = m_topRect;

  // move down
  const auto thumbWithSpacing = m_metrics.spacing + r.height();
  r.translate(0, static_cast<int>(i * thumbWithSpacing));

  return r;
}

QRect Geometry::borderRect(std::size_t i) const
{
  auto r        = thumbRect(i);
  const auto& m = m_metrics;

  // remove margins and text
  r.adjust(m.margins, m.margins, -m.margins, -m.margins);

  // remove text
  r.adjust(0, 0, 0, -(m.textSpacing + m.textHeight));

  return r;
}

QRect Geometry::imageRect(std::size_t i) const
{
  auto r = borderRect(i);

  // remove border and padding
  const auto m = m_metrics.border + m_metrics.padding;
  r.adjust(m, m, -m, -m);

  return r;
}

QRect Geometry::textRect(std::size_t i) const
{
  const auto r = borderRect(i);

  return QRect(r.left(), r.bottom() + m_metrics.textSpacing, r.width(),
               m_metrics.textHeight);
}

QRect Geometry::selectionRect(std::size_t i) const
{
  const auto br = borderRect(i);
  const auto tr = textRect(i);

  return QRect(br.left(), br.top(), br.width(), tr.bottom() - br.top());
}

std::size_t Geometry::indexAt(const QPoint& p) const
{
  // calculate index purely based on y position
  const std::size_t offset = p.y() / (m_topRect.height() + m_metrics.spacing);

  if (!selectionRect(offset).contains(p)) {
    return BadIndex;
  }

  return offset;
}

QSize Geometry::scaledImageSize(const QSize& originalSize) const
{
  const auto availableSize = imageRect(0).size();
  return resizeWithAspectRatio(originalSize, availableSize);
}

File::File(QString path) : m_path(std::move(path)), m_failed(false) {}

void File::ensureOriginalLoaded()
{
  if (!m_original.isNull()) {
    // already loaded
    return;
  }

  QImageReader reader(m_path);

  if (!reader.read(&m_original)) {
    log::error("failed to load '{}'\n{} (error {})", m_path, reader.errorString(),
               static_cast<int>(reader.error()));

    m_failed = true;
  }
}

const QString& File::path() const
{
  return m_path;
}

const QString& File::filename() const
{
  if (m_filename.isEmpty()) {
    m_filename = QFileInfo(m_path).fileName();
  }

  return m_filename;
}

const QImage& File::original() const
{
  return m_original;
}

const QImage& File::thumbnail() const
{
  return m_thumbnail;
}

bool File::failed() const
{
  return m_failed;
}

void File::loadIfNeeded(const Geometry& geo)
{
  if (needsLoad(geo)) {
    load(geo);
  }
}

bool File::needsLoad(const Geometry& geo) const
{
  if (m_failed) {
    return false;
  }

  if (m_original.isNull() || m_thumbnail.isNull()) {
    return true;
  }

  const auto scaledSize = geo.scaledImageSize(m_original.size());
  return (m_thumbnail.size() != scaledSize);
}

void File::load(const Geometry& geo)
{
  m_failed = false;
  ensureOriginalLoaded();

  if (m_failed) {
    QImage warning(":/MO/gui/warning");
    const auto scaledSize = geo.scaledImageSize(warning.size());

    m_thumbnail =
        warning.scaled(scaledSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
  } else {
    const auto scaledSize = geo.scaledImageSize(m_original.size());

    m_thumbnail =
        m_original.scaled(scaledSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
  }
}

Files::Files() : m_selection(BadIndex), m_filtered(false) {}

void Files::clear()
{
  m_allFiles.clear();
  m_filteredFiles.clear();
  m_selection = BadIndex;
  m_filtered  = false;
}

void Files::add(File f)
{
  m_allFiles.emplace_back(std::move(f));
}

void Files::addFiltered(File* f)
{
  m_filteredFiles.push_back(f);
}

bool Files::empty() const
{
  if (m_filtered) {
    return m_filteredFiles.empty();
  } else {
    return m_allFiles.empty();
  }
}

std::size_t Files::size() const
{
  if (m_filtered) {
    return m_filteredFiles.size();
  } else {
    return m_allFiles.size();
  }
}

void Files::switchToAll()
{
  m_filtered = false;
  m_filteredFiles.clear();
}

void Files::switchToFiltered()
{
  m_filtered = true;
  m_filteredFiles.clear();
}

const File* Files::get(std::size_t i) const
{
  if (m_filtered) {
    if (i < m_filteredFiles.size()) {
      return m_filteredFiles[i];
    }
  } else {
    if (i < m_allFiles.size()) {
      return &m_allFiles[i];
    }
  }

  return nullptr;
}

File* Files::get(std::size_t i)
{
  return const_cast<File*>(std::as_const(*this).get(i));
}

std::size_t Files::indexOf(const File* f) const
{
  if (m_filtered) {
    for (std::size_t i = 0; i < m_filteredFiles.size(); ++i) {
      if (m_filteredFiles[i] == f) {
        return i;
      }
    }
  } else {
    for (std::size_t i = 0; i < m_allFiles.size(); ++i) {
      if (&m_allFiles[i] == f) {
        return i;
      }
    }
  }

  return BadIndex;
}

const File* Files::selectedFile() const
{
  return get(m_selection);
}

File* Files::selectedFile()
{
  return get(m_selection);
}

std::size_t Files::selectedIndex() const
{
  return m_selection;
}

void Files::select(std::size_t i)
{
  m_selection = i;
}

std::vector<File>& Files::allFiles()
{
  return m_allFiles;
}

bool Files::isFiltered() const
{
  return m_filtered;
}

PaintContext::PaintContext(QWidget* w, Geometry geo)
    : painter(w), geo(geo), file(nullptr), thumbIndex(0), fileIndex(0)
{}

}  // namespace ImagesTabHelpers

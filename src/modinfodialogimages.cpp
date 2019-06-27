#include "modinfodialogimages.h"
#include "ui_modinfodialog.h"
#include "settings.h"
#include "utility.h"

QSize resizeWithAspectRatio(const QSize& original, const QSize& available)
{
  const auto ratio = std::min({
    1.0,
    static_cast<double>(available.width()) / original.width(),
    static_cast<double>(available.height()) / original.height()});

  const QSize scaledSize(
    static_cast<int>(std::round(original.width() * ratio)),
    static_cast<int>(std::round(original.height() * ratio)));

  return scaledSize;
}

QRect centeredRect(const QRect& rect, const QSize& size)
{
  return QRect(
    (rect.left()+rect.width()/2) - size.width()/2,
    (rect.top()+rect.height()/2) - size.height()/2,
    size.width(),
    size.height());
}


ImagesTab::ImagesTab(
  OrganizerCore& oc, PluginContainer& plugin,
  QWidget* parent, Ui::ModInfoDialog* ui, int id) :
    ModInfoDialogTab(oc, plugin, parent, ui, id),
    m_image(new ScalableImage),
    m_margins(3), m_border(1), m_padding(0), m_spacing(5),
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

  ui->imagesThumbnails->setTab(this);

  ui->imagesScrollerVBar->setTab(this);
  connect(ui->imagesScrollerVBar, &QScrollBar::valueChanged, [&]{ onScrolled(); });

  ui->imagesShowDDS->setEnabled(m_ddsAvailable);

  m_filter.setEdit(ui->imagesFilter);
  connect(&m_filter, &FilterWidget::changed, [&]{ onFilterChanged(); });

  connect(ui->imagesExplore, &QAbstractButton::clicked, [&]{ onExplore(); });
  connect(ui->imagesShowDDS, &QCheckBox::toggled, [&]{ onShowDDS(); });

  ui->imagesShowDDS->setEnabled(m_ddsAvailable);

  ui->imagesThumbnails->setAutoFillBackground(false);
  ui->imagesThumbnails->setAttribute(Qt::WA_OpaquePaintEvent, true);

  {
    auto list = std::make_unique<QListWidget>();
    parentWidget()->style()->polish(list.get());

    m_colors.border = QColor(Qt::black);
    m_colors.background = QColor(Qt::black);
    m_colors.selection = list->palette().color(QPalette::Highlight);

    m_image->setColors(m_colors.border, m_colors.background);
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
  ui->imagesThumbnails->update();

  setHasData(m_files.size() > 0);
}

void ImagesTab::saveState(Settings& s)
{
  s.directInterface().setValue(
    "mod_info_dialog_images_show_dds", m_ddsEnabled);
}

void ImagesTab::restoreState(const Settings& s)
{
  ui->imagesShowDDS->setChecked(s.directInterface()
    .value("mod_info_dialog_images_show_dds", false).toBool());
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
    m_files.select(m_files.indexOf(oldSelection));
  } else {
    m_files.select(BadIndex);
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

  for (auto& f : m_files.allFiles()) {
    if (hasTextFilter) {
      // check filter widget
      const auto m = m_filter.matches([&](auto&& what) {
        return f.path.contains(what, Qt::CaseInsensitive);
        });

      if (!m) {
        // no match, skip
        continue;
      }
    }

    if (!m_ddsEnabled) {
      // skip .dds files
      if (f.path.endsWith(".dds", Qt::CaseInsensitive)) {
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
    if (s[0] != ".") {
      s = "." + s;
    }

    m_supportedFormats.emplace_back(std::move(s));
  }
}

void ImagesTab::select(std::size_t i)
{
  m_files.select(i);

  if (auto* f=m_files.selectedFile()) {
    // when jumping elsewhere in the list, such as with page down/up, the file
    // might not be visible yet, which means it hasn't been loaded and would
    // pass a null image in setImage() below
    f->ensureOriginalLoaded();

    ui->imagesPath->setText(QDir::toNativeSeparators(f->path));
    ui->imagesExplore->setEnabled(true);
    m_image->setImage(f->original);
    ensureVisible(i);
  } else {
    ui->imagesPath->clear();
    ui->imagesExplore->setEnabled(false);
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

void ImagesTab::ensureVisible(std::size_t i)
{
  const auto geo = makeGeometry();

  const auto fullyVisible = geo.fullyVisibleCount();
  const auto first = ui->imagesScrollerVBar->value();

  if (i < first) {
    // go up
    ui->imagesScrollerVBar->setValue(static_cast<int>(i));
  } else if (i >= first + fullyVisible) {
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
  if (offset == ImagesGeometry::BadIndex) {
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

const ImagesTab::File* ImagesTab::fileAtPos(const QPoint& p) const
{
  const auto i = fileIndexAtPos(p);
  if (i >= m_files.size()) {
    return nullptr;
  }

  return m_files.get(i);
}

ImagesGeometry ImagesTab::makeGeometry() const
{
  return ImagesGeometry(
    ui->imagesThumbnails->size(),
    m_margins, m_border, m_padding, m_spacing);
}

void ImagesTab::paintThumbnailsArea(QPaintEvent* e)
{
  PaintContext cx(ui->imagesThumbnails, makeGeometry());

  cx.painter.fillRect(
    ui->imagesThumbnails->rect(),
    ui->imagesThumbnails->palette().color(QPalette::Window));

  const auto visible = cx.geo.fullyVisibleCount() + 1;
  const auto first = ui->imagesScrollerVBar->value();

  for (std::size_t i=0; i<visible; ++i) {
    const auto fileIndex = first + i;
    auto* file = m_files.get(fileIndex);
    if (!file) {
      break;
    }

    cx.file = file;
    cx.thumbIndex = i;
    cx.fileIndex = fileIndex;

    paintThumbnail(cx);
  }
}

void ImagesTab::paintThumbnail(const PaintContext& cx)
{
  paintThumbnailBackground(cx);
  paintThumbnailBorder(cx);
  paintThumbnailImage(cx);
}

void ImagesTab::paintThumbnailBackground(const PaintContext& cx)
{
  if (m_files.selectedIndex() == cx.fileIndex) {
    const auto rect = cx.geo.thumbRect(cx.thumbIndex);
    cx.painter.fillRect(rect, m_colors.selection);
  }
}

void ImagesTab::paintThumbnailBorder(const PaintContext& cx)
{
  auto borderRect = cx.geo.borderRect(cx.thumbIndex);

  // rects don't include the bottom right corner, but drawRect() does, so
  // resize it
  borderRect.setRight(borderRect.right() - 1);
  borderRect.setBottom(borderRect.bottom() - 1);

  cx.painter.setPen(m_colors.border);
  cx.painter.drawRect(borderRect);
}

void ImagesTab::paintThumbnailImage(const PaintContext& cx)
{
  if (cx.file->failed) {
    return;
  }

  if (needsReload(cx.geo, *cx.file)) {
    reload(cx.geo, *cx.file);
  }

  if (cx.file->thumbnail.isNull()) {
    return;
  }

  const auto imageRect = cx.geo.imageRect(cx.thumbIndex);
  const auto scaledThumbRect = centeredRect(
    imageRect, cx.file->thumbnail.size());

  cx.painter.fillRect(scaledThumbRect, m_colors.background);
  cx.painter.drawImage(scaledThumbRect, cx.file->thumbnail);
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
    select(i);
  }
}

void ImagesTab::thumbnailAreaWheelEvent(QWheelEvent* e)
{
  const auto d = (e->angleDelta() / 8).y();

  ui->imagesScrollerVBar->setValue(
    ui->imagesScrollerVBar->value() + (d > 0 ? -1 : 1));
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

  QToolTip::showText(
    e->globalPos(), QDir::toNativeSeparators(f->path),
    ui->imagesThumbnails);
}

void ImagesTab::onExplore()
{
  if (auto* f=m_files.selectedFile()) {
    MOBase::shell::ExploreFile(f->path);
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

void ImagesTab::onFilterChanged()
{
  update();
}

bool ImagesTab::needsReload(const ImagesGeometry& geo, const File& file) const
{
  if (file.failed) {
    return false;
  }

  if (file.original.isNull() || file.thumbnail.isNull()) {
    return true;
  }

  const auto scaledSize = geo.scaledImageSize(file.original.size());
  return (file.thumbnail.size() != scaledSize);
}

void ImagesTab::reload(const ImagesGeometry& geo, File& file)
{
  file.failed = false;
  file.ensureOriginalLoaded();

  if (file.failed) {
    return;
  }

  file.thumbnail = file.original.scaled(
    geo.scaledImageSize(file.original.size()),
    Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

void ImagesTab::updateScrollbar()
{
  if (m_files.size() == 0) {
    ui->imagesScrollerVBar->setRange(0, 0);
    ui->imagesScrollerVBar->setEnabled(false);
    return;
  }

  const auto geo = makeGeometry();
  const auto availableSize = ui->imagesThumbnails->size();
  const auto fullyVisible = geo.fullyVisibleCount();

  if (fullyVisible >= m_files.size()) {
    ui->imagesScrollerVBar->setRange(0, 0);
    ui->imagesScrollerVBar->setEnabled(false);
  } else {
    const auto d = m_files.size() - fullyVisible;
    ui->imagesScrollerVBar->setRange(0, static_cast<int>(d));
    ui->imagesScrollerVBar->setSingleStep(1);
    ui->imagesScrollerVBar->setPageStep(static_cast<int>(fullyVisible));
    ui->imagesScrollerVBar->setEnabled(true);
  }
}


void ImagesThumbnails::setTab(ImagesTab* tab)
{
  m_tab = tab;
}

void ImagesThumbnails::paintEvent(QPaintEvent* e)
{
  if (m_tab) {
    m_tab->paintThumbnailsArea(e);
  }
}

void ImagesThumbnails::mousePressEvent(QMouseEvent* e)
{
  if (m_tab) {
    m_tab->thumbnailAreaMouseEvent(e);
  }
}

void ImagesThumbnails::wheelEvent(QWheelEvent* e)
{
  if (m_tab) {
    m_tab->thumbnailAreaWheelEvent(e);
  }
}

void ImagesThumbnails::resizeEvent(QResizeEvent* e)
{
  if (m_tab) {
    m_tab->scrollAreaResized(e->size());
  }
}

void ImagesThumbnails::keyPressEvent(QKeyEvent* e)
{
  if (m_tab) {
    if (m_tab->thumbnailAreaKeyPressEvent(e)) {
      return;
    }
  }

  QWidget::keyPressEvent(e);
}

bool ImagesThumbnails::event(QEvent* e)
{
  if (e->type() == QEvent::ToolTip) {
    m_tab->showTooltip(static_cast<QHelpEvent*>(e));
    return true;
  }

  return QWidget::event(e);
}


void ImagesScrollbar::setTab(ImagesTab* tab)
{
  m_tab = tab;
}

void ImagesScrollbar::wheelEvent(QWheelEvent* e)
{
  if (m_tab) {
    m_tab->thumbnailAreaWheelEvent(e);
  }
}


ScalableImage::ScalableImage(QString path)
  : m_path(std::move(path)), m_border(1)
{
  auto sp = sizePolicy();
  sp.setHeightForWidth(true);
  setSizePolicy(sp);
}

void ScalableImage::setImage(const QString& path)
{
  m_path = path;
  m_original = {};
  m_scaled = {};

  update();
}

void ScalableImage::setImage(QImage image)
{
  m_path.clear();
  m_original = std::move(image);
  m_scaled = {};

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
  m_borderColor = border;
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
  const QRect imageRect = widgetRect.adjusted(
    m_border, m_border, -m_border, -m_border);

  const QSize scaledSize = resizeWithAspectRatio(
    m_original.size(), imageRect.size());

  if (m_scaled.isNull() || m_scaled.size() != scaledSize) {
    m_scaled = m_original.scaled(
      scaledSize.width(), scaledSize.height(),
      Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
  }

  const QRect drawBorderRect = widgetRect.adjusted(0, 0, -1, -1);
  const QRect drawImageRect = centeredRect(imageRect, m_scaled.size());

  QPainter painter(this);

  // border
  painter.setPen(m_borderColor);
  painter.drawRect(drawBorderRect);

  // background
  painter.fillRect(drawImageRect, m_backgroundColor);

  // image
  painter.drawImage(drawImageRect, m_scaled);
}


ImagesGeometry::ImagesGeometry(
  const QSize& widgetSize, int margins, int border, int padding, int spacing) :
    m_widgetSize(widgetSize),
    m_margins(margins), m_padding(padding), m_border(border),
    m_spacing(spacing), m_topRect(calcTopRect())
{
}

QRect ImagesGeometry::calcTopRect() const
{
  const auto thumbWidth = m_widgetSize.width();
  const auto imageSize = thumbWidth - (m_margins * 2) - (m_border * 2) - (m_padding * 2);
  const auto thumbHeight =
    m_margins + m_border + m_padding +
    imageSize +
    m_border + m_padding + m_margins;

  return {0, 0, thumbWidth, thumbHeight};
}

std::size_t ImagesGeometry::fullyVisibleCount() const
{
  const auto r = thumbRect(0);
  const auto visible = (m_widgetSize.height() / (r.height() + m_spacing));
  return static_cast<std::size_t>(visible);
}

QRect ImagesGeometry::thumbRect(std::size_t i) const
{
  // rect for the top thumbnail
  QRect r = m_topRect;

  // move down
  const auto thumbWithSpacing = m_spacing + r.height();
  r.translate(0, static_cast<int>(i * thumbWithSpacing));

  return r;
}

QRect ImagesGeometry::borderRect(std::size_t i) const
{
  auto r = thumbRect(i);

  // remove margins
  const auto m = m_margins;
  r.adjust(m, m, -m, -m);

  return r;
}

QRect ImagesGeometry::imageRect(std::size_t i) const
{
  auto r = borderRect(i);

  // remove border and padding
  const auto m = m_border + m_padding;
  r.adjust(m, m, -m, -m);

  return r;
}

std::size_t ImagesGeometry::indexAt(const QPoint& p) const
{
  // calculate index purely based on y position
  const std::size_t offset = p.y() / (m_topRect.height() + m_spacing);

  if (!borderRect(offset).contains(p)) {
    return BadIndex;
  }

  return offset;
}

QSize ImagesGeometry::scaledImageSize(const QSize& originalSize) const
{
  const auto availableSize = imageRect(0).size();
  return resizeWithAspectRatio(originalSize, availableSize);
}

void ImagesGeometry::dump() const
{
  qDebug()
    << "ImagesTab geometry:\n"
    << "  . widget size: " << m_widgetSize << "\n"
    << "  . margins: " << m_margins << "\n"
    << "  . border: " << m_border << "\n"
    << "  . padding: " << m_padding << "\n"
    << "  . spacing: " << m_spacing << "\n"
    << "  . top rect: " << m_topRect;
}


ImagesTab::Files::Files()
  : m_selection(BadIndex), m_filtered(false)
{
}

void ImagesTab::Files::clear()
{
  m_allFiles.clear();
  m_filteredFiles.clear();
  m_selection = BadIndex;
  m_filtered = false;
}

void ImagesTab::Files::add(File f)
{
  m_allFiles.emplace_back(std::move(f));
}

void ImagesTab::Files::addFiltered(File* f)
{
  m_filteredFiles.push_back(f);
}

bool ImagesTab::Files::empty() const
{
  if (m_filtered) {
    return m_filteredFiles.empty();
  } else {
    return m_allFiles.empty();
  }
}

std::size_t ImagesTab::Files::size() const
{
  if (m_filtered) {
    return m_filteredFiles.size();
  } else {
    return m_allFiles.size();
  }
}

void ImagesTab::Files::switchToAll()
{
  m_filtered = false;
  m_filteredFiles.clear();
}

void ImagesTab::Files::switchToFiltered()
{
  m_filtered = true;
  m_filteredFiles.clear();
}

const ImagesTab::File* ImagesTab::Files::get(std::size_t i) const
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

ImagesTab::File* ImagesTab::Files::get(std::size_t i)
{
  return const_cast<File*>(std::as_const(*this).get(i));
}

std::size_t ImagesTab::Files::indexOf(const File* f) const
{
  if (m_filtered) {
    for (std::size_t i=0; i<m_filteredFiles.size(); ++i) {
      if (m_filteredFiles[i] == f) {
        return i;
      }
    }
  } else {
    for (std::size_t i=0; i<m_allFiles.size(); ++i) {
      if (&m_allFiles[i] == f) {
        return i;
      }
    }
  }

  return BadIndex;
}

const ImagesTab::File* ImagesTab::Files::selectedFile() const
{
  return get(m_selection);
}

ImagesTab::File* ImagesTab::Files::selectedFile()
{
  return get(m_selection);
}

std::size_t ImagesTab::Files::selectedIndex() const
{
  return m_selection;
}

void ImagesTab::Files::select(std::size_t i)
{
  m_selection = i;
}

std::vector<ImagesTab::File>& ImagesTab::Files::allFiles()
{
  return m_allFiles;
}

bool ImagesTab::Files::isFiltered() const
{
  return m_filtered;
}

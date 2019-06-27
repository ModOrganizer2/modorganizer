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
    m_selection(nullptr), m_ddsAvailable(false), m_ddsEnabled(false)
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
  ui->imagesScrollerVBar->setSingleStep(1);
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
  m_filteredFiles.clear();
  ui->imagesScrollerVBar->setValue(0);

  select(nullptr);
  setHasData(false);
}

bool ImagesTab::feedFile(const QString& rootPath, const QString& fullPath)
{
  for (const auto& ext : m_supportedFormats) {
    if (fullPath.endsWith(ext, Qt::CaseInsensitive)) {
      m_files.push_back({fullPath});
      return true;
    }
  }

  return false;
}

void ImagesTab::update()
{
  filterImages();
  updateScrollbar();
  ui->imagesThumbnails->update();

  setHasData(fileCount() > 0);
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

void ImagesTab::filterImages()
{
  if (!needsFiltering()) {
    return;
  }

  m_filteredFiles.clear();

  bool hasTextFilter = !m_filter.empty();
  bool sawSelection = false;

  for (auto& f : m_files) {
    if (hasTextFilter) {
      const auto m = m_filter.matches([&](auto&& what) {
        return f.path.contains(what, Qt::CaseInsensitive);
      });

      if (!m) {
        continue;
      }
    }

    if (!m_ddsEnabled) {
      if (f.path.endsWith(".dds", Qt::CaseInsensitive)) {
        continue;
      }
    }

    if (&f == m_selection) {
      sawSelection = true;
    }

    m_filteredFiles.push_back(&f);
  }

  if (!sawSelection) {
    select(nullptr);
  }
}

std::size_t ImagesTab::fileCount() const
{
  if (needsFiltering()) {
    return m_filteredFiles.size();
  } else {
    return m_files.size();
  }
}

const ImagesTab::File* ImagesTab::getFile(std::size_t i) const
{
  if (needsFiltering()) {
    if (i >= m_filteredFiles.size()) {
      return nullptr;
    }

    return m_filteredFiles[i];
  } else {
    if (i >= m_files.size()) {
      return nullptr;
    }

    return &m_files[i];
  }
}

ImagesTab::File* ImagesTab::getFile(std::size_t i)
{
  return const_cast<File*>(std::as_const(*this).getFile(i));
}

bool ImagesTab::needsFiltering() const
{
  if (!m_filter.empty()) {
    return true;
  }

  if (!m_ddsEnabled) {
    return true;
  }

  return false;
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

void ImagesTab::select(const File* f)
{
  if (f) {
    ui->imagesPath->setText(QDir::toNativeSeparators(f->path));
    ui->imagesExplore->setEnabled(true);
    m_image->setImage(f->original);
  } else {
    ui->imagesPath->clear();
    ui->imagesExplore->setEnabled(false);
    m_image->clear();
  }

  m_selection = f;
  ui->imagesThumbnails->update();
}

ImagesGeometry ImagesTab::makeGeometry() const
{
  return ImagesGeometry(
    ui->imagesThumbnails->size(),
    m_margins, m_border, m_padding, m_spacing);
}

void ImagesTab::paintThumbnailsArea(QPaintEvent* e)
{
  const auto geo = makeGeometry();

  QPainter painter(ui->imagesThumbnails);

  painter.fillRect(
    ui->imagesThumbnails->rect(),
    ui->imagesThumbnails->palette().color(QPalette::Window));

  const auto visible = geo.fullyVisibleCount() + 1;
  const auto first = ui->imagesScrollerVBar->value();

  for (std::size_t i=0; i<visible; ++i) {
    const auto fileIndex = first + i;
    auto* file = getFile(fileIndex);
    if (!file) {
      break;
    }

    paintThumbnail(painter, geo, *file, i);
  }
}

void ImagesTab::paintThumbnail(
  QPainter& painter, const ImagesGeometry& geo,
  File& file, std::size_t i)
{
  paintThumbnailBackground(painter, geo, file, i);
  paintThumbnailBorder(painter, geo, i);
  paintThumbnailImage(painter, geo, file, i);
}

void ImagesTab::paintThumbnailBackground(
  QPainter& painter, const ImagesGeometry& geo,
  File& file, std::size_t i)
{
  if (&file == m_selection) {
    const auto rect = geo.thumbRect(i);
    painter.fillRect(rect, m_colors.selection);
  }
}

void ImagesTab::paintThumbnailBorder(
  QPainter& painter, const ImagesGeometry& geo, std::size_t i)
{
  auto borderRect = geo.borderRect(i);

  // rects don't include the bottom right corner, but drawRect() does, so
  // resize it
  borderRect.setRight(borderRect.right() - 1);
  borderRect.setBottom(borderRect.bottom() - 1);

  painter.setPen(m_colors.border);
  painter.drawRect(borderRect);
}

void ImagesTab::paintThumbnailImage(
  QPainter& painter, const ImagesGeometry& geo,
  File& file, std::size_t i)
{
  if (file.failed) {
    return;
  }

  if (needsReload(geo, file)) {
    reload(geo, file);
  }

  if (file.thumbnail.isNull()) {
    return;
  }

  const auto imageRect = geo.imageRect(i);
  const auto scaledThumbRect = centeredRect(imageRect, file.thumbnail.size());

  painter.fillRect(scaledThumbRect, m_colors.background);
  painter.drawImage(scaledThumbRect, file.thumbnail);
}

const ImagesTab::File* ImagesTab::fileAtPos(const QPoint& p) const
{
  const auto geo = makeGeometry();

  // this is the index relative to the top
  const auto offset = geo.indexAt(p);
  if (offset == ImagesGeometry::BadIndex) {
    return nullptr;
  }

  const auto first = ui->imagesScrollerVBar->value();
  if (first < 0) {
    return nullptr;
  }

  const auto i = static_cast<std::size_t>(first) + offset;
  if (i >= fileCount()) {
    return nullptr;
  }

  return getFile(i);
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

  select(fileAtPos(e->pos()));
}

void ImagesTab::thumbnailAreaWheelEvent(QWheelEvent* e)
{
  const auto d = (e->angleDelta() / 8).y();

  ui->imagesScrollerVBar->setValue(
    ui->imagesScrollerVBar->value() + (d > 0 ? -1 : 1));
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
  if (!m_selection) {
    return;
  }

  MOBase::shell::ExploreFile(m_selection->path);
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

  if (file.original.isNull()) {
    if (!file.original.load(file.path)) {
      qCritical() << "failed to load image from " << file.path;
      file.failed = true;
      return;
    }
  }

  file.thumbnail = file.original.scaled(
    geo.scaledImageSize(file.original.size()),
    Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

void ImagesTab::updateScrollbar()
{
  if (fileCount() == 0) {
    ui->imagesScrollerVBar->setRange(0, 0);
    ui->imagesScrollerVBar->setEnabled(false);
    return;
  }

  const auto geo = makeGeometry();
  const auto availableSize = ui->imagesThumbnails->size();
  const auto fullyVisible = geo.fullyVisibleCount();

  if (fullyVisible >= fileCount()) {
    ui->imagesScrollerVBar->setRange(0, 0);
    ui->imagesScrollerVBar->setEnabled(false);
  } else {
    const auto d = fileCount() - fullyVisible;
    ui->imagesScrollerVBar->setRange(0, static_cast<int>(d));
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

  // background
  painter.fillRect(drawBorderRect, m_backgroundColor);

  // border
  painter.setPen(m_borderColor);
  painter.drawRect(drawBorderRect);

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

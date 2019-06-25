#include "modinfodialogimages.h"
#include "ui_modinfodialog.h"
#include "utility.h"

ImagesTab::ImagesTab(
  OrganizerCore& oc, PluginContainer& plugin,
  QWidget* parent, Ui::ModInfoDialog* ui, int id) :
    ModInfoDialogTab(oc, plugin, parent, ui, id),
    m_image(new ScalableImage), m_margins(3), m_padding(5), m_border(1),
    m_selection(nullptr)
{
  auto* ly = new QVBoxLayout(ui->imagesImage);
  ly->setContentsMargins({0, 0, 0, 0});
  ly->addWidget(m_image);

  delete ui->imagesThumbnails->layout();

  ui->tabImagesSplitter->setSizes({128, 1});
  ui->tabImagesSplitter->setStretchFactor(0, 0);
  ui->tabImagesSplitter->setStretchFactor(1, 1);

  ui->imagesScrollArea->setWidgetResizable(false);
  ui->imagesScrollArea->setTab(this);
  ui->imagesThumbnails->setTab(this);

  connect(ui->imagesExplore, &QAbstractButton::clicked, [&]{ onExplore(); });

  getSupportedFormats();
}

void ImagesTab::clear()
{
  m_files.clear();
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
  setHasData(!m_files.empty());
  resizeWidget();
  ui->imagesThumbnails->update();
}

void ImagesTab::getSupportedFormats()
{
  for (const auto& entry : QImageReader::supportedImageFormats()) {
    QString s(entry);
    if (s.isNull() || s.isEmpty()) {
      continue;
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
    ui->imagesPath->setText(f->path);
    ui->imagesExplore->setEnabled(true);
    m_image->setImage(f->original);
  } else {
    ui->imagesPath->clear();
    ui->imagesExplore->setEnabled(false);
    m_image->clear();
  }

  m_selection = f;
}

int ImagesTab::calcThumbSize(int availableWidth) const
{
  return availableWidth - (m_margins * 2);
}

int ImagesTab::calcWidgetHeight(int availableWidth) const
{
  if (m_files.empty()) {
    return 0;
  }

  const auto thumbSize = calcThumbSize(availableWidth);

  int h = 0;

  // first thumb
  h = thumbSize;

  // subsequent thumbs with padding before each
  const auto thumbWithPadding = m_padding + thumbSize;
  h += static_cast<int>(thumbWithPadding * (m_files.size() - 1));

  // margin top and bottom
  h += m_margins * 2;

  return h;
}

QRect ImagesTab::calcTopThumbRect(int thumbSize) const
{
  return {m_margins, m_margins, thumbSize, thumbSize};
}

std::pair<std::size_t, std::size_t> ImagesTab::calcVisibleRange(
  int top, int bottom, int thumbSize) const
{
  const std::size_t begin = top / (thumbSize + m_padding);
  const std::size_t end = bottom / (thumbSize + m_padding) + 1;

  return {begin, end};
}

QRect ImagesTab::calcBorderRect(
  const QRect& topRect, int thumbSize, std::size_t i) const
{
  return {
    topRect.left(),
    static_cast<int>(topRect.top() + (i * (thumbSize + m_padding))),
    thumbSize,
    thumbSize
  };
}

QRect ImagesTab::calcImageRect(
  const QRect& topRect, int thumbSize, std::size_t i) const
{
  return calcBorderRect(topRect, thumbSize, i).adjusted(
    m_border, m_border, -m_border, -m_border);
}

QSize ImagesTab::calcScaledImageSize(
  const QSize& originalSize, const QSize& imageSize) const
{
  const auto ratio = std::min({
    1.0,
    static_cast<double>(imageSize.width()) / originalSize.width(),
    static_cast<double>(imageSize.height()) / originalSize.height()});

  const QSize scaledSize(
    static_cast<int>(std::round(originalSize.width() * ratio)),
    static_cast<int>(std::round(originalSize.height() * ratio)));

  return scaledSize;
}

void ImagesTab::paintThumbnails(QPaintEvent* e)
{
  PaintContext cx(ui->imagesThumbnails);
  cx.thumbSize = calcThumbSize(ui->imagesThumbnails->width());
  cx.topRect = calcTopThumbRect(cx.thumbSize);

  const auto [begin, end] = calcVisibleRange(
    e->rect().top(), e->rect().bottom(), cx.thumbSize);

  for (std::size_t i=begin; i<end && i<m_files.size(); ++i) {
    paintThumbnail(cx, i);
  }
}

void ImagesTab::paintThumbnail(PaintContext& cx, std::size_t i)
{
  paintThumbnailBorder(cx, i);
  paintThumbnailImage(cx, i);
}

void ImagesTab::paintThumbnailBorder(PaintContext& cx, std::size_t i)
{
  const auto borderRect = calcBorderRect(cx.topRect, cx.thumbSize, i);

  cx.painter.setPen(QColor(Qt::black));
  cx.painter.drawRect(borderRect);
}

void ImagesTab::paintThumbnailImage(PaintContext& cx, std::size_t i)
{
  auto& file = m_files[i];
  if (file.failed) {
    return;
  }

  const auto imageRect = calcImageRect(cx.topRect, cx.thumbSize, i);

  if (needsReload(file, imageRect.size())) {
    reload(file, imageRect.size());
  }

  if (file.thumbnail.isNull()) {
    return;
  }

  // center scaled image in rect
  const QRect scaledThumbRect(
    (imageRect.left()+imageRect.width()/2) - file.thumbnail.width()/2,
    (imageRect.top()+imageRect.height()/2) - file.thumbnail.height()/2,
    file.thumbnail.width(),
    file.thumbnail.height());

  cx.painter.drawImage(scaledThumbRect, file.thumbnail);
}

const ImagesTab::File* ImagesTab::fileAtPos(const QPoint& p) const
{
  const auto thumbSize = calcThumbSize(ui->imagesThumbnails->width());

  // calculate index purely based on y position
  const std::size_t i = p.y() / (thumbSize + m_padding);
  if (i >= m_files.size()) {
    return nullptr;
  }

  // get actual rect
  const auto topRect = calcTopThumbRect(thumbSize);
  const auto rect = calcBorderRect(topRect, thumbSize, i);

  if (!rect.contains(p)) {
    return nullptr;
  }

  return &m_files[i];
}

void ImagesTab::scrollAreaResized(const QSize&)
{
  resizeWidget();
}

void ImagesTab::thumbnailsMouseEvent(QMouseEvent* e)
{
  if (e->button() != Qt::LeftButton) {
    return;
  }

  if (const auto* file=fileAtPos(e->pos())) {
    select(file);
  }
}

void ImagesTab::onExplore()
{
  if (!m_selection) {
    return;
  }

  MOBase::shell::ExploreFile(m_selection->path);
}

bool ImagesTab::needsReload(const File& file, const QSize& imageSize) const
{
  if (file.failed) {
    return false;
  }

  if (file.original.isNull() || file.thumbnail.isNull()) {
    return true;
  }

  const auto scaledSize = calcScaledImageSize(file.original.size(), imageSize);
  if (file.thumbnail.size() != scaledSize) {
    return true;
  }

  return false;
}

void ImagesTab::reload(File& file, const QSize& scaledSize)
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
    calcScaledImageSize(file.original.size(), scaledSize),
    Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

void ImagesTab::resizeWidget()
{
  if (m_files.empty()) {
    ui->imagesThumbnails->setGeometry(QRect());
    return;
  }

  const auto availableWidth = ui->imagesScrollArea->viewport()->width();

  const int widgetHeight = calcWidgetHeight(availableWidth);
  ui->imagesThumbnails->setGeometry(QRect(0, 0, availableWidth, widgetHeight));
}


void ImagesScrollArea::setTab(ImagesTab* tab)
{
  m_tab = tab;
}

void ImagesScrollArea::resizeEvent(QResizeEvent* e)
{
  if (m_tab) {
    m_tab->scrollAreaResized(e->size());
  }
}


void ImagesThumbnails::setTab(ImagesTab* tab)
{
  m_tab = tab;
}

void ImagesThumbnails::paintEvent(QPaintEvent* e)
{
  if (m_tab) {
    m_tab->paintThumbnails(e);
  }
}

void ImagesThumbnails::mousePressEvent(QMouseEvent* e)
{
  if (m_tab) {
    m_tab->thumbnailsMouseEvent(e);
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

  const auto ratio = std::min({
    1.0,
    static_cast<double>(imageRect.width()) / m_original.width(),
    static_cast<double>(imageRect.height()) / m_original.height()});

  const QSize scaledSize(
    static_cast<int>(std::round(m_original.width() * ratio)),
    static_cast<int>(std::round(m_original.height() * ratio)));

  if (m_scaled.isNull() || m_scaled.size() != scaledSize) {
    m_scaled = m_original.scaled(
      scaledSize.width(), scaledSize.height(),
      Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
  }

  const QRect drawBorderRect = widgetRect.adjusted(0, 0, -1, -1);

  const QRect drawImageRect(
    (imageRect.left()+imageRect.width()/2) - m_scaled.width()/2,
    (imageRect.top()+imageRect.height()/2) - m_scaled.height()/2,
    m_scaled.width(), m_scaled.height());


  QPainter painter(this);

  painter.setPen(QColor(Qt::black));
  painter.drawRect(drawBorderRect);
  painter.drawImage(drawImageRect, m_scaled);
}

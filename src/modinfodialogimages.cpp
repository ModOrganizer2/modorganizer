#include "modinfodialogimages.h"
#include "ui_modinfodialog.h"

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


ImagesTab::ImagesTab(
  OrganizerCore& oc, PluginContainer& plugin,
  QWidget* parent, Ui::ModInfoDialog* ui, int id) :
    ModInfoDialogTab(oc, plugin, parent, ui, id),
    m_image(new ScalableImage), m_margins(3), m_padding(5), m_border(1)
{
  ui->imagesImage->layout()->addWidget(m_image);
  delete ui->imagesThumbnails->layout();

  ui->tabImagesSplitter->setSizes({128, 1});
  ui->tabImagesSplitter->setStretchFactor(0, 0);
  ui->tabImagesSplitter->setStretchFactor(1, 1);

  ui->imagesScrollArea->setWidgetResizable(false);
  ui->imagesScrollArea->setTab(this);
  ui->imagesThumbnails->setTab(this);

  const auto list = QImageReader::supportedImageFormats();
  for (const auto& entry : list) {
    QString s(entry);
    if (!s.isEmpty()) {
      if (s[0] != ".") {
        s = "." + s;
      }

      m_supportedFormats.emplace_back(std::move(s));
    }
  }
}

void ImagesTab::clear()
{
  m_image->clear();
  m_files.clear();
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
}

void ImagesTab::scrollAreaResized(const QSize& s)
{
  const int availableWidth = s.width();

  const int thumbSize = availableWidth - (m_margins * 2);

  int height = 0;
  if (!m_files.empty()) {
    height = thumbSize + static_cast<int>((m_padding + thumbSize) * (m_files.size() - 1));
  }

  height += (m_margins * 2);

  qDebug() << "new size: " << availableWidth << "x" << height;

  ui->imagesThumbnails->setGeometry(QRect(0, 0, availableWidth, height));
}

void ImagesTab::paintThumbnails(QPaintEvent* e)
{
  const auto availableRect = ui->imagesThumbnails->rect();
  const int thumbSize = availableRect.width() - (m_margins * 2);

  const QRect topRect(
    availableRect.left() + m_margins,
    availableRect.top() + m_margins,
    thumbSize, thumbSize);

  const std::size_t begin = e->rect().top() / (thumbSize + m_padding);
  const std::size_t end = e->rect().bottom() / (thumbSize + m_padding) + 1;

  QPainter painter(ui->imagesThumbnails);

  for (std::size_t i=begin; i<end && i<m_files.size(); ++i) {
    QRect borderRect(
      topRect.left(),
      static_cast<int>(topRect.top() + (i * (thumbSize + m_padding))),
      thumbSize, thumbSize);

    painter.setPen(QColor(Qt::black));
    painter.drawRect(borderRect);

    auto& file = m_files[i];
    if (file.failed) {
      continue;
    }

    const QRect thumbRect = borderRect.adjusted(
      m_border, m_border, -m_border, -m_border);

    if (needsReload(file, thumbRect.size())) {
      if (!file.original.load(file.path)) {
        qCritical() << "failed to load image from " << file.path;
        file.failed = true;
        continue;
      }

      file.thumbnail = file.original.scaled(
        scaledImageSize(file.original.size(), thumbRect.size()),
        Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    if (file.thumbnail.isNull()) {
      continue;
    }

    const QRect scaledThumbRect(
      (thumbRect.left()+thumbRect.width()/2) - file.thumbnail.width()/2,
      (thumbRect.top()+thumbRect.height()/2) - file.thumbnail.height()/2,
      file.thumbnail.width(), file.thumbnail.height());

    painter.drawImage(scaledThumbRect, file.thumbnail);
  }
}

void ImagesTab::thumbnailsMouseEvent(QMouseEvent* e)
{
  if (e->button() != Qt::LeftButton) {
    return;
  }

  const auto availableRect = ui->imagesThumbnails->rect();
  const int thumbSize = availableRect.width() - (m_margins * 2);

  const QRect topRect(
    availableRect.left() + m_margins,
    availableRect.top() + m_margins,
    thumbSize, thumbSize);

  const std::size_t i = e->y() / (thumbSize + m_padding);
  if (i >= m_files.size()) {
    return;
  }

  if (e->x() < topRect.left() || e->x() > (topRect.right() + 1)) {
    return;
  }

  m_image->setImage(m_files[i].original);
}

bool ImagesTab::needsReload(const File& file, const QSize& thumbSize) const
{
  if (file.failed) {
    return false;
  }

  if (file.original.isNull() || file.thumbnail.isNull()) {
    return true;
  }

  if (file.thumbnail.size() != scaledImageSize(file.original.size(), thumbSize)) {
    return true;
  }

  return false;
}

QSize ImagesTab::scaledImageSize(
  const QSize& originalSize, const QSize& thumbSize) const
{
  const auto ratio = std::min({
    1.0,
    static_cast<double>(thumbSize.width()) / originalSize.width(),
    static_cast<double>(thumbSize.height()) / originalSize.height()});

  const QSize scaledSize(
    static_cast<int>(std::round(originalSize.width() * ratio)),
    static_cast<int>(std::round(originalSize.height() * ratio)));

  return scaledSize;
}

#include "modinfodialogimages.h"
#include "ui_modinfodialog.h"

ScalableImage::ScalableImage(QImage original)
  : m_original(std::move(original)), m_border(1)
{
  auto sp = sizePolicy();
  sp.setHeightForWidth(true);
  setSizePolicy(sp);
}

void ScalableImage::setImage(QImage image)
{
  m_original = std::move(image);
  m_scaled = {};

  update();
}

const QImage& ScalableImage::image() const
{
  return m_original;
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
    return;
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

void ScalableImage::mousePressEvent(QMouseEvent* e)
{
  if (e->button() == Qt::LeftButton) {
    emit clicked(m_original);
  }
}


ImagesTab::ImagesTab(
  OrganizerCore& oc, PluginContainer& plugin,
  QWidget* parent, Ui::ModInfoDialog* ui)
    : ModInfoDialogTab(oc, plugin, parent, ui), m_image(new ScalableImage)
{
  ui->imagesImage->layout()->addWidget(m_image);
  ui->imagesThumbnails->setLayout(new QVBoxLayout);

  ui->tabImagesSplitter->setSizes({128, 1});
  ui->tabImagesSplitter->setStretchFactor(0, 0);
  ui->tabImagesSplitter->setStretchFactor(1, 1);
}

void ImagesTab::clear()
{
  m_image->setImage({});

  while (ui->imagesThumbnails->layout()->count() > 0) {
    auto* item = ui->imagesThumbnails->layout()->takeAt(0);
    delete item->widget();
    delete item;
  }

  static_cast<QVBoxLayout*>(ui->imagesThumbnails->layout())->addStretch(1);
}

bool ImagesTab::feedFile(const QString& rootPath, const QString& fullPath)
{
  static constexpr const char* extensions[] = {
    ".png", ".jpg"
  };

  for (const auto* e : extensions) {
    if (fullPath.endsWith(e, Qt::CaseInsensitive)) {
      add(fullPath);
      return true;
    }
  }

  return false;
}

void ImagesTab::add(const QString& fullPath)
{
  QImage image = QImage(fullPath);

  if (image.isNull()) {
    qWarning() << "ImagesTab: '" << fullPath << "' is not a valid image";
    return;
  }

  auto* thumbnail = new ScalableImage(std::move(image));

  QObject::connect(
    thumbnail, &ScalableImage::clicked,
    [&](const QImage& image){ onClicked(image); });

  static_cast<QVBoxLayout*>(ui->imagesThumbnails->layout())->insertWidget(
    ui->imagesThumbnails->layout()->count() - 1, thumbnail);
}

void ImagesTab::onClicked(const QImage& original)
{
  m_image->setImage(original);
}

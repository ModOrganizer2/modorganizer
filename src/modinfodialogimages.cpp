#include "modinfodialogimages.h"
#include "ui_modinfodialog.h"

ThumbnailButton::ThumbnailButton(const QString& fullPath, QImage original)
  : m_original(std::move(original))
{
  const auto ratio = static_cast<double>(m_original.width()) / m_original.height();

  QImage thumbnail;
  if (ratio > 1.34) {
    thumbnail = m_original.scaledToWidth(128);
  } else {
    thumbnail = m_original.scaledToHeight(96);
  }

  setIcon(QPixmap::fromImage(thumbnail));
  setIconSize(QSize(thumbnail.width(), thumbnail.height()));

  connect(this, &QPushButton::clicked, [&]{ emit open(m_original); });
}

const QImage& ThumbnailButton::image() const
{
  return m_original;
}


ImagesTab::ImagesTab(Ui::ModInfoDialog* ui)
  : ui(ui)
{
}

void ImagesTab::clear()
{
  ui->imageLabel->setPixmap({});

  while (ui->imageThumbnails->count() > 0) {
    auto* item = ui->imageThumbnails->takeAt(0);
    delete item->widget();
    delete item;
  }
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

  auto* button = new ThumbnailButton(fullPath, std::move(image));

  QObject::connect(
    button, &ThumbnailButton::open,
    [&](const QImage& image){ onOpen(image); });

  ui->imageThumbnails->addWidget(button);
}

void ImagesTab::onOpen(const QImage& original)
{
  QImage image;

  const auto ratio = static_cast<double>(original.width()) / original.height();

  if (ratio > 1.34) {
    image = original.scaledToWidth(ui->imageLabel->geometry().width());
  } else {
    image = original.scaledToHeight(ui->imageLabel->geometry().height());
  }

  ui->imageLabel->setPixmap(QPixmap::fromImage(image));
}

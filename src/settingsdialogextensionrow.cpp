#include "settingsdialogextensionrow.h"

#include "ui_settingsdialogextensionrow.h"

using namespace MOBase;

namespace
{

const auto& defaultIcon()
{
  static QIcon icon;

  if (icon.isNull()) {
    const QImage baseIcon(":/MO/gui/app_icon");
    QImage grayIcon = baseIcon.convertToFormat(QImage::Format_ARGB32);
    {
      for (int y = 0; y < grayIcon.height(); ++y) {
        QRgb* scanLine = (QRgb*)grayIcon.scanLine(y);
        for (int x = 0; x < grayIcon.width(); ++x) {
          QRgb pixel = *scanLine;
          uint ci    = uint(qGray(pixel));
          *scanLine  = qRgba(ci, ci, ci, qAlpha(pixel) / 3);
          ++scanLine;
        }
      }
    }
    icon = QIcon(QPixmap::fromImage(grayIcon));
  }

  return icon;
}

}  // namespace

ExtensionListItemWidget::ExtensionListItemWidget(const IExtension& extension)
    : ui{new Ui::ExtensionListItemWidget()}, m_extension{&extension}
{
  ui->setupUi(this);

  const auto& metadata = extension.metadata();
  const auto& icon     = metadata.icon().isNull() ? defaultIcon() : metadata.icon();

  ui->extensionIcon->setPixmap(icon.pixmap(QSize(48, 48)));
  ui->extensionName->setText(extension.metadata().name());

  ui->extensionDescription->setText(extension.metadata().description());
  ui->extensionAuthor->setText(extension.metadata().author().name());
}

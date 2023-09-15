#include "updatedialog.h"
#include "ui_updatedialog.h"

#include "lootdialog.h"  // for MarkdownPage
#include <QWebChannel>

using namespace MOBase;

UpdateDialog::UpdateDialog(QWidget* parent)
    : QDialog(parent, Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint),
      ui(new Ui::UpdateDialog)
{
  // Basic UI stuff
  ui->setupUi(this);
  connect(ui->installButton, &QPushButton::pressed, this, [&] {
    done(QDialog::Accepted);
  });
  connect(ui->cancelButton, &QPushButton::pressed, this, [&] {
    done(QDialog::Rejected);
  });

  // Replace a label with an icon
  QIcon icon     = style()->standardIcon(QStyle::SP_MessageBoxQuestion);
  QPixmap pixmap = icon.pixmap(QSize(32, 32));
  ui->iconLabel->setPixmap(pixmap);
  ui->iconLabel->setScaledContents(true);

  // Setting up the Markdown stuff
  auto* page = new MarkdownPage(this);
  ui->detailsWebView->setPage(page);

  auto* channel = new QWebChannel(this);
  channel->registerObject("content", &m_changeLogs);
  page->setWebChannel(channel);

  const QString path = QApplication::applicationDirPath() + "/resources/markdown.html";
  QFile f(path);

  if (f.open(QFile::ReadOnly)) {
    const QString html = f.readAll();
    if (!html.isEmpty()) {
      ui->detailsWebView->setHtml(html);
    } else {
      log::error("failed to read '{}', {}", path, f.errorString());
    }
  } else {
    log::error("can't open '{}', {}", path, f.errorString());
  }

  // Setting up the expander
  m_expander.set(ui->detailsButton, ui->detailsWidget);
  connect(&m_expander, &ExpanderWidget::toggled, this, [&] {
    adjustSize();
  });

  // Adjust sizes after the expander hides stuff
  adjustSize();
}

UpdateDialog::~UpdateDialog() = default;

void UpdateDialog::setChangeLogs(const QString& text)
{
  m_changeLogs.setText(text);
}

void UpdateDialog::setVersions(const QString& oldVersion, const QString& newVersion)
{
  ui->updateLabel->setText(tr("Mod Organizer %1 is available.  The current version is "
                              "%2.  Updating will not affect your mods or profiles.")
                               .arg(newVersion)
                               .arg(oldVersion));
}

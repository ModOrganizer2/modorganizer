#include "modinfodialogtextfiles.h"
#include "ui_modinfodialog.h"
#include <QMessageBox>

class TextFileItem : public QListWidgetItem
{
public:
  TextFileItem(const QString& rootPath, QString fullPath)
    : m_fullPath(std::move(fullPath))
  {
    setText(m_fullPath.mid(rootPath.length() + 1));
  }

  const QString& fullPath() const
  {
    return m_fullPath;
  }

private:
  QString m_fullPath;
};


TextFilesTab::TextFilesTab(Ui::ModInfoDialog* ui)
  : ui(ui)
{
  ui->textFileView->setupToolbar();

  ui->tabTextSplitter->setSizes({200, 1});
  ui->tabTextSplitter->setStretchFactor(0, 0);
  ui->tabTextSplitter->setStretchFactor(1, 1);

  QObject::connect(
    ui->textFileList, &QListWidget::currentItemChanged,
    [&](auto* current, auto* previous){ onSelection(current, previous); });
}

void TextFilesTab::clear()
{
  ui->textFileList->clear();
}

bool TextFilesTab::feedFile(const QString& rootPath, const QString& fullPath)
{
  if (fullPath.endsWith(".txt", Qt::CaseInsensitive)) {
    ui->textFileList->addItem(new TextFileItem(rootPath, fullPath));
    return true;
  }

  return false;
}

bool TextFilesTab::canClose()
{
  if (!ui->textFileView->dirty()) {
    return true;
  }

  const int res = QMessageBox::question(
    ui->tabText,
    QObject::tr("Save changes?"),
    QObject::tr("Save changes to \"%1\"?").arg(ui->textFileView->filename()),
    QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

  if (res == QMessageBox::Cancel) {
    return false;
  }

  if (res == QMessageBox::Yes) {
      ui->textFileView->save();
  }

  return true;
}

void TextFilesTab::onSelection(
  QListWidgetItem* current, QListWidgetItem* previous)
{
  auto* item = dynamic_cast<TextFileItem*>(current);
  if (!item) {
    qCritical("TextFilesTab: item is not a TextFileItem");
    return;
  }

  if (!canClose()) {
    ui->textFileList->setCurrentItem(previous, QItemSelectionModel::Current);
    return;
  }

  ui->textFileView->load(item->fullPath());
}

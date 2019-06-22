#include "modinfodialogtextfiles.h"
#include "ui_modinfodialog.h"
#include <QMessageBox>

class FileListItem : public QListWidgetItem
{
public:
  FileListItem(const QString& rootPath, QString fullPath)
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


GenericFilesTab::GenericFilesTab(
  QWidget* parent, QListWidget* list, QSplitter* sp, TextEditor* e)
    : m_parent(parent), m_list(list), m_editor(e)
{
  m_editor->setupToolbar();

  sp->setSizes({200, 1});
  sp->setStretchFactor(0, 0);
  sp->setStretchFactor(1, 1);

  QObject::connect(
    m_list, &QListWidget::currentItemChanged,
    [&](auto* current, auto* previous){ onSelection(current, previous); });
}

void GenericFilesTab::clear()
{
  m_list->clear();
  select(nullptr);
}

bool GenericFilesTab::canClose()
{
  if (!m_editor->dirty()) {
    return true;
  }

  const int res = QMessageBox::question(
    m_parent,
    QObject::tr("Save changes?"),
    QObject::tr("Save changes to \"%1\"?").arg(m_editor->filename()),
    QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

  if (res == QMessageBox::Cancel) {
    return false;
  }

  if (res == QMessageBox::Yes) {
    m_editor->save();
  }

  return true;
}

bool GenericFilesTab::feedFile(const QString& rootPath, const QString& fullPath)
{
  static constexpr const char* extensions[] = {
    ".txt"
  };

  for (const auto* e : extensions) {
    if (wantsFile(rootPath, fullPath)) {
      m_list->addItem(new FileListItem(rootPath, fullPath));
      return true;
    }
  }

  return false;
}

void GenericFilesTab::onSelection(
  QListWidgetItem* current, QListWidgetItem* previous)
{
  auto* item = dynamic_cast<FileListItem*>(current);
  if (!item) {
    qCritical("TextFilesTab: item is not a FileListItem");
    return;
  }

  if (!canClose()) {
    m_list->setCurrentItem(previous, QItemSelectionModel::Current);
    return;
  }

  select(item);
}

void GenericFilesTab::select(FileListItem* item)
{
  if (item) {
    m_editor->setEnabled(true);
    m_editor->load(item->fullPath());
  } else {
    m_editor->setEnabled(false);
  }
}


TextFilesTab::TextFilesTab(QWidget* parent, Ui::ModInfoDialog* ui)
  : GenericFilesTab(
    parent, ui->textFileList, ui->tabTextSplitter, ui->textFileEditor)
{
}

bool TextFilesTab::wantsFile(const QString& rootPath, const QString& fullPath) const
{
  static constexpr const char* extensions[] = {
    ".txt"
  };

  for (const auto* e : extensions) {
    if (fullPath.endsWith(e, Qt::CaseInsensitive)) {
      return true;
    }
  }

  return false;
}

IniFilesTab::IniFilesTab(QWidget* parent, Ui::ModInfoDialog* ui)
  : GenericFilesTab(
      parent, ui->iniFileList, ui->tabIniSplitter, ui->iniFileEditor)
{
}

bool IniFilesTab::wantsFile(const QString& rootPath, const QString& fullPath) const
{
  static constexpr const char* extensions[] = {
    ".ini", ".cfg"
  };

  for (const auto* e : extensions) {
    if (fullPath.endsWith(e, Qt::CaseInsensitive)) {
      if (!fullPath.endsWith("meta.ini")) {
        return true;
      }
    }
  }

  return false;
}

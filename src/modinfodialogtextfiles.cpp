#include "modinfodialogtextfiles.h"
#include "ui_modinfodialog.h"
#include "modinfodialog.h"
#include <QMessageBox>

class FileListModel : public QAbstractItemModel
{
public:
  void clear()
  {
    m_files.clear();
    endResetModel();
  }

  QModelIndex index(int row, int col, const QModelIndex& ={}) const override
  {
    return createIndex(row, col);
  }

  QModelIndex parent(const QModelIndex&) const override
  {
    return {};
  }

  int rowCount(const QModelIndex& ={}) const override
  {
    return static_cast<int>(m_files.size());
  }

  int columnCount(const QModelIndex& ={}) const override
  {
    return 1;
  }

  QVariant data(const QModelIndex& index, int role) const override
  {
    if (role == Qt::DisplayRole) {
      const auto row = index.row();
      if (row < 0) {
        return {};
      }

      const auto i = static_cast<std::size_t>(row);
      if (i >= m_files.size()) {
        return {};
      }

      return m_files[i].text;
    }

    return {};
  }

  void add(const QString& rootPath, QString fullPath)
  {
    QString text = fullPath.mid(rootPath.length() + 1);
    m_files.emplace_back(std::move(fullPath), std::move(text));
  }

  void finished()
  {
    std::sort(m_files.begin(), m_files.end(), [](const auto& a, const auto& b) {
      return (naturalCompare(a.text, b.text) < 0);
    });

    endResetModel();
  }

  QString fullPath(const QModelIndex& index) const
  {
    const auto row = index.row();
    if (row < 0) {
      return {};
    }

    const auto i = static_cast<std::size_t>(row);
    if (i >= m_files.size()) {
      return {};
    }

    return m_files[i].fullPath;
  }

private:
  struct File
  {
    QString fullPath;
    QString text;

    File(QString fp, QString t)
      : fullPath(std::move(fp)), text(std::move(t))
    {
    }
  };

  std::deque<File> m_files;
};


GenericFilesTab::GenericFilesTab(
  OrganizerCore& oc, PluginContainer& plugin,
  QWidget* parent, Ui::ModInfoDialog* ui, int id,
  QListView* list, QSplitter* sp, TextEditor* e) :
    ModInfoDialogTab(oc, plugin, parent, ui, id),
    m_list(list), m_editor(e), m_model(new FileListModel)
{
  m_list->setModel(m_model);
  m_editor->setupToolbar();

  sp->setSizes({200, 1});
  sp->setStretchFactor(0, 0);
  sp->setStretchFactor(1, 1);

  QObject::connect(
    m_list->selectionModel(), &QItemSelectionModel::currentRowChanged,
    [&](auto current, auto previous){ onSelection(current, previous); });
}

void GenericFilesTab::clear()
{
  m_model->clear();
  select({});
  setHasData(false);
}

bool GenericFilesTab::canClose()
{
  if (!m_editor->dirty()) {
    return true;
  }

  const int res = QMessageBox::question(
    parentWidget(),
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
      m_model->add(rootPath, fullPath);
      setHasData(true);
      return true;
    }
  }

  return false;
}

void GenericFilesTab::update()
{
  m_model->finished();
}

void GenericFilesTab::onSelection(
  const QModelIndex& current, const QModelIndex& previous)
{
  if (!canClose()) {
    m_list->selectionModel()->select(previous, QItemSelectionModel::Current);
    return;
  }

  select(current);
}

void GenericFilesTab::select(const QModelIndex& index)
{
  if (!index.isValid()) {
    m_editor->setEnabled(false);
    return;
  }

  m_editor->setEnabled(true);
  m_editor->load(m_model->fullPath(index));
}


TextFilesTab::TextFilesTab(
  OrganizerCore& oc, PluginContainer& plugin,
  QWidget* parent, Ui::ModInfoDialog* ui, int id)
    : GenericFilesTab(
        oc, plugin, parent, ui, id,
        ui->textFileList, ui->tabTextSplitter, ui->textFileEditor)
{
}

bool TextFilesTab::wantsFile(const QString& rootPath, const QString& fullPath) const
{
  static const QString extensions[] = {".txt"};

  for (const auto& e : extensions) {
    if (fullPath.endsWith(e, Qt::CaseInsensitive)) {
      return true;
    }
  }

  return false;
}

IniFilesTab::IniFilesTab(
  OrganizerCore& oc, PluginContainer& plugin,
  QWidget* parent, Ui::ModInfoDialog* ui, int id)
    : GenericFilesTab(
        oc, plugin, parent, ui, id,
        ui->iniFileList, ui->tabIniSplitter, ui->iniFileEditor)
{
}

bool IniFilesTab::wantsFile(const QString& rootPath, const QString& fullPath) const
{
  static const QString extensions[] = {".ini", ".cfg"};
  static const QString meta("meta.ini");

  for (const auto& e : extensions) {
    if (fullPath.endsWith(e, Qt::CaseInsensitive)) {
      if (!fullPath.endsWith(meta)) {
        return true;
      }
    }
  }

  return false;
}

#include "modinfodialogtab.h"
#include "ui_modinfodialog.h"
#include "texteditor.h"
#include "directoryentry.h"

ModInfoDialogTab::ModInfoDialogTab(
  OrganizerCore& oc, PluginContainer& plugin,
  QWidget* parent, Ui::ModInfoDialog* ui, int index) :
    ui(ui), m_core(oc), m_plugin(plugin), m_parent(parent),
    m_origin(nullptr), m_tabIndex(index)
{
}

void ModInfoDialogTab::update()
{
  // no-op
}

bool ModInfoDialogTab::feedFile(const QString&, const QString&)
{
  // no-op
  return false;
}

bool ModInfoDialogTab::canClose()
{
  return true;
}

void ModInfoDialogTab::saveState(Settings&)
{
  // no-op
}

void ModInfoDialogTab::restoreState(const Settings& s)
{
  // no-op
}

bool ModInfoDialogTab::deleteRequested()
{
  // no-op
  return false;
}

bool ModInfoDialogTab::canHandleSeparators() const
{
  return false;
}

bool ModInfoDialogTab::canHandleUnmanaged() const
{
  return false;
}

void ModInfoDialogTab::setMod(ModInfo::Ptr mod, MOShared::FilesOrigin* origin)
{
  m_mod = mod;
  m_origin = origin;
}

ModInfo::Ptr ModInfoDialogTab::mod() const
{
  return m_mod;
}

MOShared::FilesOrigin* ModInfoDialogTab::origin() const
{
  return m_origin;
}

int ModInfoDialogTab::tabIndex() const
{
  return m_tabIndex;
}

OrganizerCore& ModInfoDialogTab::core()
{
  return m_core;
}

PluginContainer& ModInfoDialogTab::plugin()
{
  return m_plugin;
}

QWidget* ModInfoDialogTab::parentWidget()
{
  return m_parent;
}

void ModInfoDialogTab::emitOriginModified()
{
  if (m_origin) {
    emit originModified(m_origin->getID());
  }
}

void ModInfoDialogTab::emitModOpen(QString name)
{
  emit modOpen(name);
}


NotesTab::NotesTab(
  OrganizerCore& oc, PluginContainer& plugin,
  QWidget* parent, Ui::ModInfoDialog* ui, int index)
   : ModInfoDialogTab(oc, plugin, parent, ui, index)
{
  connect(ui->commentsEdit, &QLineEdit::editingFinished, [&]{ onComments(); });
  connect(ui->notesEdit, &HTMLEditor::editingFinished, [&]{ onNotes(); });
}

void NotesTab::clear()
{
  ui->commentsEdit->clear();
  ui->notesEdit->clear();
}

void NotesTab::update()
{
  ui->commentsEdit->setText(mod()->comments());
  ui->notesEdit->setText(mod()->notes());
}

bool NotesTab::canHandleSeparators() const
{
  return true;
}

void NotesTab::onComments()
{
  mod()->setComments(ui->commentsEdit->text());
}

void NotesTab::onNotes()
{
  // Avoid saving html stub if notes field is empty.
  if (ui->notesEdit->toPlainText().isEmpty()) {
    mod()->setNotes({});
  } else {
    mod()->setNotes(ui->notesEdit->toHtml());
  }
}

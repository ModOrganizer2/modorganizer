#include "modinfodialogtab.h"
#include "modinfo.h"
#include "shared/filesorigin.h"
#include "texteditor.h"
#include "ui_modinfodialog.h"

ModInfoDialogTab::ModInfoDialogTab(ModInfoDialogTabContext cx)
    : ui(cx.ui), m_core(cx.core), m_plugins(cx.plugins), m_parent(cx.parent),
      m_origin(cx.origin), m_tabID(cx.id), m_hasData(false), m_firstActivation(true)
{}

void ModInfoDialogTab::activated()
{
  if (m_firstActivation) {
    m_firstActivation = false;
    firstActivation();
  }
}

void ModInfoDialogTab::resetFirstActivation()
{
  m_firstActivation = true;
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

void ModInfoDialogTab::firstActivation()
{
  // no-op
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

bool ModInfoDialogTab::usesOriginFiles() const
{
  return true;
}

void ModInfoDialogTab::setMod(ModInfoPtr mod, MOShared::FilesOrigin* origin)
{
  m_mod    = mod;
  m_origin = origin;
}

ModInfo& ModInfoDialogTab::mod() const
{
  Q_ASSERT(m_mod);
  return *m_mod;
}

ModInfoPtr ModInfoDialogTab::modPtr() const
{
  Q_ASSERT(m_mod);
  return m_mod;
}

MOShared::FilesOrigin* ModInfoDialogTab::origin() const
{
  return m_origin;
}

ModInfoTabIDs ModInfoDialogTab::tabID() const
{
  return m_tabID;
}

bool ModInfoDialogTab::hasData() const
{
  return m_hasData;
}

OrganizerCore& ModInfoDialogTab::core()
{
  return m_core;
}

PluginManager& ModInfoDialogTab::plugins()
{
  return m_plugins;
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

void ModInfoDialogTab::setHasData(bool b)
{
  if (m_hasData != b) {
    m_hasData = b;
    emit hasDataChanged();
  }
}

void ModInfoDialogTab::setFocus()
{
  emit wantsFocus();
}

NotesTab::NotesTab(ModInfoDialogTabContext cx) : ModInfoDialogTab(std::move(cx))
{
  connect(ui->comments, &QLineEdit::editingFinished, [&] {
    onComments();
  });
  connect(ui->notes, &HTMLEditor::editingFinished, [&] {
    onNotes();
  });
  connect(ui->setColorButton, &QPushButton::clicked, [&] {
    onSetColor();
  });
  connect(ui->resetColorButton, &QPushButton::clicked, [&] {
    onResetColor();
  });
}

void NotesTab::updateCommentsColor(bool clear)
{
  QPalette commentPalette = QPalette();

  if (!clear) {
    auto modColor = mod().color();
    if (modColor.isValid()) {
      commentPalette.setColor(QPalette::Base, modColor);
      commentPalette.setColor(QPalette::Text, ColorSettings::idealTextColor(modColor));
    }
  }
  ui->comments->setPalette(commentPalette);
}

void NotesTab::clear()
{
  ui->comments->clear();
  ui->notes->clear();
  updateCommentsColor(true);
  setHasData(false);
}

void NotesTab::update()
{
  const auto comments = mod().comments();
  const auto notes    = mod().notes();

  ui->comments->setText(comments);
  ui->notes->setText(notes);
  updateCommentsColor();
  checkHasData();
}

bool NotesTab::canHandleSeparators() const
{
  return true;
}

void NotesTab::onComments()
{
  mod().setComments(ui->comments->text());
  checkHasData();
}

void NotesTab::onNotes()
{
  // Avoid saving html stub if notes field is empty.
  if (ui->notes->toPlainText().isEmpty()) {
    mod().setNotes({});
  } else {
    mod().setNotes(ui->notes->toHtml());
  }

  checkHasData();
}

void NotesTab::onSetColor()
{
  QColorDialog dialog(m_parent);
  dialog.setOption(QColorDialog::ShowAlphaChannel);

  QColor currentColor = mod().color();

  if (currentColor.isValid()) {
    dialog.setCurrentColor(currentColor);
  }

  if (!dialog.exec())
    return;

  currentColor = dialog.currentColor();
  if (!currentColor.isValid())
    return;

  mod().setColor(currentColor);
  updateCommentsColor();
  checkHasData();
}

void NotesTab::onResetColor()
{
  QColor color = QColor();

  mod().setColor(color);
  updateCommentsColor();
  checkHasData();
}

bool NotesTab::usesOriginFiles() const
{
  return false;
}

void NotesTab::checkHasData()
{
  setHasData(!ui->comments->text().isEmpty() || !ui->notes->toPlainText().isEmpty() ||
             mod().color().isValid());
}

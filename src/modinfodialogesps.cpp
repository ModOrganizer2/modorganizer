#include "modinfodialogesps.h"
#include "ui_modinfodialog.h"
#include <report.h>

using MOBase::reportError;

class ESP
{
public:
  ESP(QString rootPath, QString relativePath)
    : m_rootPath(std::move(rootPath)), m_active(false)
  {
    if (relativePath.contains('/')) {
      m_inactivePath = relativePath;
    } else {
      m_activePath = relativePath;
      m_active = true;
    }
  }

  const QString& rootPath() const
  {
    return m_rootPath;
  }

  const QString& relativePath() const
  {
    if (m_active) {
      return m_activePath;
    } else {
      return m_inactivePath;
    }
  }

  const QString& activePath() const
  {
    return m_activePath;
  }

  const QString& inactivePath() const
  {
    return m_inactivePath;
  }

  QFileInfo fileInfo() const
  {
    return m_rootPath + QDir::separator() + relativePath();
  }

  bool isActive() const
  {
    return m_active;
  }

  bool activate(const QString& newName)
  {
    QDir root(m_rootPath);

    if (root.rename(m_inactivePath, newName)) {
      m_active = true;
      m_activePath = newName;

      if (QFileInfo(m_inactivePath).fileName() != newName) {
        // file was renamed
        m_inactivePath = QFileInfo(m_inactivePath).path() + QDir::separator() + newName;
      }

      return true;
    }

    return false;
  }

  bool deactivate(const QString& newName)
  {
    QDir root(m_rootPath);

    if (root.rename(m_activePath, newName)) {
      m_active = false;
      m_inactivePath = newName;
      return true;
    }

    return false;
  }

private:
  QString m_rootPath;
  QString m_activePath;
  QString m_inactivePath;
  bool m_active;
};


class ESPItem : public QListWidgetItem
{
public:
  ESPItem(ESP esp)
    : m_esp(std::move(esp))
  {
    updateText();
  }

  const ESP& esp() const
  {
    return m_esp;
  }

  void setESP(ESP esp)
  {
    m_esp = esp;
    updateText();
  }

private:
  ESP m_esp;

  void updateText()
  {
    setText(m_esp.fileInfo().fileName());
  }
};


ESPsTab::ESPsTab(
  OrganizerCore& oc, PluginContainer& plugin,
  QWidget* parent, Ui::ModInfoDialog* ui, int id)
    : ModInfoDialogTab(oc, plugin, parent, ui, id)
{
  QObject::connect(
    ui->activateESP, &QToolButton::clicked, [&]{ onActivate(); });

  QObject::connect(
    ui->deactivateESP, &QToolButton::clicked, [&]{ onDeactivate(); });
}

void ESPsTab::clear()
{
  ui->inactiveESPList->clear();
  ui->activeESPList->clear();
  setHasData(false);
}

bool ESPsTab::feedFile(const QString& rootPath, const QString& fullPath)
{
  static constexpr const char* extensions[] = {
    ".esp", ".esm", ".esl"
  };

  for (const auto* e : extensions) {
    if (fullPath.endsWith(e, Qt::CaseInsensitive)) {
      QString relativePath = fullPath.mid(rootPath.length() + 1);

      auto* item = new ESPItem({rootPath, relativePath});

      if (item->esp().isActive()) {
        ui->activeESPList->addItem(item);
      } else {
        ui->inactiveESPList->addItem(item);
      }

      setHasData(true);
      return true;
    }
  }

  return false;
}

void ESPsTab::onActivate()
{
  auto* item = selectedInactive();
  if (!item) {
    qWarning("ESPsTab::onActivate(): no selection");
    return;
  }

  ESP esp = item->esp();

  if (esp.isActive()) {
    qWarning("ESPsTab::onActive(): item is already active");
    return;
  }

  QDir root(esp.rootPath());
  const QFileInfo file(esp.fileInfo());

  QString newName = file.fileName();

  while (root.exists(newName)) {
    bool okClicked = false;

    newName = QInputDialog::getText(
      parentWidget(),
      QObject::tr("File Exists"),
      QObject::tr("A file with that name exists, please enter a new one"),
      QLineEdit::Normal, file.fileName(), &okClicked);

    if (!okClicked) {
      return;
    }

    if (newName.isEmpty()) {
      newName = file.fileName();
    }
  }

  if (esp.activate(newName)) {
    ui->inactiveESPList->takeItem(ui->inactiveESPList->row(item));
    ui->activeESPList->addItem(item);
    item->setESP(esp);
  } else {
    reportError(QObject::tr("Failed to move file"));
  }
}

void ESPsTab::onDeactivate()
{
  auto* item = selectedActive();
  if (!item) {
    qWarning("ESPsTab::onDeactivate(): no selection");
    return;
  }

  ESP esp = item->esp();

  if (!esp.isActive()) {
    qWarning("ESPsTab::onDeactivate(): item is already inactive");
    return;
  }

  QDir root(esp.rootPath());

  // if we moved the file from optional to active in this session, we move the
  // file back to where it came from. Otherwise, it is moved to the new folder
  // "optional"

  QString newName = esp.inactivePath();

  if (newName.isEmpty()) {
    if (!root.exists("optional")) {
      if (!root.mkdir("optional")) {
        reportError(QObject::tr("Failed to create directory \"optional\""));
        return;
      }
    }

    newName = QString("optional") + QDir::separator() + esp.fileInfo().fileName();
  }

  if (esp.deactivate(newName)) {
    ui->activeESPList->takeItem(ui->activeESPList->row(item));
    ui->inactiveESPList->addItem(item);
    item->setESP(esp);
  } else {
    reportError(QObject::tr("Failed to move file"));
  }
}

ESPItem* ESPsTab::selectedInactive()
{
  auto* item = ui->inactiveESPList->currentItem();
  if (!item) {
    return nullptr;
  }

  auto* esp = dynamic_cast<ESPItem*>(item);
  if (!esp) {
    qCritical("ESPsTab: inactive item is not an ESPItem");
    return nullptr;
  }

  return esp;
}

ESPItem* ESPsTab::selectedActive()
{
  auto* item = ui->activeESPList->currentItem();
  if (!item) {
    return nullptr;
  }

  auto* esp = dynamic_cast<ESPItem*>(item);
  if (!esp) {
    qCritical("ESPsTab: active item is not an ESPItem");
    return nullptr;
  }

  return esp;
}

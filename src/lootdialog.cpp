#include "lootdialog.h"
#include "ui_lootdialog.h"
#include "loot.h"
#include "organizercore.h"
#include <utility.h>
#include <expanderwidget.h>
#include <QWebChannel>

using namespace MOBase;


MarkdownDocument::MarkdownDocument(QObject* parent)
  : QObject(parent)
{
}

void MarkdownDocument::setText(const QString& text)
{
  if (m_text == text)
    return;

  m_text = text;
  emit textChanged(m_text);
}


MarkdownPage::MarkdownPage(QObject* parent)
  : QWebEnginePage(parent)
{
}

bool MarkdownPage::acceptNavigationRequest(const QUrl &url, NavigationType, bool)
{
  static const QStringList allowed = {"qrc", "data"};

  if (!allowed.contains(url.scheme())) {
    QDesktopServices::openUrl(url);
    return false;
  }

  return true;
}


LootDialog::LootDialog(QWidget* parent, OrganizerCore& core, Loot& loot) :
  QDialog(parent), ui(new Ui::LootDialog), m_core(core), m_loot(loot),
  m_finished(false), m_cancelling(false)
{
  createUI();

  QObject::connect(
    &m_loot, &Loot::output, this,
    [&](auto&& s){ addOutput(s); }, Qt::QueuedConnection);

  QObject::connect(
    &m_loot, &Loot::progress,
    this, [&](auto&& p){ setProgress(p); }, Qt::QueuedConnection);

  QObject::connect(
    &m_loot, &Loot::log, this,
    [&](auto&& lv, auto&& s){ log(lv, s); }, Qt::QueuedConnection);

  QObject::connect(
    &m_loot, &Loot::finished, this,
    [&]{ onFinished(); }, Qt::QueuedConnection);
}

LootDialog::~LootDialog() = default;

void LootDialog::setText(const QString& s)
{
  ui->progressText->setText(s);
}

void LootDialog::setProgress(lootcli::Progress p)
{
  setText(progressToString(p));

  if (p == lootcli::Progress::Done) {
    ui->progressBar->setRange(0, 1);
    ui->progressBar->setValue(1);
  }
}

QString LootDialog::progressToString(lootcli::Progress p)
{
  using P = lootcli::Progress;

  switch (p)
  {
    case P::CheckingMasterlistExistence: return tr("Checking masterlist existence");
    case P::UpdatingMasterlist: return tr("Updating masterlist");
    case P::LoadingLists: return tr("Loading lists");
    case P::ReadingPlugins: return tr("Reading plugins");
    case P::SortingPlugins: return tr("Sorting plugins");
    case P::WritingLoadorder: return tr("Writing loadorder.txt");
    case P::ParsingLootMessages: return tr("Parsing loot messages");
    case P::Done: return tr("Done");
    default: return QString("unknown progress %1").arg(static_cast<int>(p));
  }
}

void LootDialog::addOutput(const QString& s)
{
  if (m_core.settings().diagnostics().lootLogLevel() > lootcli::LogLevels::Debug) {
    return;
  }

  const auto lines = s.split(QRegExp("[\\r\\n]"), QString::SkipEmptyParts);

  for (auto&& line : lines) {
    if (line.isEmpty()) {
      continue;
    }

    addLineOutput(line);
  }
}

bool LootDialog::result() const
{
  return m_loot.result();
}

void LootDialog::cancel()
{
  if (!m_finished && !m_cancelling) {
    addLineOutput(tr("Stopping LOOT..."));
    m_loot.cancel();
    ui->buttons->setEnabled(false);
    m_cancelling = true;
  }
}

void LootDialog::openReport()
{
  const auto path = m_loot.outPath();
  shell::Open(path);
}

void LootDialog::createUI()
{
  ui->setupUi(this);
  ui->progressBar->setMaximum(0);

  auto* page = new MarkdownPage(this);
  ui->report->setPage(page);

  auto* channel = new QWebChannel(this);
  channel->registerObject("content", &m_report);
  page->setWebChannel(channel);

  const QString path = QApplication::applicationDirPath() + "/resources/markdown.html";
  QFile f(path);

  if (f.open(QFile::ReadOnly)) {
    const QString html = f.readAll();
    if (!html.isEmpty()) {
      ui->report->setHtml(html);
    } else {
      log::error("failed to read '{}', {}", path, f.errorString());
    }
  } else {
    log::error("can't open '{}', {}", path, f.errorString());
  }

  ui->openJsonReport->setEnabled(false);
  connect(ui->openJsonReport, &QPushButton::clicked, [&]{ openReport(); });

  new ExpanderWidget(ui->details, ui->detailsPanel);

  connect(ui->buttons, &QDialogButtonBox::clicked, [&](auto* b){ onButton(b); });

  resize(480, 275);
}

void LootDialog::closeEvent(QCloseEvent* e)
{
  if (m_finished) {
    QDialog::closeEvent(e);
  } else {
    cancel();
    e->ignore();
  }
}

void LootDialog::onButton(QAbstractButton* b)
{
  if (ui->buttons->buttonRole(b) == QDialogButtonBox::RejectRole) {
    if (m_finished) {
      close();
    } else {
      cancel();
    }
  }
}

void LootDialog::addLineOutput(const QString& line)
{
  ui->output->appendPlainText(line);
}

void LootDialog::onFinished()
{
  m_finished = true;

  if (m_cancelling) {
    close();
  } else {
    handleReport();
    ui->openJsonReport->setEnabled(true);
    ui->buttons->setStandardButtons(QDialogButtonBox::Close);
  }
}

void LootDialog::log(log::Levels lv, const QString& s)
{
  if (lv >= log::Levels::Warning) {
    log::log(lv, "{}", s);
  }

  if (m_core.settings().diagnostics().lootLogLevel() > lootcli::LogLevels::Debug) {
    addLineOutput(QString("[%1] %2").arg(log::levelToString(lv)).arg(s));
  }
}

void LootDialog::handleReport()
{
  const auto& lootReport = m_loot.report();

  if (!lootReport.messages.empty()) {
    addLineOutput("");
  }

  for (auto&& m : lootReport.messages) {
    log(m.type, m.text);
  }

  for (auto&& p : lootReport.plugins) {
    m_core.pluginList()->addLootReport(p.name, p);
  }

  m_report.setText(lootReport.toMarkdown());
}

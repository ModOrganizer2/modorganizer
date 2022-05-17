#include "lootdialog.h"
#include "loot.h"
#include "organizercore.h"
#include "ui_lootdialog.h"
#include <QWebChannel>
#include <utility.h>

using namespace MOBase;

QString progressToString(lootcli::Progress p)
{
  using P = lootcli::Progress;

  static const std::map<P, QString> map = {
      {P::CheckingMasterlistExistence, QObject::tr("Checking masterlist existence")},
      {P::UpdatingMasterlist, QObject::tr("Updating masterlist")},
      {P::LoadingLists, QObject::tr("Loading lists")},
      {P::ReadingPlugins, QObject::tr("Reading plugins")},
      {P::SortingPlugins, QObject::tr("Sorting plugins")},
      {P::WritingLoadorder, QObject::tr("Writing loadorder.txt")},
      {P::ParsingLootMessages, QObject::tr("Parsing loot messages")},
      {P::Done, QObject::tr("Done")}};

  auto itor = map.find(p);
  if (itor == map.end()) {
    return QString("unknown progress %1").arg(static_cast<int>(p));
  } else {
    return itor->second;
  }
}

MarkdownDocument::MarkdownDocument(QObject* parent) : QObject(parent) {}

void MarkdownDocument::setText(const QString& text)
{
  if (m_text == text)
    return;

  m_text = text;
  emit textChanged(m_text);
}

MarkdownPage::MarkdownPage(QObject* parent) : QWebEnginePage(parent) {}

bool MarkdownPage::acceptNavigationRequest(const QUrl& url, NavigationType, bool)
{
  static const QStringList allowed = {"qrc", "data"};

  if (!allowed.contains(url.scheme())) {
    shell::Open(url);
    return false;
  }

  return true;
}

LootDialog::LootDialog(QWidget* parent, OrganizerCore& core, Loot& loot)
    : QDialog(parent, Qt::WindowMaximizeButtonHint), ui(new Ui::LootDialog),
      m_core(core), m_loot(loot), m_finished(false), m_cancelling(false)
{
  createUI();

  QObject::connect(
      &m_loot, &Loot::output, this,
      [&](auto&& s) {
        addOutput(s);
      },
      Qt::QueuedConnection);

  QObject::connect(
      &m_loot, &Loot::progress, this,
      [&](auto&& p) {
        setProgress(p);
      },
      Qt::QueuedConnection);

  QObject::connect(
      &m_loot, &Loot::log, this,
      [&](auto&& lv, auto&& s) {
        log(lv, s);
      },
      Qt::QueuedConnection);

  QObject::connect(
      &m_loot, &Loot::finished, this,
      [&] {
        onFinished();
      },
      Qt::QueuedConnection);
}

LootDialog::~LootDialog() = default;

void LootDialog::setText(const QString& s)
{
  ui->progressText->setText(s);
}

void LootDialog::setProgress(lootcli::Progress p)
{
  // don't overwrite the "stopping loot" message even if lootcli generates a new
  // progress message
  if (!m_cancelling) {
    setText(progressToString(p));
  }

  if (p == lootcli::Progress::Done) {
    ui->progressBar->setRange(0, 1);
    ui->progressBar->setValue(1);
  }
}

void LootDialog::addOutput(const QString& s)
{
  if (m_core.settings().diagnostics().lootLogLevel() > lootcli::LogLevels::Debug) {
    return;
  }

  const auto lines = s.split(QRegularExpression("[\\r\\n]"), Qt::SkipEmptyParts);

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
    log::debug("loot dialog: cancelling");
    m_loot.cancel();

    setText(tr("Stopping LOOT..."));
    addLineOutput("stopping loot");

    ui->buttons->setEnabled(false);
    m_cancelling = true;
  }
}

void LootDialog::openReport()
{
  const auto path = m_loot.outPath();
  shell::Open(path);
}

int LootDialog::exec()
{
  auto& s = m_core.settings();

  GeometrySaver gs(s, this);
  s.geometry().restoreState(&m_expander);

  const auto r = QDialog::exec();

  s.geometry().saveState(&m_expander);

  return r;
}

void LootDialog::accept()
{
  // no-op
}

void LootDialog::reject()
{
  if (m_finished) {
    log::debug("loot dialog reject: loot finished, closing");
    QDialog::reject();
  } else {
    log::debug("loot dialog reject: not finished, cancelling");
    cancel();
  }
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

  m_expander.set(ui->details, ui->detailsPanel);
  ui->openJsonReport->setEnabled(false);
  connect(ui->openJsonReport, &QPushButton::clicked, [&] {
    openReport();
  });

  ui->buttons->setStandardButtons(QDialogButtonBox::Cancel);

  m_report.setText(tr("Running LOOT..."));

  resize(650, 450);
  setSizeGripEnabled(true);
}

void LootDialog::closeEvent(QCloseEvent* e)
{
  if (m_finished) {
    log::debug("loot dialog close event: finished, closing");
    QDialog::closeEvent(e);
  } else {
    log::debug("loot dialog close event: not finished, cancelling");
    cancel();
    e->ignore();
  }
}

void LootDialog::addLineOutput(const QString& line)
{
  ui->output->appendPlainText(line);
}

void LootDialog::onFinished()
{
  log::debug("loot dialog: loot is finished");

  m_finished = true;

  if (m_cancelling) {
    log::debug("loot dialog: was cancelling, closing");
    close();
  } else {
    log::debug("loot dialog: showing report");

    showReport();

    ui->openJsonReport->setEnabled(true);
    ui->buttons->setStandardButtons(QDialogButtonBox::Close);

    // if loot failed, the Done progress won't be received; this makes sure
    // the progress bar is stopped
    setProgress(lootcli::Progress::Done);
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

void LootDialog::showReport()
{
  const auto& lootReport = m_loot.report();

  if (m_loot.result()) {
    m_core.pluginList()->clearAdditionalInformation();
    for (auto&& p : lootReport.plugins) {
      m_core.pluginList()->addLootReport(p.name, p);
    }
  }

  m_report.setText(lootReport.toMarkdown());
}

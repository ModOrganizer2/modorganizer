#include "statusbar.h"
#include "instancemanager.h"
#include "nexusinterface.h"
#include "organizercore.h"
#include "settings.h"
#include "ui_mainwindow.h"

StatusBar::StatusBar(QWidget* parent)
    : QStatusBar(parent), ui(nullptr), m_normal(new QLabel),
      m_progress(new QProgressBar), m_progressSpacer1(new QWidget),
      m_progressSpacer2(new QWidget), m_notifications(nullptr), m_update(nullptr),
      m_api(new QLabel)
{}

void StatusBar::setup(Ui::MainWindow* mainWindowUI, const Settings& settings)
{
  ui              = mainWindowUI;
  m_notifications = new StatusBarAction(ui->actionNotifications);
  m_update        = new StatusBarAction(ui->actionUpdate);

  addWidget(m_normal);

  m_progressSpacer1->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  addPermanentWidget(m_progressSpacer1, 0);
  addPermanentWidget(m_progress);

  m_progressSpacer2->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  addPermanentWidget(m_progressSpacer2, 0);

  addPermanentWidget(m_notifications);
  addPermanentWidget(m_update);
  addPermanentWidget(m_api);

  m_progress->setTextVisible(true);
  m_progress->setRange(0, 100);
  m_progress->setMaximumWidth(300);
  m_progress->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

  m_update->set(false);
  m_notifications->set(false);

  m_api->setObjectName("apistats");
  m_api->setToolTip(QObject::tr(
      "This tracks the number of queued Nexus API requests, as well as the "
      "remaining daily and hourly requests. The Nexus API limits you to a pool "
      "of requests per day and requests per hour. It is dynamically updated "
      "every time a request is completed. If you run out of requests, you will "
      "be unable to queue downloads, check updates, parse mod info, or even log "
      "in. Both pools must be consumed before this happens."));

  clearMessage();
  setProgress(-1);
  setAPI({}, {});

  checkSettings(settings);
}

void StatusBar::setProgress(int percent)
{
  bool visible = true;

  if (percent < 0 || percent >= 100) {
    clearMessage();
    visible = false;
  } else {
    showMessage(QObject::tr("Loading..."));
    m_progress->setValue(percent);
  }

  m_progress->setVisible(visible);
  m_progressSpacer1->setVisible(visible);
  m_progressSpacer2->setVisible(visible);
}

void StatusBar::setNotifications(bool hasNotifications)
{
  if (m_notifications) {
    m_notifications->set(hasNotifications);
  }
}

void StatusBar::setAPI(const APIStats& stats, const APIUserAccount& user)
{
  QString text;
  QString textColor;
  QString backgroundColor;

  if (user.type() == APIUserAccountTypes::None) {
    text            = "API: not logged in";
    textColor       = "";
    backgroundColor = "";
  } else {
    text = QString("API: Queued: %1 | Daily: %2 | Hourly: %3")
               .arg(stats.requestsQueued)
               .arg(user.limits().remainingDailyRequests)
               .arg(user.limits().remainingHourlyRequests);

    if (user.remainingRequests() > 500) {
      textColor       = "white";
      backgroundColor = "darkgreen";
    } else if (user.remainingRequests() > 200) {
      textColor       = "black";
      backgroundColor = "rgb(226, 192, 0)";  // yellow
    } else {
      textColor       = "white";
      backgroundColor = "darkred";
    }
  }

  m_api->setText(text);

  QString ss(R"(
    QLabel
    {
      padding-left: 0.1em;
      padding-right: 0.1em;
      padding-top: 0;
      padding-bottom: 0;)");

  if (!textColor.isEmpty()) {
    ss += QString("\ncolor: %1;").arg(textColor);
  }

  if (!backgroundColor.isEmpty()) {
    ss += QString("\nbackground-color: %2;").arg(backgroundColor);
  }

  ss += "\n}";

  m_api->setStyleSheet(ss);
  m_api->setAutoFillBackground(true);
}

void StatusBar::setUpdateAvailable(bool b)
{
  m_update->set(b);
}

void StatusBar::checkSettings(const Settings& settings)
{
  m_api->setVisible(!settings.interface().hideAPICounter());
}

void StatusBar::updateNormalMessage(OrganizerCore& core)
{
  QString game;

  if (core.managedGame()) {
    game = core.managedGame()->displayGameName();
  } else {
    game = tr("Unknown game");
  }

  QString instance = "?";
  if (auto i = InstanceManager::singleton().currentInstance())
    instance = i->displayName();

  QString profile = core.profileName();

  const auto s = QString("%1 - %2 - %3").arg(game).arg(instance).arg(profile);

  m_normal->setText(s);
}

void StatusBar::showEvent(QShowEvent*)
{
  visibilityChanged(true);
}

void StatusBar::hideEvent(QHideEvent*)
{
  visibilityChanged(false);
}

void StatusBar::visibilityChanged(bool visible)
{
  // the central widget typically has no bottom padding because the status bar
  // is more than enough, but when it's hidden, the bottom widget (currently
  // the log) touches the bottom border of the window, which looks ugly
  //
  // when hiding the statusbar, the central widget is given the same border
  // margin as it has on the top (which is typically 6, as it's the default from
  // the qt designer)

  auto m = ui->centralWidget->layout()->contentsMargins();

  if (visible) {
    m.setBottom(0);
  } else {
    m.setBottom(m.top());
  }

  ui->centralWidget->layout()->setContentsMargins(m);
}

StatusBarAction::StatusBarAction(QAction* action)
    : m_action(action), m_icon(new QLabel), m_text(new QLabel)
{
  setLayout(new QHBoxLayout);
  layout()->setContentsMargins(0, 0, 0, 0);
  layout()->addWidget(m_icon);
  layout()->addWidget(m_text);
}

void StatusBarAction::set(bool visible)
{
  if (visible) {
    m_icon->setPixmap(m_action->icon().pixmap(16, 16));
    m_text->setText(cleanupActionText(m_action->text()));
  }

  setVisible(visible);
}

void StatusBarAction::mouseDoubleClickEvent(QMouseEvent* e)
{
  if (m_action->isEnabled()) {
    m_action->trigger();
  }
}

QString StatusBarAction::cleanupActionText(const QString& original) const
{
  QString s = original;

  s.replace(QRegularExpression("\\&([^&])"), "\\1");  // &Item -> Item
  s.replace("&&", "&");                               // &&Item -> &Item

  if (s.endsWith("...")) {
    s = s.left(s.size() - 3);
  }

  return s;
}

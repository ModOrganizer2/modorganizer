#include "modinfodialognexus.h"
#include "bbcode.h"
#include "iplugingame.h"
#include "organizercore.h"
#include "settings.h"
#include "ui_modinfodialog.h"
#include <log.h>
#include <utility.h>
#include <versioninfo.h>

using namespace MOBase;

bool isValidModID(int id)
{
  return (id > 0);
}

NexusTab::NexusTab(ModInfoDialogTabContext cx)
    : ModInfoDialogTab(std::move(cx)), m_requestStarted(false), m_loading(false)
{
  ui->modID->setValidator(new QIntValidator(ui->modID));
  ui->endorse->setVisible(core().settings().nexus().endorsementIntegration());
  ui->track->setVisible(core().settings().nexus().trackedIntegration());

  connect(ui->modID, &QLineEdit::editingFinished, [&] {
    onModIDChanged();
  });
  connect(ui->sourceGame,
          static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), [&] {
            onSourceGameChanged();
          });
  connect(ui->version, &QLineEdit::editingFinished, [&] {
    onVersionChanged();
  });
  connect(ui->category, &QLineEdit::editingFinished, [&] {
    onCategoryChanged();
  });

  connect(ui->refresh, &QPushButton::clicked, [&] {
    onRefreshBrowser();
  });
  connect(ui->visitNexus, &QPushButton::clicked, [&] {
    onVisitNexus();
  });
  connect(ui->endorse, &QPushButton::clicked, [&] {
    onEndorse();
  });
  connect(ui->track, &QPushButton::clicked, [&] {
    onTrack();
  });

  connect(ui->hasCustomURL, &QCheckBox::toggled, [&] {
    onCustomURLToggled();
  });
  connect(ui->customURL, &QLineEdit::editingFinished, [&] {
    onCustomURLChanged();
  });
  connect(ui->visitCustomURL, &QPushButton::clicked, [&] {
    onVisitCustomURL();
  });
}

NexusTab::~NexusTab()
{
  cleanup();
}

void NexusTab::cleanup()
{
  if (m_modConnection) {
    disconnect(m_modConnection);
    m_modConnection = {};
  }
}

void NexusTab::clear()
{
  ui->modID->clear();
  ui->sourceGame->clear();
  ui->version->clear();
  ui->category->clear();
  ui->browser->setPage(new NexusTabWebpage(ui->browser));
  ui->hasCustomURL->setChecked(false);
  ui->customURL->clear();
  setHasData(false);
}

void NexusTab::update()
{
  QScopedValueRollback loading(m_loading, true);

  clear();

  ui->modID->setText(QString("%1").arg(mod().nexusId()));

  QString gameName = mod().gameName();
  ui->sourceGame->addItem(core().managedGame()->gameName(),
                          core().managedGame()->gameShortName());

  if (core().managedGame()->validShortNames().size() == 0) {
    ui->sourceGame->setDisabled(true);
  } else {
    for (auto game : plugins().plugins<MOBase::IPluginGame>()) {
      for (QString gameName : core().managedGame()->validShortNames()) {
        if (game->gameShortName().compare(gameName, Qt::CaseInsensitive) == 0) {
          ui->sourceGame->addItem(game->gameName(), game->gameShortName());
          break;
        }
      }
    }
  }

  ui->sourceGame->setCurrentIndex(ui->sourceGame->findData(gameName));

  ui->category->setText(QString("%1").arg(mod().getNexusCategory()));

  auto* page = new NexusTabWebpage(ui->browser);
  ui->browser->setPage(page);

  connect(page, &NexusTabWebpage::linkClicked, [&](const QUrl& url) {
    shell::Open(url);
  });

  ui->endorse->setEnabled((mod().endorsedState() == EndorsedState::ENDORSED_FALSE) ||
                          (mod().endorsedState() == EndorsedState::ENDORSED_NEVER));

  setHasData(mod().nexusId() >= 0);
}

void NexusTab::firstActivation()
{
  updateWebpage();
}

void NexusTab::setMod(ModInfoPtr mod, MOShared::FilesOrigin* origin)
{
  cleanup();

  ModInfoDialogTab::setMod(mod, origin);

  m_modConnection = connect(mod.data(), &ModInfo::modDetailsUpdated, [&] {
    onModChanged();
  });
}

bool NexusTab::usesOriginFiles() const
{
  return false;
}

void NexusTab::updateVersionColor()
{
  if (mod().version() != mod().newestVersion()) {
    ui->version->setStyleSheet("color: red");
    ui->version->setToolTip(
        tr("Current Version: %1").arg(mod().newestVersion().canonicalString()));
  } else {
    ui->version->setStyleSheet("color: green");
    ui->version->setToolTip(tr("No update available"));
  }
}

void NexusTab::updateWebpage()
{
  const int modID = mod().nexusId();

  if (isValidModID(modID)) {
    const QString nexusLink =
        NexusInterface::instance().getModURL(modID, mod().gameName());

    ui->visitNexus->setToolTip(nexusLink);
    refreshData(modID);
  } else {
    onModChanged();
  }

  ui->version->setText(mod().version().displayString());
  ui->hasCustomURL->setChecked(mod().hasCustomURL());
  ui->customURL->setText(mod().url());
  ui->customURL->setEnabled(mod().hasCustomURL());
  ui->visitCustomURL->setEnabled(mod().hasCustomURL());
  ui->visitCustomURL->setToolTip(mod().parseCustomURL().toString());

  updateTracking();
}

void NexusTab::updateTracking()
{
  if (mod().trackedState() == TrackedState::TRACKED_TRUE) {
    ui->track->setChecked(true);
    ui->track->setText(tr("Tracked"));
  } else {
    ui->track->setChecked(false);
    ui->track->setText(tr("Untracked"));
  }
}

void NexusTab::refreshData(int modID)
{
  if (tryRefreshData(modID)) {
    m_requestStarted = true;
  } else {
    onModChanged();
  }
}

bool NexusTab::tryRefreshData(int modID)
{
  if (isValidModID(modID) && !m_requestStarted) {
    if (mod().updateNXMInfo()) {
      ui->browser->setHtml("");
      return true;
    }
  }

  return false;
}

void NexusTab::onModChanged()
{
  m_requestStarted = false;

  const QString nexusDescription = mod().getNexusDescription();

  QString descriptionAsHTML = R"(
<html>
  <head>
    <style class="nexus-description">
    body
    {
      font-family: sans-serif;
      font-size: 14px;
      background: #404040;
      color: #f1f1f1;
      max-width: 1060px;
      margin-left: auto;
      margin-right: auto;
      padding-right: 7px;
      padding-left: 7px;
      padding-top: 20px;
      padding-bottom: 20px;
    }

    img {
      max-width: 100%;
    }

    figure.quote {
      position: relative;
      padding: 24px;
      margin: 10px 20px 10px 10px;
      color: #e1e1e1;
      line-height: 1.5;
      font-style: italic;
      border-left: 6px solid #57a5cc;
      border-left-color: rgb(87, 165, 204);
      background: #383838 url(data:image/svg+xml;base64,PHN2ZyBjbGFzcz0iaWNvbi1xdW90ZSIgdmVyc2lvbj0iMS4xIiB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHN0eWxlPSJmaWxsOnJnYig2OSwgNjksIDcwKTtoZWlnaHQ6MjlweDtsZWZ0OjE1cHg7cG9zaXRpb246YWJzb2x1dGU7dG9wOjE1cHg7d2lkdGg6MzhweDsiPjxwYXRoIGNsYXNzPSJwYXRoMSIgZD0iTTAgMjAuNjc0YzAgNy4yMjUgNC42NjggMTEuMzM3IDkuODkyIDExLjMzNyA0LjgyNC0wLjA2MiA4LjcxOS0zLjk1NiA4Ljc4MS04Ljc3NSAwLTQuNzg1LTMuMzM0LTguMDA5LTcuNTU4LTguMDA5LTAuMDc4LTAuMDA0LTAuMTctMC4wMDYtMC4yNjItMC4wMDYtMC43MDMgMC0xLjM3NyAwLjEyNC0yLjAwMSAwLjM1MiAxLjA0MS00LjAxNCA1LjE1My04LjY4MyA4LjcxLTEwLjU3MmwtNi4xMTMtNS4wMDJjLTYuODkxIDQuODkxLTExLjQ0OCAxMi4zMzgtMTEuNDQ4IDIwLjY3NHpNMjIuNjc1IDIwLjY3NGMwIDcuMjI1IDQuNjY4IDExLjMzNyA5Ljg5MiAxMS4zMzcgNC44LTAuMDU2IDguNjctMy45NjEgOC42Ny04Ljc2OSAwLTAuMDA0IDAtMC4wMDggMC0wLjAxMiAwLTQuNzc5LTMuMjIzLTguMDAyLTcuNDQ3LTguMDAyLTAuMDk1LTAuMDA2LTAuMjA2LTAuMDA5LTAuMzE4LTAuMDA5LTAuNjg0IDAtMS4zMzkgMC4xMjYtMS45NDMgMC4zNTUgMC45MjctNC4wMTQgNS4xNS04LjY4MiA4LjcwNy0xMC41NzJsLTYuMTI0LTUuMDAyYy02Ljg5MSA0Ljg5MS0xMS40MzcgMTIuMzM4LTExLjQzNyAyMC42NzR6IiBzdHlsZT0iZmlsbDpyZ2IoNjksIDY5LCA3MCk7aGVpZ2h0OmF1dG87d2lkdGg6YXV0bzsiLz48L3N2Zz4=) no-repeat;
    }

    figure.quote blockquote {
      position: relative;
      margin: 0;
      padding: 0;
    }

    div.spoiler_content {
      background: #262626;
      border: 1px dashed #3b3b3b;
      padding: 5px;
      margin: 5px;
    }

    div.bbc_spoiler_show{
      border: 1px solid black;
      background-color: #454545;
      font-size: 11px;
      padding: 3px;
      color: #E6E6E6;
      border-radius: 3px;
      display: inline-block;
      cursor: pointer;
    }

    details summary::marker {
      display:none;
    }

    summary:focus {
      outline: 0;
    }

    a
    {
      /*should avoid overflow with long links forcing wordwrap regardless of spaces*/
      overflow-wrap: break-word;
      word-wrap: break-word;

      color: #8197ec;
      text-decoration: none;
    }
    </style>
  </head>
  <body>%1</body>
</html>)";

  if (nexusDescription.isEmpty()) {
    descriptionAsHTML = descriptionAsHTML.arg(tr(R"(
      <div style="text-align: center;">
      <p>This mod does not have a valid Nexus ID. You can add a custom web
      page for it in the "Custom URL" box below.</p>
      </div>)"));
  } else {
    descriptionAsHTML = descriptionAsHTML.arg(BBCode::convertToHTML(nexusDescription));
  }

  ui->browser->page()->setHtml(descriptionAsHTML);
  updateVersionColor();
  updateTracking();
}

void NexusTab::onModIDChanged()
{
  if (m_loading) {
    return;
  }

  const int oldID = mod().nexusId();
  const int newID = ui->modID->text().toInt();

  if (oldID != newID) {
    mod().setNexusID(newID);
    mod().setLastNexusQuery(QDateTime::fromSecsSinceEpoch(0));

    ui->browser->page()->setHtml("");

    if (isValidModID(newID)) {
      refreshData(newID);
    }
  }
}

void NexusTab::onSourceGameChanged()
{
  if (m_loading) {
    return;
  }

  for (auto game : plugins().plugins<MOBase::IPluginGame>()) {
    if (game->gameName() == ui->sourceGame->currentText()) {
      mod().setGameName(game->gameShortName());
      mod().setLastNexusQuery(QDateTime::fromSecsSinceEpoch(0));
      refreshData(mod().nexusId());
      return;
    }
  }
}

void NexusTab::onVersionChanged()
{
  if (m_loading) {
    return;
  }

  MOBase::VersionInfo version(ui->version->text());
  mod().setVersion(version);
  updateVersionColor();
}

void NexusTab::onCategoryChanged()
{
  if (m_loading) {
    return;
  }

  int category = ui->category->text().toInt();
  mod().setNexusCategory(category);
}

void NexusTab::onRefreshBrowser()
{
  const auto modID = mod().nexusId();

  if (isValidModID(modID)) {
    mod().setLastNexusQuery(QDateTime::fromSecsSinceEpoch(0));
    updateWebpage();
  } else {
    log::info("Mod has no valid Nexus ID, info can't be updated.");
  }
}

void NexusTab::onVisitNexus()
{
  const int modID = mod().nexusId();

  if (isValidModID(modID)) {
    const QString nexusLink =
        NexusInterface::instance().getModURL(modID, mod().gameName());

    shell::Open(QUrl(nexusLink));
  }
}

void NexusTab::onEndorse()
{
  // use modPtr() instead of mod() or this because the callback may be
  // executed after the dialog is closed
  core().loggedInAction(parentWidget(), [m = modPtr()] {
    m->endorse(true);
  });
}

void NexusTab::onTrack()
{
  // use modPtr() instead of mod() or this because the callback may be
  // executed after the dialog is closed
  core().loggedInAction(parentWidget(), [m = modPtr()] {
    if (m->trackedState() == TrackedState::TRACKED_TRUE) {
      m->track(false);
    } else {
      m->track(true);
    }
  });
}

void NexusTab::onCustomURLToggled()
{
  if (m_loading) {
    return;
  }

  mod().setHasCustomURL(ui->hasCustomURL->isChecked());
  ui->customURL->setEnabled(mod().hasCustomURL());
  ui->visitCustomURL->setEnabled(mod().hasCustomURL());
}

void NexusTab::onCustomURLChanged()
{
  if (m_loading) {
    return;
  }

  mod().setCustomURL(ui->customURL->text());
  ui->visitCustomURL->setToolTip(mod().parseCustomURL().toString());
}

void NexusTab::onVisitCustomURL()
{
  const QUrl url = mod().parseCustomURL();
  if (url.isValid()) {
    shell::Open(url);
  }
}

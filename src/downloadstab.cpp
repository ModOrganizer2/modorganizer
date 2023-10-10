#include "downloadstab.h"
#include "downloadlist.h"
#include "downloadlistview.h"
#include "organizercore.h"
#include "ui_mainwindow.h"

DownloadsTab::DownloadsTab(OrganizerCore& core, Ui::MainWindow* mwui)
    : m_core(core), ui{mwui->btnRefreshDownloads, mwui->downloadView,
                       mwui->showHiddenBox, mwui->downloadFilterEdit}
{
  DownloadList* sourceModel = new DownloadList(m_core, ui.list);

  ui.list->setModel(sourceModel);
  ui.list->setManager(m_core.downloadManager());
  ui.list->setSourceModel(sourceModel);
  ui.list->setItemDelegate(
      new DownloadProgressDelegate(m_core.downloadManager(), ui.list, sourceModel));

  update();

  m_filter.setEdit(ui.filter);
  m_filter.setList(ui.list);
  m_filter.setSortPredicate([sourceModel](auto&& left, auto&& right) {
    return sourceModel->lessThanPredicate(left, right);
  });

  connect(ui.refresh, &QPushButton::clicked, [&] {
    refresh();
  });
  connect(ui.list, SIGNAL(installDownload(QUuid)), &m_core,
          SLOT(installDownload(QUuid)));
  connect(ui.list, SIGNAL(queryInfo(QUuid)), m_core.downloadManager(),
          SLOT(queryInfo(QUuid)));
  connect(ui.list, SIGNAL(queryInfoMd5(QUuid)), m_core.downloadManager(),
          SLOT(queryInfoMd5(QUuid)));
  connect(ui.list, SIGNAL(visitOnNexus(QUuid)), m_core.downloadManager(),
          SLOT(visitOnNexus(QUuid)));
  connect(ui.list, SIGNAL(openFile(QUuid)), m_core.downloadManager(),
          SLOT(openFile(QUuid)));
  connect(ui.list, SIGNAL(openMetaFile(QUuid)), m_core.downloadManager(),
          SLOT(openMetaFile(QUuid)));
  connect(ui.list, SIGNAL(openInDownloadsFolder(QUuid)), m_core.downloadManager(),
          SLOT(openInDownloadsFolder(QUuid)));
  connect(ui.list, SIGNAL(removeDownload(QUuid, bool, int)), m_core.downloadManager(),
          SLOT(removeDownload(QUuid, bool, int)));
  connect(ui.list, SIGNAL(restoreDownload(QUuid)), m_core.downloadManager(),
          SLOT(restoreDownload(QUuid)));
  connect(ui.list, SIGNAL(cancelDownload(QUuid)), m_core.downloadManager(),
          SLOT(cancelDownload(QUuid)));
  connect(ui.list, SIGNAL(pauseDownload(QUuid)), m_core.downloadManager(),
          SLOT(pauseDownload(QUuid)));
  connect(ui.list, &DownloadListView::resumeDownload, [&](QUuid i) {
    resumeDownload(i);
  });
}

void DownloadsTab::update()
{
  // this means downloadTab initialization hasn't happened yet
  if (ui.list->model() == nullptr) {
    return;
  }

  // set the view attribute and default row sizes
  if (m_core.settings().interface().compactDownloads()) {
    ui.list->setProperty("downloadView", "compact");
    ui.list->setStyleSheet("DownloadListView::item { padding: 4px 2px; }");
  } else {
    ui.list->setProperty("downloadView", "standard");
    ui.list->setStyleSheet("DownloadListView::item { padding: 16px 4px; }");
  }

  ui.list->style()->unpolish(ui.list);
  ui.list->style()->polish(ui.list);
  qobject_cast<DownloadListHeader*>(ui.list->header())->customResizeSections();

  m_core.downloadManager()->initializeList();
}

void DownloadsTab::refresh()
{
  m_core.downloadManager()->refreshList();
}

void DownloadsTab::resumeDownload(QUuid moId)
{
  m_core.loggedInAction(ui.list, [this, moId] {
    m_core.downloadManager()->resumeDownload(moId);
  });
}

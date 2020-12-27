#include "downloadstab.h"
#include "downloadlist.h"
#include "downloadlistsortproxy.h"
#include "downloadlistwidget.h"
#include "organizercore.h"
#include "ui_mainwindow.h"

DownloadsTab::DownloadsTab(OrganizerCore& core, Ui::MainWindow* mwui)
  : m_core(core), ui{
      mwui->btnRefreshDownloads, mwui->downloadView, mwui->showHiddenBox,
      mwui->downloadFilterEdit}
{
  DownloadList *sourceModel = new DownloadList(
    m_core.downloadManager(), ui.list);

  DownloadListSortProxy *sortProxy = new DownloadListSortProxy(
    m_core.downloadManager(), ui.list);

  sortProxy->setSourceModel(sourceModel);
  connect(ui.filter, SIGNAL(textChanged(QString)), sortProxy, SLOT(updateFilter(QString)));
  connect(ui.filter, SIGNAL(textChanged(QString)), this, SLOT(downloadFilterChanged(QString)));

  ui.list->setSourceModel(sourceModel);
  ui.list->setModel(sortProxy);
  ui.list->setManager(m_core.downloadManager());
  ui.list->setItemDelegate(new DownloadProgressDelegate(
    m_core.downloadManager(), sortProxy, ui.list));
  update();

  connect(ui.refresh, &QPushButton::clicked, [&]{ refresh(); });
  connect(ui.list, SIGNAL(installDownload(int)), &m_core, SLOT(installDownload(int)));
  connect(ui.list, SIGNAL(queryInfo(int)), m_core.downloadManager(), SLOT(queryInfo(int)));
  connect(ui.list, SIGNAL(queryInfoMd5(int)), m_core.downloadManager(), SLOT(queryInfoMd5(int)));
  connect(ui.list, SIGNAL(visitOnNexus(int)), m_core.downloadManager(), SLOT(visitOnNexus(int)));
  connect(ui.list, SIGNAL(openFile(int)), m_core.downloadManager(), SLOT(openFile(int)));
  connect(ui.list, SIGNAL(openMetaFile(int)), m_core.downloadManager(), SLOT(openMetaFile(int)));
  connect(ui.list, SIGNAL(openInDownloadsFolder(int)), m_core.downloadManager(), SLOT(openInDownloadsFolder(int)));
  connect(ui.list, SIGNAL(removeDownload(int, bool)), m_core.downloadManager(), SLOT(removeDownload(int, bool)));
  connect(ui.list, SIGNAL(restoreDownload(int)), m_core.downloadManager(), SLOT(restoreDownload(int)));
  connect(ui.list, SIGNAL(cancelDownload(int)), m_core.downloadManager(), SLOT(cancelDownload(int)));
  connect(ui.list, SIGNAL(pauseDownload(int)), m_core.downloadManager(), SLOT(pauseDownload(int)));
  connect(ui.list, SIGNAL(resumeDownload(int)), this, SLOT(resumeDownload(int)));
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
    ui.list->setStyleSheet("DownloadListWidget::item { padding: 4px 2px; }");
  } else {
    ui.list->setProperty("downloadView", "standard");
    ui.list->setStyleSheet("DownloadListWidget::item { padding: 16px 4px; }");
  }

  ui.list->setMetaDisplay(m_core.settings().interface().metaDownloads());
  ui.list->style()->unpolish(ui.list);
  ui.list->style()->polish(ui.list);
  qobject_cast<DownloadListHeader*>(ui.list->header())->customResizeSections();

  m_core.downloadManager()->refreshList();
}

void DownloadsTab::refresh()
{
  m_core.downloadManager()->refreshList();
}

void DownloadsTab::resumeDownload(int downloadIndex)
{
  m_core.loggedInAction(ui.list, [this, downloadIndex] {
    m_core.downloadManager()->resumeDownload(downloadIndex);
  });
}

void DownloadsTab::downloadFilterChanged(const QString &filter)
{
  if (!filter.isEmpty()) {
    ui.list->setStyleSheet("QTreeView { border: 2px ridge #f00; }");
  } else {
    ui.list->setStyleSheet("");
  }
}

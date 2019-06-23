#ifndef MODINFODIALOGNEXUS_H
#define MODINFODIALOGNEXUS_H

#include "modinfodialogtab.h"

class NexusTabWebpage : public QWebEnginePage
{
  Q_OBJECT

public:
  NexusTabWebpage(QObject* parent = 0)
    : QWebEnginePage(parent)
  {
  }

  bool acceptNavigationRequest(
    const QUrl & url, QWebEnginePage::NavigationType type, bool) override
  {
    if (type == QWebEnginePage::NavigationTypeLinkClicked)
    {
      emit linkClicked(url);
      return false;
    }

    return true;
  }

signals:
  void linkClicked(const QUrl&);
};


class NexusTab : public ModInfoDialogTab
{
public:
  NexusTab(
    OrganizerCore& oc, PluginContainer& plugin,
    QWidget* parent, Ui::ModInfoDialog* ui, int index);

  ~NexusTab();

  void clear() override;
  void update() override;
  void setMod(ModInfo::Ptr mod, MOShared::FilesOrigin* origin) override;

private:
  QMetaObject::Connection m_modConnection;
  bool m_requestStarted;

  void cleanup();
  void updateVersionColor();
  void updateWebpage();

  void refreshData(int modID);
  bool tryRefreshData(int modID);

  void onModChanged();
  void onOpenLink();
  void onModIDChanged();
  void onSourceGameChanged();
  void onVersionChanged();
  void onRefreshBrowser();
  void onEndorse();
  void onUrlChanged();
};

#endif // MODINFODIALOGNEXUS_H

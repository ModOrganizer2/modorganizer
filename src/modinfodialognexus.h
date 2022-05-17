#ifndef MODINFODIALOGNEXUS_H
#define MODINFODIALOGNEXUS_H

#include "modinfodialogtab.h"

class NexusTabWebpage : public QWebEnginePage
{
  Q_OBJECT

public:
  NexusTabWebpage(QObject* parent = 0) : QWebEnginePage(parent) {}

  bool acceptNavigationRequest(const QUrl& url, QWebEnginePage::NavigationType type,
                               bool) override
  {
    if (type == QWebEnginePage::NavigationTypeLinkClicked) {
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
  Q_OBJECT;

public:
  NexusTab(ModInfoDialogTabContext cx);

  ~NexusTab();

  void clear() override;
  void update() override;
  void firstActivation() override;
  void setMod(ModInfoPtr mod, MOShared::FilesOrigin* origin) override;
  bool usesOriginFiles() const override;

private:
  QMetaObject::Connection m_modConnection;
  bool m_requestStarted;
  bool m_loading;

  void cleanup();
  void updateVersionColor();
  void updateWebpage();
  void updateTracking();

  void refreshData(int modID);
  bool tryRefreshData(int modID);
  void onModChanged();

  void onModIDChanged();
  void onSourceGameChanged();
  void onVersionChanged();

  void onRefreshBrowser();
  void onVisitNexus();
  void onEndorse();
  void onTrack();

  void onCustomURLToggled();
  void onCustomURLChanged();
  void onVisitCustomURL();
};

#endif  // MODINFODIALOGNEXUS_H

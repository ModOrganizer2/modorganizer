#include <QWebEnginePage>

#ifndef DESCRIPTIONPAGE_H
#define DESCRIPTIONPAGE_H

class DescriptionPage : public QWebEnginePage {
    Q_OBJECT

public:
    DescriptionPage(QObject* parent = 0) : QWebEnginePage(parent) {}

    bool acceptNavigationRequest(const QUrl& url, QWebEnginePage::NavigationType type, bool isMainFrame) {
        if (type == QWebEnginePage::NavigationTypeLinkClicked) {
            emit linkClicked(url);
            return false;
        }
        return true;
    }

signals:
    void linkClicked(const QUrl&);
};

#endif // DESCRIPTIONPAGE_H

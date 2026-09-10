#pragma once

#include "../core/result.h"

#include <QByteArray>
#include <QHash>
#include <QNetworkAccessManager>
#include <QUrl>

namespace remustwo {

struct HttpResponse {
    int statusCode = 0;
    QByteArray body;
    QString error;
};

class HttpClient {
public:
    explicit HttpClient(QNetworkAccessManager *manager = nullptr);
    HttpResponse get(const QUrl &url, int timeoutMs = 5000,
        const QHash<QString, QString> &headers = {}) const;
    HttpResponse head(const QUrl &url, int timeoutMs = 5000) const;
    HttpResponse postJson(const QUrl &url, const QByteArray &jsonBody, int timeoutMs = 5000,
        const QHash<QString, QString> &headers = {}) const;

private:
    QNetworkAccessManager *m_manager;
    mutable QNetworkAccessManager m_ownedManager;
};

} // namespace remustwo

#include "http_client.h"

#include "../core/constants/api.h"

#include <QEventLoop>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

namespace remustwo {

namespace {

    void applyExtraHeaders(QNetworkRequest &request, const QHash<QString, QString> &headers) {
        for (auto it = headers.constBegin(); it != headers.constEnd(); ++it)
            request.setRawHeader(it.key().toUtf8(), it.value().toUtf8());
    }

} // namespace

HttpClient::HttpClient(QNetworkAccessManager *manager)
    : m_manager(manager ? manager : &m_ownedManager) { }

HttpResponse HttpClient::get(const QUrl &url, int timeoutMs, const QHash<QString, QString> &headers) const {
    HttpResponse response;
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, Constants::API::USER_AGENT);
    applyExtraHeaders(request, headers);
    QNetworkReply *reply = m_manager->get(request);

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    loop.exec();

    if (!timer.isActive()) {
        response.error = QStringLiteral("Request timed out");
        reply->abort();
        reply->deleteLater();
        return response;
    }

    response.statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() != QNetworkReply::NoError) {
        response.error = reply->errorString();
    } else {
        response.body = reply->readAll();
    }
    reply->deleteLater();
    return response;
}

HttpResponse HttpClient::head(const QUrl &url, int timeoutMs) const {
    HttpResponse response;
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, Constants::API::USER_AGENT);
    QNetworkReply *reply = m_manager->head(request);

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    loop.exec();

    if (!timer.isActive()) {
        response.error = QStringLiteral("Request timed out");
        reply->abort();
        reply->deleteLater();
        return response;
    }

    response.statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() != QNetworkReply::NoError)
        response.error = reply->errorString();
    reply->deleteLater();
    return response;
}

HttpResponse HttpClient::postJson(
    const QUrl &url, const QByteArray &jsonBody, int timeoutMs, const QHash<QString, QString> &headers) const {
    HttpResponse response;
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setHeader(QNetworkRequest::UserAgentHeader, Constants::API::USER_AGENT);
    applyExtraHeaders(request, headers);
    QNetworkReply *reply = m_manager->post(request, jsonBody);

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    loop.exec();

    if (!timer.isActive()) {
        response.error = QStringLiteral("Request timed out");
        reply->abort();
        reply->deleteLater();
        return response;
    }

    response.statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() != QNetworkReply::NoError) {
        response.error = reply->errorString();
    } else {
        response.body = reply->readAll();
    }
    reply->deleteLater();
    return response;
}

} // namespace remustwo

#include "xrpcclient.h"

#include <QJsonDocument>
#include <QNetworkReply>
#include <QUrlQuery>

#include "dpopkey.h"

XrpcClient::XrpcClient(QObject *parent)
    : QObject(parent)
    , m_service(QStringLiteral("https://bsky.social"))
{
}

QString XrpcClient::service() const
{
    return m_service;
}

void XrpcClient::setService(const QString &service)
{
    m_service = service;
}

void XrpcClient::setAccessJwt(const QString &jwt)
{
    m_accessJwt = jwt;
    m_dpopKey = nullptr;
}

void XrpcClient::setDpop(DpopKey *key, const QString &accessToken)
{
    m_dpopKey = key;
    m_accessJwt = accessToken;
}

void XrpcClient::setDpopNonce(const QString &nonce)
{
    m_dpopNonce = nonce;
}

void XrpcClient::clearAuth()
{
    m_accessJwt.clear();
    m_dpopKey = nullptr;
    m_dpopNonce.clear();
}

QNetworkRequest XrpcClient::makeRequest(const QUrl &url, const QString &method,
                                        const QString &jwt) const
{
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
    if (!jwt.isEmpty()) {
        if (m_dpopKey) {
            request.setRawHeader("Authorization", "DPoP " + jwt.toUtf8());
            request.setRawHeader("DPoP",
                                 m_dpopKey->proof(method, url.toString(), m_dpopNonce, jwt)
                                         .toLatin1());
        } else {
            request.setRawHeader("Authorization", "Bearer " + jwt.toUtf8());
        }
    }
    return request;
}

QNetworkReply *XrpcClient::get(const QString &nsid, const QVariantMap &params)
{
    QUrl url(m_service + QStringLiteral("/xrpc/") + nsid);
    QUrlQuery query;
    for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
        if (it.value().type() == QVariant::List) {
            // Array params repeat the key (e.g. getFeedGenerators feeds)
            const QVariantList list = it.value().toList();
            for (const QVariant &value : list)
                query.addQueryItem(it.key(), value.toString());
        } else {
            query.addQueryItem(it.key(), it.value().toString());
        }
    }
    url.setQuery(query);
    return m_nam.get(makeRequest(url, QStringLiteral("GET"), m_accessJwt));
}

QNetworkReply *XrpcClient::post(const QString &nsid, const QJsonObject &body,
                                const QString &jwtOverride)
{
    const QUrl url(m_service + QStringLiteral("/xrpc/") + nsid);
    QNetworkRequest request = makeRequest(url, QStringLiteral("POST"),
                                          jwtOverride.isEmpty() ? m_accessJwt : jwtOverride);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    const QByteArray payload = body.isEmpty() ? QByteArray()
                                              : QJsonDocument(body).toJson(QJsonDocument::Compact);
    return m_nam.post(request, payload);
}

QNetworkReply *XrpcClient::postBlob(const QString &nsid, const QByteArray &data,
                                    const QString &contentType)
{
    const QUrl url(m_service + QStringLiteral("/xrpc/") + nsid);
    QNetworkRequest request = makeRequest(url, QStringLiteral("POST"), m_accessJwt);
    request.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
    return m_nam.post(request, data);
}

QJsonObject XrpcClient::parseReply(QNetworkReply *reply, QString *errorOut)
{
    reply->deleteLater();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
    if (errorOut) {
        errorOut->clear();
        if (status >= 400 || reply->error() != QNetworkReply::NoError) {
            *errorOut = obj.value(QStringLiteral("error")).toString();
            if (errorOut->isEmpty())
                *errorOut = reply->errorString();
        }
    }
    return obj;
}

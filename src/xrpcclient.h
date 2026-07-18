#ifndef XRPCCLIENT_H
#define XRPCCLIENT_H

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QObject>
#include <QVariantMap>

class QNetworkReply;
class DpopKey;

// Thin wrapper around QNetworkAccessManager for AT Protocol XRPC calls.
//   Queries:    GET  https://<service>/xrpc/<nsid>?<params>
//   Procedures: POST https://<service>/xrpc/<nsid> with a JSON body
//
// Two auth modes:
//   Bearer -- app-password session JWTs (setAccessJwt)
//   DPoP   -- OAuth access token bound to a DPoP key (setDpop); every
//             request carries a signed proof, with the server-issued nonce
//             (setDpopNonce, from the DPoP-Nonce response header)
class XrpcClient : public QObject
{
    Q_OBJECT
public:
    explicit XrpcClient(QObject *parent = nullptr);

    QString service() const;
    void setService(const QString &service);

    void setAccessJwt(const QString &jwt);
    void setDpop(DpopKey *key, const QString &accessToken);
    void setDpopNonce(const QString &nonce);
    QString dpopNonce() const { return m_dpopNonce; }
    void clearAuth();

    QNetworkReply *get(const QString &nsid, const QVariantMap &params = QVariantMap());
    // jwtOverride is used for com.atproto.server.refreshSession, which is
    // authorized with the refresh token instead of the access token.
    QNetworkReply *post(const QString &nsid, const QJsonObject &body = QJsonObject(),
                        const QString &jwtOverride = QString());

    // Consumes (deleteLater()s) the reply. On failure errorOut receives the
    // XRPC error code (e.g. "ExpiredToken", "AuthenticationRequired") or the
    // network error string; empty on success.
    static QJsonObject parseReply(QNetworkReply *reply, QString *errorOut);

private:
    QNetworkRequest makeRequest(const QUrl &url, const QString &method,
                                const QString &jwt) const;

    QNetworkAccessManager m_nam;
    QString m_service;
    QString m_accessJwt;
    DpopKey *m_dpopKey = nullptr;
    QString m_dpopNonce;
};

#endif // XRPCCLIENT_H

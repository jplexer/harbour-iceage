#ifndef OAUTHFLOW_H
#define OAUTHFLOW_H

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>

#include <functional>

#include "dpopkey.h"

class QNetworkReply;
class QTcpServer;

// atproto OAuth (authorization code + PKCE + PAR + DPoP) against the PDS's
// authorization server. start() drives: discovery -> PAR -> system browser
// -> loopback redirect (http://127.0.0.1:<port>/callback) -> code exchange.
// refresh() runs the refresh_token grant with the same DPoP key.
class OAuthFlow : public QObject
{
    Q_OBJECT
public:
    static const QString clientId();
    static const QString scope();

    explicit OAuthFlow(QObject *parent = nullptr);

    // key must outlive the flow; a fresh key should be generated per login.
    // loginHint (the user's handle) pre-fills the server's sign-in form.
    void start(const QString &serviceUrl, DpopKey *key,
               const QString &loginHint = QString());
    void refresh(const QString &tokenEndpoint, const QString &refreshToken, DpopKey *key);
    void cancel();

signals:
    // issuer/tokenEndpoint are echoed back so the caller can persist them.
    // did may be empty on refresh responses.
    void tokensReceived(const QString &accessToken, const QString &refreshToken,
                        const QString &did, const QString &issuer,
                        const QString &tokenEndpoint);
    void failed(const QString &message);

private:
    void discoverAuthServer(const QString &serviceUrl);
    void fetchAuthServerMetadata();
    void pushAuthorizationRequest();
    void startListener();
    void onListenerConnection();
    void exchangeCode(const QString &code);
    void requestTokens(const QJsonObject &form);
    // POST a form with a DPoP proof, retrying once when the server demands a
    // nonce (error=use_dpop_nonce, nonce in the DPoP-Nonce header).
    void postWithDpop(const QString &url, const QJsonObject &form, bool retried,
                      const std::function<void(const QJsonObject &, const QString &,
                                               int)> &done);
    void fail(const QString &message);
    void stopListener();

    QNetworkAccessManager m_nam;
    QTcpServer *m_server = nullptr;
    DpopKey *m_key = nullptr;

    QString m_issuer;
    QString m_authEndpoint;
    QString m_tokenEndpoint;
    QString m_parEndpoint;
    QString m_redirectUri;
    QString m_loginHint;
    QString m_state;
    QString m_verifier;
    QString m_dpopNonce;
    bool m_active = false;
};

#endif // OAUTHFLOW_H

#ifndef SESSION_H
#define SESSION_H

#include <QList>
#include <QNetworkAccessManager>
#include <QObject>
#include <functional>

#include "dpopkey.h"
#include "oauthflow.h"
#include "secretsstore.h"
#include "xrpcclient.h"

typedef std::function<void(const QJsonObject &data, const QString &error)> XrpcCallback;

// Owns the login state and the XRPC client. Two ways in:
//   - login():      app password (com.atproto.server.createSession)
//   - loginOAuth(): atproto OAuth via the system browser (PKCE + PAR + DPoP)
// Sensitive material (refresh tokens, DPoP key) is persisted in Sailfish
// Secrets; non-sensitive state (handle, did, service, mode) in QSettings.
class Session : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool loggedIn READ loggedIn NOTIFY loggedInChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString handle READ handle NOTIFY handleChanged)
    Q_PROPERTY(QString did READ did NOTIFY didChanged)
    // PDS base URL, e.g. https://bsky.social or a self-hosted server. Reads
    // also go through the PDS, which proxies them to the AppView.
    Q_PROPERTY(QString service READ service WRITE setService NOTIFY serviceChanged)
public:
    explicit Session(QObject *parent = nullptr);

    bool loggedIn() const { return m_loggedIn; }
    bool busy() const { return m_busy; }
    QString handle() const { return m_handle; }
    QString did() const { return m_did; }
    QString service() const { return m_client.service(); }
    void setService(const QString &service);

    Q_INVOKABLE void login(const QString &identifier, const QString &appPassword);
    // Resolves handle -> DID -> PDS endpoint, then runs OAuth against that
    // server with the handle as login hint. The primary sign-in path.
    Q_INVOKABLE void loginWithHandle(const QString &handle);
    // OAuth against the currently configured service (advanced/manual path).
    Q_INVOKABLE void loginOAuth();
    // Abandons a sign-in in progress (any method) and clears busy.
    Q_INVOKABLE void cancelLogin();
    // Restores a previous session from stored credentials, if any.
    Q_INVOKABLE void resume();
    Q_INVOKABLE void logout();

    // Authorized XRPC calls; transparently handle DPoP nonces and refresh
    // expired access tokens (retrying once each).
    void getAuthorized(const QString &nsid, const QVariantMap &params, const XrpcCallback &cb);
    void postAuthorized(const QString &nsid, const QJsonObject &body, const XrpcCallback &cb);
    // com.atproto.repo.uploadBlob with the same nonce/refresh handling;
    // the callback data contains the "blob" object for record embeds.
    void uploadBlob(const QByteArray &data, const QString &contentType,
                    const XrpcCallback &cb);

    XrpcClient *client() { return &m_client; }

signals:
    void loggedInChanged();
    void busyChanged();
    void handleChanged();
    void didChanged();
    void serviceChanged();
    void loginError(const QString &message);

private:
    void resolvePds(const QString &did, const QString &handle);
    void startOAuth(const QString &loginHint);
    void doAuthorized(bool isPost, const QString &nsid, const QVariantMap &params,
                      const QJsonObject &body, const XrpcCallback &cb,
                      bool triedNonce, bool triedRefresh);
    void doUploadBlob(const QByteArray &data, const QString &contentType,
                      const XrpcCallback &cb, bool triedNonce, bool triedRefresh);
    void adoptSession(const QJsonObject &data);
    void adoptOAuthTokens(const QString &accessToken, const QString &refreshToken,
                          const QString &did, const QString &issuer,
                          const QString &tokenEndpoint);
    void onOAuthFailed(const QString &message);
    // Queues the callback; only one refresh runs at a time (refresh tokens
    // rotate, so parallel refreshes would invalidate each other).
    void refreshThen(const std::function<void(bool ok)> &then);
    void finishRefresh(bool ok);
    void fetchProfileIdentity();
    void setBusy(bool busy);
    void setLoggedIn(bool loggedIn);

    XrpcClient m_client;
    SecretsStore m_secrets;
    OAuthFlow m_oauth;
    QNetworkAccessManager m_nam; // identity resolution (unauthenticated)
    DpopKey m_dpopKey;
    QString m_authMode; // "app" or "oauth"
    QString m_handle;
    QString m_did;
    QString m_refreshJwt;        // app-password mode
    QString m_oauthRefreshToken; // oauth mode
    QString m_issuer;
    QString m_tokenEndpoint;
    QList<std::function<void(bool)> > m_refreshCallbacks;
    bool m_loggedIn = false;
    bool m_busy = false;
    int m_loginAttempt = 0; // bumped by cancelLogin; stale replies compare against it
};

#endif // SESSION_H

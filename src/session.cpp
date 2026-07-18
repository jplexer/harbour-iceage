#include "session.h"

#include <QDebug>
#include <QJsonArray>
#include <QNetworkReply>
#include <QSettings>
#include <QStandardPaths>
#include <QUrlQuery>

namespace {
const QString kHandleKey = QStringLiteral("session/handle");
const QString kDidKey = QStringLiteral("session/did");
const QString kRefreshJwtKey = QStringLiteral("session/refreshJwt"); // legacy, migrated
const QString kServiceKey = QStringLiteral("session/service");
const QString kModeKey = QStringLiteral("session/mode");
const QString kIssuerKey = QStringLiteral("session/issuer");
const QString kTokenEndpointKey = QStringLiteral("session/tokenEndpoint");
const QString kDefaultService = QStringLiteral("https://bsky.social");

const QString kRefreshJwtSecret = QStringLiteral("refreshJwt");
const QString kOAuthRefreshSecret = QStringLiteral("oauthRefreshToken");
const QString kDpopKeySecret = QStringLiteral("dpopKeyPem");

const QString kModeApp = QStringLiteral("app");
const QString kModeOAuth = QStringLiteral("oauth");

// Under Sailjail only ~/.config/<Org>/<App>/ is writable. QSettings' default
// path (~/.config/<Org>/<App>.conf) sits one level up and silently fails to
// persist, so point it at a file inside the granted directory instead.
QString settingsPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
            + QStringLiteral("/settings.conf");
}

bool needsRefresh(const QString &error)
{
    return error == QLatin1String("ExpiredToken")
            || error == QLatin1String("InvalidToken")
            || error == QLatin1String("invalid_token");
}
}

Session::Session(QObject *parent)
    : QObject(parent)
    , m_authMode(kModeApp)
{
    m_client.setService(QSettings(settingsPath(), QSettings::IniFormat).value(kServiceKey, kDefaultService).toString());

    connect(&m_oauth, &OAuthFlow::tokensReceived, this, &Session::adoptOAuthTokens);
    connect(&m_oauth, &OAuthFlow::failed, this, &Session::onOAuthFailed);
}

void Session::setService(const QString &service)
{
    QString normalized = service.trimmed();
    if (normalized.isEmpty())
        normalized = kDefaultService;
    if (!normalized.startsWith(QLatin1String("http")))
        normalized.prepend(QLatin1String("https://"));
    while (normalized.endsWith(QLatin1Char('/')))
        normalized.chop(1);
    if (normalized == m_client.service())
        return;
    m_client.setService(normalized);
    QSettings(settingsPath(), QSettings::IniFormat).setValue(kServiceKey, normalized);
    emit serviceChanged();
}

void Session::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged();
}

void Session::setLoggedIn(bool loggedIn)
{
    if (m_loggedIn == loggedIn)
        return;
    m_loggedIn = loggedIn;
    emit loggedInChanged();
}

// ---- app-password login ----------------------------------------------------

void Session::login(const QString &identifier, const QString &appPassword)
{
    if (m_busy)
        return;
    setBusy(true);
    QJsonObject body;
    body.insert(QStringLiteral("identifier"), identifier.trimmed());
    body.insert(QStringLiteral("password"), appPassword);
    QNetworkReply *reply = m_client.post(QStringLiteral("com.atproto.server.createSession"), body);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        QString error;
        const QJsonObject data = XrpcClient::parseReply(reply, &error);
        setBusy(false);
        if (!error.isEmpty()) {
            emit loginError(data.value(QStringLiteral("message")).toString(error));
            return;
        }
        adoptSession(data);
    });
}

void Session::adoptSession(const QJsonObject &data)
{
    m_authMode = kModeApp;
    m_handle = data.value(QStringLiteral("handle")).toString();
    m_did = data.value(QStringLiteral("did")).toString();
    m_refreshJwt = data.value(QStringLiteral("refreshJwt")).toString();
    m_client.setAccessJwt(data.value(QStringLiteral("accessJwt")).toString());

    QSettings settings(settingsPath(), QSettings::IniFormat);
    settings.setValue(kModeKey, kModeApp);
    settings.setValue(kHandleKey, m_handle);
    settings.setValue(kDidKey, m_did);
    m_secrets.store(kRefreshJwtSecret, m_refreshJwt.toUtf8());

    emit handleChanged();
    emit didChanged();
    setLoggedIn(true);
}

// ---- OAuth login -----------------------------------------------------------

void Session::loginWithHandle(const QString &handle)
{
    if (m_busy)
        return;
    QString normalized = handle.trimmed();
    if (normalized.startsWith(QLatin1Char('@')))
        normalized.remove(0, 1);
    if (normalized.isEmpty()) {
        emit loginError(QStringLiteral("Enter your handle"));
        return;
    }
    setBusy(true);

    // 1. handle -> DID via the public AppView (works for any federated handle)
    QUrl url(QStringLiteral(
            "https://public.api.bsky.app/xrpc/com.atproto.identity.resolveHandle"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("handle"), normalized);
    url.setQuery(query);
    QNetworkReply *reply = m_nam.get(QNetworkRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply, normalized]() {
        QString error;
        const QJsonObject data = XrpcClient::parseReply(reply, &error);
        const QString did = data.value(QStringLiteral("did")).toString();
        if (did.isEmpty()) {
            setBusy(false);
            emit loginError(QStringLiteral("Could not resolve handle %1").arg(normalized));
            return;
        }
        resolvePds(did, normalized);
    });
}

void Session::resolvePds(const QString &did, const QString &handle)
{
    // 2. DID -> DID document -> PDS service endpoint
    QUrl url;
    if (did.startsWith(QLatin1String("did:plc:"))) {
        url = QUrl(QStringLiteral("https://plc.directory/") + did);
    } else if (did.startsWith(QLatin1String("did:web:"))) {
        url = QUrl(QStringLiteral("https://") + did.mid(8)
                   + QStringLiteral("/.well-known/did.json"));
    } else {
        setBusy(false);
        emit loginError(QStringLiteral("Unsupported DID method: %1").arg(did));
        return;
    }

    QNetworkReply *reply = m_nam.get(QNetworkRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply, handle]() {
        QString error;
        const QJsonObject doc = XrpcClient::parseReply(reply, &error);
        QString pds;
        const QJsonArray services = doc.value(QStringLiteral("service")).toArray();
        for (const QJsonValue &value : services) {
            const QJsonObject service = value.toObject();
            if (service.value(QStringLiteral("type")).toString()
                    == QLatin1String("AtprotoPersonalDataServer")) {
                pds = service.value(QStringLiteral("serviceEndpoint")).toString();
                break;
            }
        }
        if (pds.isEmpty()) {
            setBusy(false);
            emit loginError(QStringLiteral("Could not find a PDS for this account"));
            return;
        }
        setService(pds);
        startOAuth(handle);
    });
}

void Session::loginOAuth()
{
    if (m_busy)
        return;
    setBusy(true);
    startOAuth(QString());
}

void Session::startOAuth(const QString &loginHint)
{
    if (!m_dpopKey.generate()) {
        setBusy(false);
        emit loginError(QStringLiteral("Could not generate DPoP key"));
        return;
    }
    m_authMode = kModeOAuth;
    m_oauth.start(m_client.service(), &m_dpopKey, loginHint);
}

void Session::adoptOAuthTokens(const QString &accessToken, const QString &refreshToken,
                               const QString &did, const QString &issuer,
                               const QString &tokenEndpoint)
{
    m_authMode = kModeOAuth;
    m_oauthRefreshToken = refreshToken;
    m_issuer = issuer;
    m_tokenEndpoint = tokenEndpoint;
    if (!did.isEmpty() && did != m_did) {
        m_did = did;
        emit didChanged();
    }
    m_client.setDpop(&m_dpopKey, accessToken);

    QSettings settings(settingsPath(), QSettings::IniFormat);
    settings.setValue(kModeKey, kModeOAuth);
    settings.setValue(kDidKey, m_did);
    settings.setValue(kIssuerKey, m_issuer);
    settings.setValue(kTokenEndpointKey, m_tokenEndpoint);
    m_secrets.store(kOAuthRefreshSecret, m_oauthRefreshToken.toUtf8());
    m_secrets.store(kDpopKeySecret, m_dpopKey.toPem());

    setLoggedIn(true);
    if (!m_refreshCallbacks.isEmpty()) {
        finishRefresh(true);
    } else {
        // Fresh browser login: resolve the handle for display
        fetchProfileIdentity();
    }
}

void Session::onOAuthFailed(const QString &message)
{
    qWarning() << "Session: OAuth flow failed:" << message;
    if (!m_refreshCallbacks.isEmpty()) {
        finishRefresh(false);
        return;
    }
    setBusy(false);
    emit loginError(message);
}

void Session::fetchProfileIdentity()
{
    getAuthorized(QStringLiteral("com.atproto.server.getSession"), QVariantMap(),
                  [this](const QJsonObject &data, const QString &error) {
        setBusy(false);
        if (error.isEmpty()) {
            m_handle = data.value(QStringLiteral("handle")).toString();
            QSettings(settingsPath(), QSettings::IniFormat).setValue(kHandleKey, m_handle);
            emit handleChanged();
        }
    });
}

// ---- resume / logout -------------------------------------------------------

void Session::resume()
{
    QSettings settings(settingsPath(), QSettings::IniFormat);
    m_authMode = settings.value(kModeKey, kModeApp).toString();
    m_handle = settings.value(kHandleKey).toString();
    m_did = settings.value(kDidKey).toString();
    emit handleChanged();
    emit didChanged();

    if (m_authMode == kModeOAuth) {
        m_issuer = settings.value(kIssuerKey).toString();
        m_tokenEndpoint = settings.value(kTokenEndpointKey).toString();
        m_oauthRefreshToken = QString::fromUtf8(m_secrets.load(kOAuthRefreshSecret));
        if (m_oauthRefreshToken.isEmpty() || m_tokenEndpoint.isEmpty()
                || !m_dpopKey.fromPem(m_secrets.load(kDpopKeySecret))) {
            qWarning() << "Session: OAuth resume failed --"
                       << "refreshToken empty:" << m_oauthRefreshToken.isEmpty()
                       << "tokenEndpoint empty:" << m_tokenEndpoint.isEmpty()
                       << "dpopKey valid:" << m_dpopKey.isValid();
            m_authMode = kModeApp;
            return;
        }
    } else {
        m_refreshJwt = QString::fromUtf8(m_secrets.load(kRefreshJwtSecret));
        if (m_refreshJwt.isEmpty()) {
            // Migrate a token stored by older builds in plain QSettings
            m_refreshJwt = settings.value(kRefreshJwtKey).toString();
            if (!m_refreshJwt.isEmpty())
                settings.remove(kRefreshJwtKey);
        }
        if (m_refreshJwt.isEmpty())
            return;
    }

    setBusy(true);
    refreshThen([this](bool ok) {
        setBusy(false);
        if (!ok)
            logout(); // stored credentials are stale or revoked
    });
}

void Session::logout()
{
    m_oauth.cancel();
    QSettings settings(settingsPath(), QSettings::IniFormat);
    settings.remove(QStringLiteral("session"));
    m_secrets.remove(kRefreshJwtSecret);
    m_secrets.remove(kOAuthRefreshSecret);
    m_secrets.remove(kDpopKeySecret);
    m_refreshJwt.clear();
    m_oauthRefreshToken.clear();
    m_issuer.clear();
    m_tokenEndpoint.clear();
    m_handle.clear();
    m_did.clear();
    m_authMode = kModeApp;
    m_client.clearAuth();
    emit handleChanged();
    emit didChanged();
    setLoggedIn(false);
}

// ---- token refresh ---------------------------------------------------------

void Session::refreshThen(const std::function<void(bool)> &then)
{
    m_refreshCallbacks.append(then);
    if (m_refreshCallbacks.size() > 1)
        return; // a refresh is already in flight; tokens rotate, don't race

    if (m_authMode == kModeOAuth) {
        m_oauth.refresh(m_tokenEndpoint, m_oauthRefreshToken, &m_dpopKey);
        // result arrives via adoptOAuthTokens / onOAuthFailed
        return;
    }

    QNetworkReply *reply = m_client.post(QStringLiteral("com.atproto.server.refreshSession"),
                                         QJsonObject(), m_refreshJwt);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        QString error;
        const QJsonObject data = XrpcClient::parseReply(reply, &error);
        if (!error.isEmpty()) {
            finishRefresh(false);
            return;
        }
        adoptSession(data);
        finishRefresh(true);
    });
}

void Session::finishRefresh(bool ok)
{
    const QList<std::function<void(bool)> > callbacks = m_refreshCallbacks;
    m_refreshCallbacks.clear();
    for (int i = 0; i < callbacks.count(); ++i)
        callbacks.at(i)(ok);
}

// ---- authorized calls ------------------------------------------------------

void Session::getAuthorized(const QString &nsid, const QVariantMap &params, const XrpcCallback &cb)
{
    doAuthorized(false, nsid, params, QJsonObject(), cb, false, false);
}

void Session::postAuthorized(const QString &nsid, const QJsonObject &body, const XrpcCallback &cb)
{
    doAuthorized(true, nsid, QVariantMap(), body, cb, false, false);
}

void Session::doAuthorized(bool isPost, const QString &nsid, const QVariantMap &params,
                           const QJsonObject &body, const XrpcCallback &cb,
                           bool triedNonce, bool triedRefresh)
{
    QNetworkReply *reply = isPost ? m_client.post(nsid, body) : m_client.get(nsid, params);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, isPost, nsid, params, body, cb, triedNonce, triedRefresh]() {
        // DPoP servers hand out a fresh nonce on most responses
        const QByteArray nonce = reply->rawHeader("DPoP-Nonce");
        if (!nonce.isEmpty())
            m_client.setDpopNonce(QString::fromLatin1(nonce));

        QString error;
        const QJsonObject data = XrpcClient::parseReply(reply, &error);

        if (error == QLatin1String("use_dpop_nonce") && !triedNonce) {
            doAuthorized(isPost, nsid, params, body, cb, true, triedRefresh);
            return;
        }
        if (needsRefresh(error) && !triedRefresh) {
            refreshThen([this, isPost, nsid, params, body, cb, triedNonce, error](bool ok) {
                if (!ok) {
                    logout();
                    cb(QJsonObject(), error);
                    return;
                }
                doAuthorized(isPost, nsid, params, body, cb, triedNonce, true);
            });
            return;
        }
        cb(data, error);
    });
}

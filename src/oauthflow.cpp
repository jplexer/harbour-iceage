#include "oauthflow.h"

#include <QCryptographicHash>
#include <QDesktopServices>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrl>
#include <QUrlQuery>

namespace {

QByteArray formEncode(const QJsonObject &form)
{
    QUrlQuery query;
    for (auto it = form.constBegin(); it != form.constEnd(); ++it)
        query.addQueryItem(it.key(), it.value().toString());
    return query.toString(QUrl::FullyEncoded).toUtf8();
}

QJsonObject parseJson(const QByteArray &data)
{
    return QJsonDocument::fromJson(data).object();
}

} // namespace

const QString OAuthFlow::clientId()
{
    return QStringLiteral("https://funnyna.me/iceage/client-metadata.json");
}

const QString OAuthFlow::scope()
{
    return QStringLiteral("atproto transition:generic");
}

OAuthFlow::OAuthFlow(QObject *parent)
    : QObject(parent)
{
}

void OAuthFlow::fail(const QString &message)
{
    stopListener();
    m_active = false;
    emit failed(message);
}

void OAuthFlow::cancel()
{
    stopListener();
    m_active = false;
}

void OAuthFlow::stopListener()
{
    if (m_server) {
        m_server->close();
        m_server->deleteLater();
        m_server = nullptr;
    }
}

void OAuthFlow::start(const QString &serviceUrl, DpopKey *key, const QString &loginHint)
{
    if (m_active)
        return;
    m_active = true;
    m_key = key;
    m_loginHint = loginHint;
    m_dpopNonce.clear();
    discoverAuthServer(serviceUrl);
}

void OAuthFlow::discoverAuthServer(const QString &serviceUrl)
{
    const QUrl url(serviceUrl + QStringLiteral("/.well-known/oauth-protected-resource"));
    QNetworkReply *reply = m_nam.get(QNetworkRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        const QJsonObject data = parseJson(reply->readAll());
        const QJsonArray servers = data.value(QStringLiteral("authorization_servers")).toArray();
        if (servers.isEmpty()) {
            fail(QStringLiteral("Server does not advertise an OAuth authorization server"));
            return;
        }
        m_issuer = servers.first().toString();
        fetchAuthServerMetadata();
    });
}

void OAuthFlow::fetchAuthServerMetadata()
{
    const QUrl url(m_issuer + QStringLiteral("/.well-known/oauth-authorization-server"));
    QNetworkReply *reply = m_nam.get(QNetworkRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        const QJsonObject data = parseJson(reply->readAll());
        m_authEndpoint = data.value(QStringLiteral("authorization_endpoint")).toString();
        m_tokenEndpoint = data.value(QStringLiteral("token_endpoint")).toString();
        m_parEndpoint =
                data.value(QStringLiteral("pushed_authorization_request_endpoint")).toString();
        if (m_authEndpoint.isEmpty() || m_tokenEndpoint.isEmpty() || m_parEndpoint.isEmpty()) {
            fail(QStringLiteral("Incomplete authorization server metadata"));
            return;
        }
        startListener();
    });
}

void OAuthFlow::startListener()
{
    stopListener();
    m_server = new QTcpServer(this);
    if (!m_server->listen(QHostAddress::LocalHost, 0)) {
        fail(QStringLiteral("Could not start local callback listener"));
        return;
    }
    connect(m_server, &QTcpServer::newConnection, this, &OAuthFlow::onListenerConnection);
    m_redirectUri = QStringLiteral("http://127.0.0.1:%1/callback")
                            .arg(m_server->serverPort());
    pushAuthorizationRequest();
}

void OAuthFlow::pushAuthorizationRequest()
{
    m_state = QString::fromLatin1(DpopKey::base64Url(DpopKey::randomBytes(24)));
    m_verifier = QString::fromLatin1(DpopKey::base64Url(DpopKey::randomBytes(32)));
    const QString challenge = QString::fromLatin1(DpopKey::base64Url(
            QCryptographicHash::hash(m_verifier.toLatin1(), QCryptographicHash::Sha256)));

    QJsonObject form;
    form.insert(QStringLiteral("client_id"), clientId());
    form.insert(QStringLiteral("response_type"), QStringLiteral("code"));
    form.insert(QStringLiteral("redirect_uri"), m_redirectUri);
    form.insert(QStringLiteral("scope"), scope());
    form.insert(QStringLiteral("state"), m_state);
    form.insert(QStringLiteral("code_challenge"), challenge);
    form.insert(QStringLiteral("code_challenge_method"), QStringLiteral("S256"));
    if (!m_loginHint.isEmpty())
        form.insert(QStringLiteral("login_hint"), m_loginHint);

    postWithDpop(m_parEndpoint, form, false,
                 [this](const QJsonObject &data, const QString &error, int) {
        if (!error.isEmpty()) {
            fail(QStringLiteral("Authorization request failed: ") + error);
            return;
        }
        const QString requestUri = data.value(QStringLiteral("request_uri")).toString();
        if (requestUri.isEmpty()) {
            fail(QStringLiteral("Authorization server returned no request_uri"));
            return;
        }
        QUrl url(m_authEndpoint);
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("client_id"), clientId());
        query.addQueryItem(QStringLiteral("request_uri"), requestUri);
        url.setQuery(query);
        QDesktopServices::openUrl(url);
    });
}

void OAuthFlow::onListenerConnection()
{
    QTcpSocket *socket = m_server->nextPendingConnection();
    connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
        const QByteArray request = socket->readAll();
        const int lineEnd = request.indexOf("\r\n");
        const QList<QByteArray> parts = request.left(lineEnd).split(' ');
        if (parts.size() < 2)
            return;
        const QUrl url(QStringLiteral("http://127.0.0.1")
                       + QString::fromLatin1(parts.at(1)));

        const QByteArray body =
                "<html><body style=\"font-family:sans-serif;text-align:center;"
                "padding-top:4em\"><h2>Signed in</h2>"
                "<p>You can return to Ice Age now.</p></body></html>";
        socket->write("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n"
                      "Connection: close\r\nContent-Length: "
                      + QByteArray::number(body.size()) + "\r\n\r\n" + body);
        socket->flush();
        socket->disconnectFromHost();

        if (url.path() != QLatin1String("/callback"))
            return; // favicon etc.

        const QUrlQuery query(url);
        stopListener();
        if (query.hasQueryItem(QStringLiteral("error"))) {
            fail(query.queryItemValue(QStringLiteral("error_description"),
                                      QUrl::FullyDecoded));
            return;
        }
        if (query.queryItemValue(QStringLiteral("state")) != m_state) {
            fail(QStringLiteral("State mismatch in OAuth callback"));
            return;
        }
        exchangeCode(query.queryItemValue(QStringLiteral("code"), QUrl::FullyDecoded));
    });
}

void OAuthFlow::exchangeCode(const QString &code)
{
    QJsonObject form;
    form.insert(QStringLiteral("grant_type"), QStringLiteral("authorization_code"));
    form.insert(QStringLiteral("code"), code);
    form.insert(QStringLiteral("redirect_uri"), m_redirectUri);
    form.insert(QStringLiteral("client_id"), clientId());
    form.insert(QStringLiteral("code_verifier"), m_verifier);
    requestTokens(form);
}

void OAuthFlow::refresh(const QString &tokenEndpoint, const QString &refreshToken,
                        DpopKey *key)
{
    if (m_active)
        return;
    m_active = true;
    m_key = key;
    m_tokenEndpoint = tokenEndpoint;

    QJsonObject form;
    form.insert(QStringLiteral("grant_type"), QStringLiteral("refresh_token"));
    form.insert(QStringLiteral("refresh_token"), refreshToken);
    form.insert(QStringLiteral("client_id"), clientId());
    requestTokens(form);
}

void OAuthFlow::requestTokens(const QJsonObject &form)
{
    postWithDpop(m_tokenEndpoint, form, false,
                 [this](const QJsonObject &data, const QString &error, int) {
        if (!error.isEmpty()) {
            fail(QStringLiteral("Token request failed: ") + error);
            return;
        }
        const QString accessToken = data.value(QStringLiteral("access_token")).toString();
        const QString refreshToken = data.value(QStringLiteral("refresh_token")).toString();
        if (accessToken.isEmpty()) {
            fail(QStringLiteral("Token response contained no access token"));
            return;
        }
        m_active = false;
        emit tokensReceived(accessToken, refreshToken,
                            data.value(QStringLiteral("sub")).toString(),
                            m_issuer, m_tokenEndpoint);
    });
}

void OAuthFlow::postWithDpop(const QString &url, const QJsonObject &form, bool retried,
                             const std::function<void(const QJsonObject &, const QString &,
                                                      int)> &done)
{
    QNetworkRequest request((QUrl(url)));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));
    request.setRawHeader("DPoP",
                         m_key->proof(QStringLiteral("POST"), url, m_dpopNonce).toLatin1());

    QNetworkReply *reply = m_nam.post(request, formEncode(form));
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, url, form, retried, done]() {
        reply->deleteLater();
        const int status =
                reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray nonce = reply->rawHeader("DPoP-Nonce");
        if (!nonce.isEmpty())
            m_dpopNonce = QString::fromLatin1(nonce);

        const QJsonObject data = parseJson(reply->readAll());
        const QString errorCode = data.value(QStringLiteral("error")).toString();

        if (errorCode == QLatin1String("use_dpop_nonce") && !retried) {
            postWithDpop(url, form, true, done);
            return;
        }
        QString error;
        if (status < 200 || status >= 300) {
            error = data.value(QStringLiteral("error_description")).toString(errorCode);
            if (error.isEmpty())
                error = reply->errorString();
        }
        done(data, error, status);
    });
}

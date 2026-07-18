#ifndef DPOPKEY_H
#define DPOPKEY_H

#include <QByteArray>
#include <QJsonObject>
#include <QString>

typedef struct evp_pkey_st EVP_PKEY;

// An EC P-256 keypair used for DPoP (RFC 9449): every OAuth token request
// and every authorized XRPC call carries a proof JWT signed with this key,
// binding the tokens to this installation.
class DpopKey
{
public:
    DpopKey();
    ~DpopKey();
    DpopKey(const DpopKey &) = delete;
    DpopKey &operator=(const DpopKey &) = delete;

    bool isValid() const { return m_pkey != nullptr; }
    bool generate();
    void clear();

    QByteArray toPem() const;
    bool fromPem(const QByteArray &pem);

    QJsonObject publicJwk() const;

    // Compact ES256 JWT: b64url(header).b64url(payload).b64url(signature)
    QString signJwt(const QJsonObject &header, const QJsonObject &payload) const;

    // DPoP proof for one HTTP request. nonce and accessToken may be empty
    // (no ath claim is added without an access token).
    QString proof(const QString &method, const QString &url,
                  const QString &nonce = QString(),
                  const QString &accessToken = QString()) const;

    static QByteArray base64Url(const QByteArray &data);
    static QByteArray randomBytes(int count);

private:
    EVP_PKEY *m_pkey = nullptr;
};

#endif // DPOPKEY_H

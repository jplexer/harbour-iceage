#define OPENSSL_SUPPRESS_DEPRECATED
#include "dpopkey.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonDocument>
#include <QUrl>

#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/evp.h>
#include <openssl/obj_mac.h>
#include <openssl/pem.h>
#include <openssl/rand.h>

DpopKey::DpopKey()
{
}

DpopKey::~DpopKey()
{
    clear();
}

void DpopKey::clear()
{
    if (m_pkey) {
        EVP_PKEY_free(m_pkey);
        m_pkey = nullptr;
    }
}

bool DpopKey::generate()
{
    clear();
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, nullptr);
    if (!ctx)
        return false;
    bool ok = EVP_PKEY_keygen_init(ctx) > 0
            && EVP_PKEY_CTX_set_ec_paramgen_curve_nid(ctx, NID_X9_62_prime256v1) > 0
            && EVP_PKEY_keygen(ctx, &m_pkey) > 0;
    EVP_PKEY_CTX_free(ctx);
    return ok;
}

QByteArray DpopKey::toPem() const
{
    if (!m_pkey)
        return QByteArray();
    BIO *bio = BIO_new(BIO_s_mem());
    QByteArray pem;
    if (PEM_write_bio_PrivateKey(bio, m_pkey, nullptr, nullptr, 0, nullptr, nullptr) == 1) {
        char *data = nullptr;
        const long length = BIO_get_mem_data(bio, &data);
        pem = QByteArray(data, length);
    }
    BIO_free(bio);
    return pem;
}

bool DpopKey::fromPem(const QByteArray &pem)
{
    clear();
    if (pem.isEmpty())
        return false;
    BIO *bio = BIO_new_mem_buf(pem.constData(), pem.size());
    m_pkey = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    return m_pkey != nullptr;
}

QJsonObject DpopKey::publicJwk() const
{
    QJsonObject jwk;
    if (!m_pkey)
        return jwk;
    const EC_KEY *ec = EVP_PKEY_get0_EC_KEY(m_pkey);
    if (!ec)
        return jwk;
    const EC_POINT *point = EC_KEY_get0_public_key(ec);
    const EC_GROUP *group = EC_KEY_get0_group(ec);
    BIGNUM *x = BN_new();
    BIGNUM *y = BN_new();
    unsigned char xBuf[32];
    unsigned char yBuf[32];
    if (EC_POINT_get_affine_coordinates(group, point, x, y, nullptr) == 1
            && BN_bn2binpad(x, xBuf, 32) == 32
            && BN_bn2binpad(y, yBuf, 32) == 32) {
        jwk.insert(QStringLiteral("kty"), QStringLiteral("EC"));
        jwk.insert(QStringLiteral("crv"), QStringLiteral("P-256"));
        jwk.insert(QStringLiteral("x"),
                   QString::fromLatin1(base64Url(QByteArray((char *)xBuf, 32))));
        jwk.insert(QStringLiteral("y"),
                   QString::fromLatin1(base64Url(QByteArray((char *)yBuf, 32))));
    }
    BN_free(x);
    BN_free(y);
    return jwk;
}

QString DpopKey::signJwt(const QJsonObject &header, const QJsonObject &payload) const
{
    if (!m_pkey)
        return QString();

    const QByteArray input =
            base64Url(QJsonDocument(header).toJson(QJsonDocument::Compact))
            + '.'
            + base64Url(QJsonDocument(payload).toJson(QJsonDocument::Compact));

    EVP_MD_CTX *md = EVP_MD_CTX_new();
    size_t derLen = 0;
    QByteArray der;
    bool ok = EVP_DigestSignInit(md, nullptr, EVP_sha256(), nullptr, m_pkey) == 1
            && EVP_DigestSign(md, nullptr, &derLen,
                              (const unsigned char *)input.constData(), input.size()) == 1;
    if (ok) {
        der.resize((int)derLen);
        ok = EVP_DigestSign(md, (unsigned char *)der.data(), &derLen,
                            (const unsigned char *)input.constData(), input.size()) == 1;
        der.resize((int)derLen);
    }
    EVP_MD_CTX_free(md);
    if (!ok)
        return QString();

    // JOSE wants the raw 64-byte r||s form, not DER
    const unsigned char *p = (const unsigned char *)der.constData();
    ECDSA_SIG *sig = d2i_ECDSA_SIG(nullptr, &p, der.size());
    if (!sig)
        return QString();
    const BIGNUM *r = nullptr;
    const BIGNUM *s = nullptr;
    ECDSA_SIG_get0(sig, &r, &s);
    unsigned char raw[64];
    ok = BN_bn2binpad(r, raw, 32) == 32 && BN_bn2binpad(s, raw + 32, 32) == 32;
    ECDSA_SIG_free(sig);
    if (!ok)
        return QString();

    return QString::fromLatin1(input + '.' + base64Url(QByteArray((char *)raw, 64)));
}

QString DpopKey::proof(const QString &method, const QString &url,
                       const QString &nonce, const QString &accessToken) const
{
    QJsonObject header;
    header.insert(QStringLiteral("typ"), QStringLiteral("dpop+jwt"));
    header.insert(QStringLiteral("alg"), QStringLiteral("ES256"));
    header.insert(QStringLiteral("jwk"), publicJwk());

    // htu is the URI without query or fragment
    QUrl htu(url);
    htu.setQuery(QString());
    htu.setFragment(QString());

    QJsonObject payload;
    payload.insert(QStringLiteral("jti"),
                   QString::fromLatin1(base64Url(randomBytes(12))));
    payload.insert(QStringLiteral("htm"), method);
    payload.insert(QStringLiteral("htu"), htu.toString());
    payload.insert(QStringLiteral("iat"),
                   (double)(QDateTime::currentMSecsSinceEpoch() / 1000));
    if (!nonce.isEmpty())
        payload.insert(QStringLiteral("nonce"), nonce);
    if (!accessToken.isEmpty()) {
        const QByteArray hash = QCryptographicHash::hash(accessToken.toUtf8(),
                                                         QCryptographicHash::Sha256);
        payload.insert(QStringLiteral("ath"), QString::fromLatin1(base64Url(hash)));
    }
    return signJwt(header, payload);
}

QByteArray DpopKey::base64Url(const QByteArray &data)
{
    return data.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
}

QByteArray DpopKey::randomBytes(int count)
{
    QByteArray bytes(count, 0);
    RAND_bytes((unsigned char *)bytes.data(), count);
    return bytes;
}

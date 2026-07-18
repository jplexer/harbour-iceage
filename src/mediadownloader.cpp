#include "mediadownloader.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QStandardPaths>
#include <QUrlQuery>

MediaDownloader::MediaDownloader(QObject *parent)
    : QObject(parent)
{
}

void MediaDownloader::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged();
}

void MediaDownloader::downloadImage(const QString &url)
{
    if (m_busy)
        return;
    setBusy(true);
    // Bluesky CDN fullsize URLs end in @jpeg / @png
    const QString suffix = url.endsWith(QLatin1String("@png"))
            ? QStringLiteral(".png") : QStringLiteral(".jpg");
    fetchToFile(QUrl(url),
                QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
                suffix);
}

void MediaDownloader::downloadVideo(const QString &did, const QString &cid)
{
    if (m_busy || did.isEmpty() || cid.isEmpty())
        return;
    setBusy(true);
    resolvePds(did, cid);
}

void MediaDownloader::resolvePds(const QString &did, const QString &cid)
{
    QUrl url;
    if (did.startsWith(QLatin1String("did:plc:"))) {
        url = QUrl(QStringLiteral("https://plc.directory/") + did);
    } else if (did.startsWith(QLatin1String("did:web:"))) {
        url = QUrl(QStringLiteral("https://") + did.mid(8)
                   + QStringLiteral("/.well-known/did.json"));
    } else {
        setBusy(false);
        emit downloadError(tr("Unsupported DID method"));
        return;
    }

    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
    QNetworkReply *reply = m_nam.get(request);
    QPointer<MediaDownloader> guard(this);
    connect(reply, &QNetworkReply::finished, this, [guard, reply, did, cid]() {
        reply->deleteLater();
        if (!guard)
            return;
        const QJsonObject doc = QJsonDocument::fromJson(reply->readAll()).object();
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
            guard->setBusy(false);
            emit guard->downloadError(tr("Could not resolve the author's server"));
            return;
        }

        QUrl url(pds + QStringLiteral("/xrpc/com.atproto.sync.getBlob"));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("did"), did);
        query.addQueryItem(QStringLiteral("cid"), cid);
        url.setQuery(query);
        guard->fetchToFile(url,
                           QStandardPaths::writableLocation(QStandardPaths::MoviesLocation),
                           QStringLiteral(".mp4"));
    });
}

void MediaDownloader::fetchToFile(const QUrl &url, const QString &dir,
                                  const QString &suffix)
{
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
    QNetworkReply *reply = m_nam.get(request);
    QPointer<MediaDownloader> guard(this);
    connect(reply, &QNetworkReply::finished, this, [guard, reply, dir, suffix]() {
        reply->deleteLater();
        if (!guard)
            return;
        guard->setBusy(false);

        const int status =
                reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status >= 400 || reply->error() != QNetworkReply::NoError) {
            emit guard->downloadError(reply->errorString());
            return;
        }

        QDir().mkpath(dir);
        const QString path = dir + QStringLiteral("/IceAge_")
                + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss"))
                + suffix;
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            emit guard->downloadError(tr("Could not write %1").arg(path));
            return;
        }
        file.write(reply->readAll());
        file.close();
        emit guard->finished(path);
    });
}

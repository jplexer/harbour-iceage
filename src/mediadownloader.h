#ifndef MEDIADOWNLOADER_H
#define MEDIADOWNLOADER_H

#include <QNetworkAccessManager>
#include <QObject>

// Saves post media locally: images (CDN URL) into Pictures, videos into
// Videos. Videos are fetched as the original blob from the author's PDS
// (com.atproto.sync.getBlob) since the playback URL is an HLS playlist.
class MediaDownloader : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
public:
    explicit MediaDownloader(QObject *parent = nullptr);

    bool busy() const { return m_busy; }

    Q_INVOKABLE void downloadImage(const QString &url);
    Q_INVOKABLE void downloadVideo(const QString &did, const QString &cid);

signals:
    void busyChanged();
    void finished(const QString &path);
    void downloadError(const QString &message);

private:
    void setBusy(bool busy);
    // Resolves the DID document (plc.directory or did:web) to the PDS
    void resolvePds(const QString &did, const QString &cid);
    void fetchToFile(const QUrl &url, const QString &dir, const QString &suffix);

    QNetworkAccessManager m_nam;
    bool m_busy = false;
};

#endif // MEDIADOWNLOADER_H

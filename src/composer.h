#ifndef COMPOSER_H
#define COMPOSER_H

#include <QJsonArray>
#include <QObject>
#include <QStringList>

#include <memory>

#include "session.h"

// Creates app.bsky.feed.post records, optionally as a reply. Links and
// hashtags are detected client-side; @mentions are resolved to DIDs via
// com.atproto.identity.resolveHandle before posting, so they become real
// mention facets. Unresolvable mentions post as plain text.
class Composer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(Session *session READ session WRITE setSession NOTIFY sessionChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    // Local file paths of attached images (max 4, like Bluesky)
    Q_PROPERTY(QStringList images READ images NOTIFY imagesChanged)
public:
    explicit Composer(QObject *parent = nullptr);

    Session *session() const { return m_session; }
    void setSession(Session *session);
    bool busy() const { return m_busy; }
    QStringList images() const { return m_images; }

    Q_INVOKABLE void addImage(const QString &pathOrUrl);
    Q_INVOKABLE void removeImage(int index);
    // Hashtag autocomplete from tags seen in parsed posts
    Q_INVOKABLE QStringList suggestTags(const QString &prefix) const;

    // Reply/quote parameters are empty for a plain top-level post.
    Q_INVOKABLE void post(const QString &text,
                          const QString &replyParentUri = QString(),
                          const QString &replyParentCid = QString(),
                          const QString &replyRootUri = QString(),
                          const QString &replyRootCid = QString(),
                          const QString &quoteUri = QString(),
                          const QString &quoteCid = QString());

signals:
    void sessionChanged();
    void busyChanged();
    void imagesChanged();
    void posted();
    void postError(const QString &message);

private:
    struct ReplyRefs {
        QString parentUri;
        QString parentCid;
        QString rootUri;
        QString rootCid;
        // Quoted post (app.bsky.embed.record), if any
        QString quoteUri;
        QString quoteCid;
    };

    // Uploads attached images one by one (recompressed under the ~1MB
    // blob limit), then sends the record with an images embed.
    void uploadImages(const QString &text, const ReplyRefs &reply,
                      const QHash<QString, QString> &mentionDids,
                      std::shared_ptr<QJsonArray> items, int index);
    void sendRecord(const QString &text, const ReplyRefs &reply,
                    const QHash<QString, QString> &mentionDids,
                    const QJsonArray &imageItems);
    void setBusy(bool busy);

    Session *m_session = nullptr;
    bool m_busy = false;
    QStringList m_images;
};

#endif // COMPOSER_H

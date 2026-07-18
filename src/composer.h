#ifndef COMPOSER_H
#define COMPOSER_H

#include <QObject>

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
public:
    explicit Composer(QObject *parent = nullptr);

    Session *session() const { return m_session; }
    void setSession(Session *session);
    bool busy() const { return m_busy; }

    // Reply parameters are empty for a top-level post.
    Q_INVOKABLE void post(const QString &text,
                          const QString &replyParentUri = QString(),
                          const QString &replyParentCid = QString(),
                          const QString &replyRootUri = QString(),
                          const QString &replyRootCid = QString());

signals:
    void sessionChanged();
    void busyChanged();
    void posted();
    void postError(const QString &message);

private:
    struct ReplyRefs {
        QString parentUri;
        QString parentCid;
        QString rootUri;
        QString rootCid;
    };

    void sendRecord(const QString &text, const ReplyRefs &reply,
                    const QHash<QString, QString> &mentionDids);
    void setBusy(bool busy);

    Session *m_session = nullptr;
    bool m_busy = false;
};

#endif // COMPOSER_H

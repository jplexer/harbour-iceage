#ifndef AUTHORFEEDMODEL_H
#define AUTHORFEEDMODEL_H

#include "abstractpostmodel.h"

// One account's posts (app.bsky.feed.getAuthorFeed) plus their profile
// (app.bsky.actor.getProfile), for the profile page.
class AuthorFeedModel : public AbstractPostModel
{
    Q_OBJECT
    Q_PROPERTY(QString actor READ actor WRITE setActor NOTIFY actorChanged)
    Q_PROPERTY(QVariantMap profile READ profile NOTIFY profileChanged)
    // getAuthorFeed filter: posts_no_replies (default) or posts_with_replies
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)
public:
    explicit AuthorFeedModel(QObject *parent = nullptr);

    QString actor() const { return m_actor; }
    void setActor(const QString &actor);
    QVariantMap profile() const { return m_profile; }
    QString filter() const { return m_filter; }
    void setFilter(const QString &filter);

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void loadMore();
    // Follow or unfollow this profile (app.bsky.graph.follow record),
    // optimistically updating the profile map.
    Q_INVOKABLE void toggleFollow();
    // Mute/unmute (app.bsky.graph.muteActor) and block/unblock
    // (app.bsky.graph.block record), same optimistic pattern.
    Q_INVOKABLE void toggleMute();
    Q_INVOKABLE void toggleBlock();

signals:
    void actorChanged();
    void profileChanged();
    void filterChanged();

private:
    void fetchProfile();
    void fetchFeed(bool reset);

    QString m_actor;
    QString m_cursor;
    QString m_filter = QStringLiteral("posts_no_replies");
    QVariantMap m_profile;
};

#endif // AUTHORFEEDMODEL_H

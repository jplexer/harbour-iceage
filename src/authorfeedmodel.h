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
public:
    explicit AuthorFeedModel(QObject *parent = nullptr);

    QString actor() const { return m_actor; }
    void setActor(const QString &actor);
    QVariantMap profile() const { return m_profile; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void loadMore();
    // Follow or unfollow this profile (app.bsky.graph.follow record),
    // optimistically updating the profile map.
    Q_INVOKABLE void toggleFollow();

signals:
    void actorChanged();
    void profileChanged();

private:
    void fetchProfile();
    void fetchFeed(bool reset);

    QString m_actor;
    QString m_cursor;
    QVariantMap m_profile;
};

#endif // AUTHORFEEDMODEL_H

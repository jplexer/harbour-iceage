#include "authorfeedmodel.h"

#include <QDateTime>
#include <QJsonArray>
#include <QPointer>

AuthorFeedModel::AuthorFeedModel(QObject *parent)
    : AbstractPostModel(parent)
{
}

void AuthorFeedModel::setActor(const QString &actor)
{
    if (m_actor == actor)
        return;
    m_actor = actor;
    emit actorChanged();
}

void AuthorFeedModel::setFilter(const QString &filter)
{
    if (m_filter == filter)
        return;
    m_filter = filter;
    emit filterChanged();
    fetchFeed(true);
}

void AuthorFeedModel::refresh()
{
    fetchProfile();
    fetchFeed(true);
}

void AuthorFeedModel::loadMore()
{
    if (!m_cursor.isEmpty())
        fetchFeed(false);
}

void AuthorFeedModel::fetchProfile()
{
    if (!session() || !session()->loggedIn() || m_actor.isEmpty())
        return;

    QVariantMap params;
    params.insert(QStringLiteral("actor"), m_actor);

    QPointer<AuthorFeedModel> guard(this);
    session()->getAuthorized(QStringLiteral("app.bsky.actor.getProfile"), params,
                             [guard](const QJsonObject &data, const QString &error) {
        if (!guard || !error.isEmpty())
            return;
        QVariantMap profile;
        profile.insert(QStringLiteral("handle"),
                       data.value(QStringLiteral("handle")).toString());
        QString name = data.value(QStringLiteral("displayName")).toString();
        if (name.isEmpty())
            name = data.value(QStringLiteral("handle")).toString();
        profile.insert(QStringLiteral("displayName"), name);
        profile.insert(QStringLiteral("avatar"),
                       data.value(QStringLiteral("avatar")).toString());
        profile.insert(QStringLiteral("banner"),
                       data.value(QStringLiteral("banner")).toString());
        profile.insert(QStringLiteral("description"),
                       data.value(QStringLiteral("description")).toString());
        profile.insert(QStringLiteral("followersCount"),
                       data.value(QStringLiteral("followersCount")).toInt());
        profile.insert(QStringLiteral("followsCount"),
                       data.value(QStringLiteral("followsCount")).toInt());
        profile.insert(QStringLiteral("postsCount"),
                       data.value(QStringLiteral("postsCount")).toInt());
        profile.insert(QStringLiteral("did"), data.value(QStringLiteral("did")).toString());
        const QJsonObject viewer = data.value(QStringLiteral("viewer")).toObject();
        profile.insert(QStringLiteral("followingUri"),
                       viewer.value(QStringLiteral("following")).toString());
        profile.insert(QStringLiteral("followsYou"),
                       !viewer.value(QStringLiteral("followedBy")).toString().isEmpty());
        profile.insert(QStringLiteral("muted"),
                       viewer.value(QStringLiteral("muted")).toBool());
        profile.insert(QStringLiteral("blockingUri"),
                       viewer.value(QStringLiteral("blocking")).toString());
        guard->m_profile = profile;
        emit guard->profileChanged();
    });
}

void AuthorFeedModel::toggleFollow()
{
    if (!session() || !session()->loggedIn())
        return;
    const QString did = m_profile.value(QStringLiteral("did")).toString();
    const QString followingUri = m_profile.value(QStringLiteral("followingUri")).toString();
    if (did.isEmpty() || followingUri == QLatin1String("pending"))
        return;

    QPointer<AuthorFeedModel> guard(this);
    const int followers = m_profile.value(QStringLiteral("followersCount")).toInt();

    if (followingUri.isEmpty()) {
        // Follow, optimistically
        m_profile.insert(QStringLiteral("followingUri"), QStringLiteral("pending"));
        m_profile.insert(QStringLiteral("followersCount"), followers + 1);
        emit profileChanged();

        QJsonObject record;
        record.insert(QStringLiteral("$type"), QStringLiteral("app.bsky.graph.follow"));
        record.insert(QStringLiteral("subject"), did);
        record.insert(QStringLiteral("createdAt"),
                      QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
        QJsonObject body;
        body.insert(QStringLiteral("repo"), session()->did());
        body.insert(QStringLiteral("collection"), QStringLiteral("app.bsky.graph.follow"));
        body.insert(QStringLiteral("record"), record);

        session()->postAuthorized(QStringLiteral("com.atproto.repo.createRecord"), body,
                                  [guard, followers](const QJsonObject &data,
                                                     const QString &error) {
            if (!guard)
                return;
            if (error.isEmpty()) {
                guard->m_profile.insert(QStringLiteral("followingUri"),
                                        data.value(QStringLiteral("uri")).toString());
            } else {
                guard->m_profile.insert(QStringLiteral("followingUri"), QString());
                guard->m_profile.insert(QStringLiteral("followersCount"), followers);
                emit guard->errorOccurred(error);
            }
            emit guard->profileChanged();
        });
    } else {
        // Unfollow, optimistically
        m_profile.insert(QStringLiteral("followingUri"), QString());
        m_profile.insert(QStringLiteral("followersCount"), followers - 1);
        emit profileChanged();

        QJsonObject body;
        body.insert(QStringLiteral("repo"), session()->did());
        body.insert(QStringLiteral("collection"), QStringLiteral("app.bsky.graph.follow"));
        body.insert(QStringLiteral("rkey"),
                    followingUri.section(QLatin1Char('/'), -1));

        session()->postAuthorized(QStringLiteral("com.atproto.repo.deleteRecord"), body,
                                  [guard, followingUri, followers](const QJsonObject &,
                                                                   const QString &error) {
            if (!guard || error.isEmpty())
                return;
            guard->m_profile.insert(QStringLiteral("followingUri"), followingUri);
            guard->m_profile.insert(QStringLiteral("followersCount"), followers);
            emit guard->errorOccurred(error);
            emit guard->profileChanged();
        });
    }
}

void AuthorFeedModel::toggleMute()
{
    if (!session() || !session()->loggedIn())
        return;
    const QString did = m_profile.value(QStringLiteral("did")).toString();
    if (did.isEmpty())
        return;
    const bool muted = m_profile.value(QStringLiteral("muted")).toBool();

    m_profile.insert(QStringLiteral("muted"), !muted);
    emit profileChanged();

    QJsonObject body;
    body.insert(QStringLiteral("actor"), did);
    const QString endpoint = muted
            ? QStringLiteral("app.bsky.graph.unmuteActor")
            : QStringLiteral("app.bsky.graph.muteActor");

    QPointer<AuthorFeedModel> guard(this);
    session()->postAuthorized(endpoint, body,
                              [guard, muted](const QJsonObject &, const QString &error) {
        if (!guard || error.isEmpty())
            return;
        guard->m_profile.insert(QStringLiteral("muted"), muted);
        emit guard->errorOccurred(error);
        emit guard->profileChanged();
    });
}

void AuthorFeedModel::toggleBlock()
{
    if (!session() || !session()->loggedIn())
        return;
    const QString did = m_profile.value(QStringLiteral("did")).toString();
    const QString blockingUri = m_profile.value(QStringLiteral("blockingUri")).toString();
    if (did.isEmpty() || blockingUri == QLatin1String("pending"))
        return;

    QPointer<AuthorFeedModel> guard(this);

    if (blockingUri.isEmpty()) {
        // Block, optimistically
        m_profile.insert(QStringLiteral("blockingUri"), QStringLiteral("pending"));
        emit profileChanged();

        QJsonObject record;
        record.insert(QStringLiteral("$type"), QStringLiteral("app.bsky.graph.block"));
        record.insert(QStringLiteral("subject"), did);
        record.insert(QStringLiteral("createdAt"),
                      QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
        QJsonObject body;
        body.insert(QStringLiteral("repo"), session()->did());
        body.insert(QStringLiteral("collection"), QStringLiteral("app.bsky.graph.block"));
        body.insert(QStringLiteral("record"), record);

        session()->postAuthorized(QStringLiteral("com.atproto.repo.createRecord"), body,
                                  [guard](const QJsonObject &data, const QString &error) {
            if (!guard)
                return;
            if (error.isEmpty()) {
                guard->m_profile.insert(QStringLiteral("blockingUri"),
                                        data.value(QStringLiteral("uri")).toString());
            } else {
                guard->m_profile.insert(QStringLiteral("blockingUri"), QString());
                emit guard->errorOccurred(error);
            }
            emit guard->profileChanged();
        });
    } else {
        // Unblock, optimistically
        m_profile.insert(QStringLiteral("blockingUri"), QString());
        emit profileChanged();

        QJsonObject body;
        body.insert(QStringLiteral("repo"), session()->did());
        body.insert(QStringLiteral("collection"), QStringLiteral("app.bsky.graph.block"));
        body.insert(QStringLiteral("rkey"),
                    blockingUri.section(QLatin1Char('/'), -1));

        session()->postAuthorized(QStringLiteral("com.atproto.repo.deleteRecord"), body,
                                  [guard, blockingUri](const QJsonObject &,
                                                       const QString &error) {
            if (!guard || error.isEmpty())
                return;
            guard->m_profile.insert(QStringLiteral("blockingUri"), blockingUri);
            emit guard->errorOccurred(error);
            emit guard->profileChanged();
        });
    }
}

void AuthorFeedModel::fetchFeed(bool reset)
{
    if (!session() || !session()->loggedIn() || m_actor.isEmpty() || loading())
        return;
    setLoading(true);

    QVariantMap params;
    params.insert(QStringLiteral("actor"), m_actor);
    params.insert(QStringLiteral("limit"), 50);
    params.insert(QStringLiteral("filter"), m_filter);
    if (!reset)
        params.insert(QStringLiteral("cursor"), m_cursor);

    QPointer<AuthorFeedModel> guard(this);
    const QString filter = m_filter;
    session()->getAuthorized(QStringLiteral("app.bsky.feed.getAuthorFeed"), params,
                             [guard, reset, filter](const QJsonObject &data, const QString &error) {
        if (!guard)
            return;
        guard->setLoading(false);
        if (guard->m_filter != filter) {
            // Tab switched while this fetch was in flight; the switch's own
            // fetch was blocked by the loading guard, so run it now.
            guard->fetchFeed(true);
            return;
        }
        if (!error.isEmpty()) {
            emit guard->errorOccurred(error);
            return;
        }
        guard->m_cursor = data.value(QStringLiteral("cursor")).toString();

        QList<Post> posts;
        const QJsonArray feed = data.value(QStringLiteral("feed")).toArray();
        for (const QJsonValue &value : feed)
            posts.append(PostRoles::parseFeedItem(value.toObject()));

        if (reset) {
            guard->beginResetModel();
            guard->m_posts = posts;
            guard->endResetModel();
        } else if (!posts.isEmpty()) {
            guard->beginInsertRows(QModelIndex(), guard->m_posts.count(),
                                   guard->m_posts.count() + posts.count() - 1);
            guard->m_posts += posts;
            guard->endInsertRows();
        }
    });
}

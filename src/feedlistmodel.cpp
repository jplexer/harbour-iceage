#include "feedlistmodel.h"

#include <QJsonArray>
#include <QPointer>

FeedListModel::FeedListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

void FeedListModel::setSession(Session *session)
{
    if (m_session == session)
        return;
    m_session = session;
    emit sessionChanged();
}

void FeedListModel::setLoading(bool loading)
{
    if (m_loading == loading)
        return;
    m_loading = loading;
    emit loadingChanged();
}

int FeedListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_feeds.count();
}

QVariant FeedListModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < 0 || index.row() >= m_feeds.count())
        return QVariant();
    const Feed &feed = m_feeds.at(index.row());
    switch (role) {
    case UriRole: return feed.uri;
    case NameRole: return feed.name;
    case AvatarRole: return feed.avatar;
    }
    return QVariant();
}

QHash<int, QByteArray> FeedListModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles.insert(UriRole, "uri");
    roles.insert(NameRole, "name");
    roles.insert(AvatarRole, "avatar");
    return roles;
}

void FeedListModel::refresh()
{
    if (!m_session || !m_session->loggedIn() || m_loading)
        return;
    setLoading(true);

    QPointer<FeedListModel> guard(this);
    m_session->getAuthorized(QStringLiteral("app.bsky.actor.getPreferences"),
                             QVariantMap(),
                             [guard](const QJsonObject &data, const QString &error) {
        if (!guard)
            return;
        if (!error.isEmpty()) {
            guard->setLoading(false);
            emit guard->errorOccurred(error);
            return;
        }

        // savedFeedsPrefV2 supersedes savedFeedsPref; accept either
        QStringList pinned;
        const QJsonArray prefs = data.value(QStringLiteral("preferences")).toArray();
        for (const QJsonValue &value : prefs) {
            const QJsonObject pref = value.toObject();
            const QString type = pref.value(QStringLiteral("$type")).toString();
            if (type == QLatin1String("app.bsky.actor.defs#savedFeedsPrefV2")) {
                const QJsonArray items = pref.value(QStringLiteral("items")).toArray();
                for (const QJsonValue &iv : items) {
                    const QJsonObject item = iv.toObject();
                    if (item.value(QStringLiteral("type")).toString()
                                == QLatin1String("feed")
                            && item.value(QStringLiteral("pinned")).toBool()) {
                        pinned.append(item.value(QStringLiteral("value")).toString());
                    }
                }
                break;
            }
            if (type == QLatin1String("app.bsky.actor.defs#savedFeedsPref")) {
                const QJsonArray uris = pref.value(QStringLiteral("pinned")).toArray();
                for (const QJsonValue &uv : uris) {
                    const QString uri = uv.toString();
                    if (uri.contains(QLatin1String("/app.bsky.feed.generator/")))
                        pinned.append(uri);
                }
                // keep scanning: a V2 pref later in the list wins
            }
        }

        if (pinned.isEmpty()) {
            guard->setLoading(false);
            if (!guard->m_feeds.isEmpty()) {
                guard->beginResetModel();
                guard->m_feeds.clear();
                guard->endResetModel();
            }
            return;
        }
        guard->fetchGenerators(pinned);
    });
}

void FeedListModel::fetchGenerators(const QStringList &uris)
{
    QVariantMap params;
    QVariantList feeds;
    for (const QString &uri : uris)
        feeds.append(uri);
    params.insert(QStringLiteral("feeds"), feeds);

    QPointer<FeedListModel> guard(this);
    m_session->getAuthorized(QStringLiteral("app.bsky.feed.getFeedGenerators"), params,
                             [guard, uris](const QJsonObject &data, const QString &error) {
        if (!guard)
            return;
        guard->setLoading(false);
        if (!error.isEmpty()) {
            emit guard->errorOccurred(error);
            return;
        }

        // Preserve the pinned order from preferences
        QHash<QString, Feed> byUri;
        const QJsonArray views = data.value(QStringLiteral("feeds")).toArray();
        for (const QJsonValue &value : views) {
            const QJsonObject view = value.toObject();
            Feed feed;
            feed.uri = view.value(QStringLiteral("uri")).toString();
            feed.name = view.value(QStringLiteral("displayName")).toString();
            feed.avatar = view.value(QStringLiteral("avatar")).toString();
            byUri.insert(feed.uri, feed);
        }
        QList<Feed> feeds;
        for (const QString &uri : uris) {
            if (byUri.contains(uri))
                feeds.append(byUri.value(uri));
        }

        guard->beginResetModel();
        guard->m_feeds = feeds;
        guard->endResetModel();
    });
}

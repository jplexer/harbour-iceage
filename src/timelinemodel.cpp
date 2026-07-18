#include "timelinemodel.h"

#include <QJsonArray>
#include <QPointer>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>

namespace {
// Same sandbox-writable file Session uses (see session.cpp)
QString settingsPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
            + QStringLiteral("/settings.conf");
}
}

TimelineModel::TimelineModel(QObject *parent)
    : AbstractPostModel(parent)
{
    // Restore the last selected feed; the login-triggered refresh
    // fetches it
    QSettings settings(settingsPath(), QSettings::IniFormat);
    m_feedUri = settings.value(QStringLiteral("timeline/feedUri")).toString();
    m_feedName = settings.value(QStringLiteral("timeline/feedName")).toString();
}

void TimelineModel::setFeedUri(const QString &feedUri)
{
    if (m_feedUri == feedUri)
        return;
    m_feedUri = feedUri;
    QSettings settings(settingsPath(), QSettings::IniFormat);
    settings.setValue(QStringLiteral("timeline/feedUri"), m_feedUri);
    emit feedUriChanged();
    m_cursor.clear();
    fetch(true);
}

void TimelineModel::setFeedName(const QString &feedName)
{
    if (m_feedName == feedName)
        return;
    m_feedName = feedName;
    QSettings settings(settingsPath(), QSettings::IniFormat);
    settings.setValue(QStringLiteral("timeline/feedName"), m_feedName);
    emit feedNameChanged();
}

QString TimelineModel::endpoint() const
{
    return m_feedUri.isEmpty() ? QStringLiteral("app.bsky.feed.getTimeline")
                               : QStringLiteral("app.bsky.feed.getFeed");
}

QVariantMap TimelineModel::baseParams() const
{
    QVariantMap params;
    params.insert(QStringLiteral("limit"), 50);
    if (!m_feedUri.isEmpty())
        params.insert(QStringLiteral("feed"), m_feedUri);
    return params;
}

void TimelineModel::refresh()
{
    fetch(true);
}

void TimelineModel::loadMore()
{
    if (!m_cursor.isEmpty())
        fetch(false);
}

void TimelineModel::loadNewer()
{
    if (m_posts.isEmpty()) {
        refresh();
        return;
    }
    if (!session() || !session()->loggedIn() || loading())
        return;
    setLoading(true);

    QPointer<TimelineModel> guard(this);
    const QString feedUri = m_feedUri;
    session()->getAuthorized(endpoint(), baseParams(),
                             [guard, feedUri](const QJsonObject &data, const QString &error) {
        if (!guard)
            return;
        guard->setLoading(false);
        if (guard->m_feedUri != feedUri) {
            // Feed switched mid-flight; its own fetch was blocked by the
            // loading guard, so run it now.
            guard->fetch(true);
            return;
        }
        if (!error.isEmpty()) {
            emit guard->errorOccurred(error);
            return;
        }

        QList<Post> fetched;
        const QJsonArray feed = data.value(QStringLiteral("feed")).toArray();
        for (const QJsonValue &value : feed)
            fetched.append(PostRoles::parseFeedItem(value.toObject()));

        // A repost shows the same uri with a different reason, so key on both
        QSet<QString> known;
        for (const Post &post : guard->m_posts)
            known.insert(post.uri + QLatin1Char('|') + post.repostedBy);

        int overlap = -1;
        for (int i = 0; i < fetched.count(); ++i) {
            if (known.contains(fetched.at(i).uri + QLatin1Char('|')
                               + fetched.at(i).repostedBy)) {
                overlap = i;
                break;
            }
        }

        if (overlap < 0) {
            // More than a page of new posts: the lists don't connect, so
            // start over from the top
            guard->m_cursor = data.value(QStringLiteral("cursor")).toString();
            guard->beginResetModel();
            guard->m_posts = fetched;
            guard->endResetModel();
        } else if (overlap > 0) {
            // Keep the old pagination cursor; it continues from the tail
            guard->beginInsertRows(QModelIndex(), 0, overlap - 1);
            for (int i = overlap - 1; i >= 0; --i)
                guard->m_posts.prepend(fetched.at(i));
            guard->endInsertRows();
        }
    });
}

void TimelineModel::fetch(bool reset)
{
    if (!session() || !session()->loggedIn() || loading())
        return;
    setLoading(true);

    QVariantMap params = baseParams();
    if (!reset)
        params.insert(QStringLiteral("cursor"), m_cursor);

    // The model is owned by QML and may be destroyed while the request is in
    // flight; the QPointer guard makes the callback a no-op in that case.
    QPointer<TimelineModel> guard(this);
    const QString feedUri = m_feedUri;
    session()->getAuthorized(endpoint(), params,
                             [guard, reset, feedUri](const QJsonObject &data,
                                                     const QString &error) {
        if (!guard)
            return;
        guard->setLoading(false);
        if (guard->m_feedUri != feedUri) {
            guard->fetch(true);
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

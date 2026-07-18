#include "timelinemodel.h"

#include <QJsonArray>
#include <QPointer>

TimelineModel::TimelineModel(QObject *parent)
    : AbstractPostModel(parent)
{
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

void TimelineModel::fetch(bool reset)
{
    if (!session() || !session()->loggedIn() || loading())
        return;
    setLoading(true);

    QVariantMap params;
    params.insert(QStringLiteral("limit"), 50);
    if (!reset)
        params.insert(QStringLiteral("cursor"), m_cursor);

    // The model is owned by QML and may be destroyed while the request is in
    // flight; the QPointer guard makes the callback a no-op in that case.
    QPointer<TimelineModel> guard(this);
    session()->getAuthorized(QStringLiteral("app.bsky.feed.getTimeline"), params,
                             [guard, reset](const QJsonObject &data, const QString &error) {
        if (!guard)
            return;
        guard->setLoading(false);
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

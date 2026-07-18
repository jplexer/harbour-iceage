#include "searchpostsmodel.h"

#include <QJsonArray>
#include <QPointer>

SearchPostsModel::SearchPostsModel(QObject *parent)
    : AbstractPostModel(parent)
{
}

void SearchPostsModel::setQuery(const QString &query)
{
    if (m_query == query)
        return;
    m_query = query;
    emit queryChanged();
}

void SearchPostsModel::search()
{
    if (m_query.trimmed().isEmpty()) {
        if (!m_posts.isEmpty()) {
            beginResetModel();
            m_posts.clear();
            endResetModel();
        }
        return;
    }
    fetch(true);
}

void SearchPostsModel::loadMore()
{
    if (!m_cursor.isEmpty())
        fetch(false);
}

void SearchPostsModel::fetch(bool reset)
{
    if (!session() || !session()->loggedIn() || loading())
        return;
    setLoading(true);

    QVariantMap params;
    params.insert(QStringLiteral("q"), m_query);
    params.insert(QStringLiteral("limit"), 25);
    if (!reset)
        params.insert(QStringLiteral("cursor"), m_cursor);

    QPointer<SearchPostsModel> guard(this);
    const QString query = m_query;
    session()->getAuthorized(QStringLiteral("app.bsky.feed.searchPosts"), params,
                             [guard, reset, query](const QJsonObject &data,
                                                   const QString &error) {
        if (!guard)
            return;
        guard->setLoading(false);
        if (guard->m_query != query) {
            // Query changed while this fetch was in flight; the new
            // search was blocked by the loading guard, so run it now.
            guard->search();
            return;
        }
        if (!error.isEmpty()) {
            emit guard->errorOccurred(error);
            return;
        }
        guard->m_cursor = data.value(QStringLiteral("cursor")).toString();

        QList<Post> posts;
        const QJsonArray list = data.value(QStringLiteral("posts")).toArray();
        for (const QJsonValue &value : list)
            posts.append(PostRoles::parsePostView(value.toObject()));

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

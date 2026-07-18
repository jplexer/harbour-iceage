#include "actorsearchmodel.h"

#include <QJsonArray>
#include <QPointer>

ActorSearchModel::ActorSearchModel(QObject *parent)
    : ActorModelBase(parent)
{
}

void ActorSearchModel::setQuery(const QString &query)
{
    if (m_query == query)
        return;
    m_query = query;
    emit queryChanged();
}

void ActorSearchModel::search()
{
    if (m_query.trimmed().isEmpty()) {
        clear();
        return;
    }
    fetch(true);
}

void ActorSearchModel::loadMore()
{
    if (!m_cursor.isEmpty())
        fetch(false);
}

void ActorSearchModel::fetch(bool reset)
{
    if (!session() || !session()->loggedIn() || loading())
        return;
    setLoading(true);

    QVariantMap params;
    params.insert(QStringLiteral("q"), m_query);
    params.insert(QStringLiteral("limit"), 25);
    if (!reset)
        params.insert(QStringLiteral("cursor"), m_cursor);

    QPointer<ActorSearchModel> guard(this);
    const QString query = m_query;
    session()->getAuthorized(QStringLiteral("app.bsky.actor.searchActors"), params,
                             [guard, reset, query](const QJsonObject &data,
                                                   const QString &error) {
        if (!guard)
            return;
        guard->setLoading(false);
        if (guard->m_query != query) {
            // Query changed mid-flight; rerun with the current one
            guard->search();
            return;
        }
        if (!error.isEmpty()) {
            emit guard->errorOccurred(error);
            return;
        }
        guard->m_cursor = data.value(QStringLiteral("cursor")).toString();
        guard->applyPage(reset,
                         parseActors(data.value(QStringLiteral("actors")).toArray()));
    });
}

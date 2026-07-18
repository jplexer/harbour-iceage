#include "actorsearchmodel.h"

#include <QJsonArray>
#include <QPointer>

ActorSearchModel::ActorSearchModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

void ActorSearchModel::setSession(Session *session)
{
    if (m_session == session)
        return;
    m_session = session;
    emit sessionChanged();
}

void ActorSearchModel::setQuery(const QString &query)
{
    if (m_query == query)
        return;
    m_query = query;
    emit queryChanged();
}

void ActorSearchModel::setLoading(bool loading)
{
    if (m_loading == loading)
        return;
    m_loading = loading;
    emit loadingChanged();
}

int ActorSearchModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_actors.count();
}

QVariant ActorSearchModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < 0 || index.row() >= m_actors.count())
        return QVariant();
    const Actor &actor = m_actors.at(index.row());
    switch (role) {
    case DidRole: return actor.did;
    case HandleRole: return actor.handle;
    case DisplayNameRole: return actor.displayName;
    case AvatarRole: return actor.avatar;
    case DescriptionRole: return actor.description;
    case FollowsYouRole: return actor.followsYou;
    }
    return QVariant();
}

QHash<int, QByteArray> ActorSearchModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles.insert(DidRole, "did");
    roles.insert(HandleRole, "handle");
    roles.insert(DisplayNameRole, "displayName");
    roles.insert(AvatarRole, "avatar");
    roles.insert(DescriptionRole, "description");
    roles.insert(FollowsYouRole, "followsYou");
    return roles;
}

ActorSearchModel::Actor ActorSearchModel::parseActor(const QJsonObject &json)
{
    Actor actor;
    actor.did = json.value(QStringLiteral("did")).toString();
    actor.handle = json.value(QStringLiteral("handle")).toString();
    actor.displayName = json.value(QStringLiteral("displayName")).toString();
    if (actor.displayName.isEmpty())
        actor.displayName = actor.handle;
    actor.avatar = json.value(QStringLiteral("avatar")).toString();
    actor.description = json.value(QStringLiteral("description")).toString()
                            .simplified();
    const QJsonObject viewer = json.value(QStringLiteral("viewer")).toObject();
    actor.followsYou =
            !viewer.value(QStringLiteral("followedBy")).toString().isEmpty();
    return actor;
}

void ActorSearchModel::clear()
{
    if (m_actors.isEmpty())
        return;
    beginResetModel();
    m_actors.clear();
    endResetModel();
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
    if (!m_session || !m_session->loggedIn() || m_loading)
        return;
    setLoading(true);

    QVariantMap params;
    params.insert(QStringLiteral("q"), m_query);
    params.insert(QStringLiteral("limit"), 25);
    if (!reset)
        params.insert(QStringLiteral("cursor"), m_cursor);

    QPointer<ActorSearchModel> guard(this);
    const QString query = m_query;
    m_session->getAuthorized(QStringLiteral("app.bsky.actor.searchActors"), params,
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

        QList<Actor> actors;
        const QJsonArray list = data.value(QStringLiteral("actors")).toArray();
        for (const QJsonValue &value : list)
            actors.append(parseActor(value.toObject()));

        if (reset) {
            guard->beginResetModel();
            guard->m_actors = actors;
            guard->endResetModel();
        } else if (!actors.isEmpty()) {
            guard->beginInsertRows(QModelIndex(), guard->m_actors.count(),
                                   guard->m_actors.count() + actors.count() - 1);
            guard->m_actors += actors;
            guard->endInsertRows();
        }
    });
}

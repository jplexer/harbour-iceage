#include "actormodelbase.h"

#include <QJsonArray>

ActorModelBase::ActorModelBase(QObject *parent)
    : QAbstractListModel(parent)
{
}

void ActorModelBase::setSession(Session *session)
{
    if (m_session == session)
        return;
    m_session = session;
    emit sessionChanged();
}

void ActorModelBase::setLoading(bool loading)
{
    if (m_loading == loading)
        return;
    m_loading = loading;
    emit loadingChanged();
}

int ActorModelBase::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_actors.count();
}

QVariant ActorModelBase::data(const QModelIndex &index, int role) const
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

QHash<int, QByteArray> ActorModelBase::roleNames() const
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

ActorModelBase::Actor ActorModelBase::parseActor(const QJsonObject &json)
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

QList<ActorModelBase::Actor> ActorModelBase::parseActors(const QJsonArray &list)
{
    QList<Actor> actors;
    for (const QJsonValue &value : list)
        actors.append(parseActor(value.toObject()));
    return actors;
}

void ActorModelBase::applyPage(bool reset, const QList<Actor> &actors)
{
    if (reset) {
        beginResetModel();
        m_actors = actors;
        endResetModel();
    } else if (!actors.isEmpty()) {
        beginInsertRows(QModelIndex(), m_actors.count(),
                        m_actors.count() + actors.count() - 1);
        m_actors += actors;
        endInsertRows();
    }
}

void ActorModelBase::clear()
{
    if (m_actors.isEmpty())
        return;
    beginResetModel();
    m_actors.clear();
    endResetModel();
}

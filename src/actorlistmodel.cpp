#include "actorlistmodel.h"

#include <QJsonArray>
#include <QPointer>

ActorListModel::ActorListModel(QObject *parent)
    : ActorModelBase(parent)
{
}

void ActorListModel::setActor(const QString &actor)
{
    if (m_actor == actor)
        return;
    m_actor = actor;
    emit actorChanged();
}

void ActorListModel::setMode(const QString &mode)
{
    if (m_mode == mode)
        return;
    m_mode = mode;
    emit modeChanged();
}

void ActorListModel::refresh()
{
    fetch(true);
}

void ActorListModel::loadMore()
{
    if (!m_cursor.isEmpty())
        fetch(false);
}

void ActorListModel::fetch(bool reset)
{
    if (!session() || !session()->loggedIn() || m_actor.isEmpty() || loading())
        return;
    setLoading(true);

    const bool following = m_mode == QLatin1String("following");
    const QString endpoint = following
            ? QStringLiteral("app.bsky.graph.getFollows")
            : QStringLiteral("app.bsky.graph.getFollowers");
    const QString arrayKey = following
            ? QStringLiteral("follows") : QStringLiteral("followers");

    QVariantMap params;
    params.insert(QStringLiteral("actor"), m_actor);
    params.insert(QStringLiteral("limit"), 50);
    if (!reset)
        params.insert(QStringLiteral("cursor"), m_cursor);

    QPointer<ActorListModel> guard(this);
    session()->getAuthorized(endpoint, params,
                             [guard, reset, arrayKey](const QJsonObject &data,
                                                      const QString &error) {
        if (!guard)
            return;
        guard->setLoading(false);
        if (!error.isEmpty()) {
            emit guard->errorOccurred(error);
            return;
        }
        guard->m_cursor = data.value(QStringLiteral("cursor")).toString();
        guard->applyPage(reset, parseActors(data.value(arrayKey).toArray()));
    });
}

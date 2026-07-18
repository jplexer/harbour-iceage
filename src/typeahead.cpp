#include "typeahead.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointer>

Typeahead::Typeahead(QObject *parent)
    : QObject(parent)
{
    m_timer.setSingleShot(true);
    m_timer.setInterval(250);
    connect(&m_timer, &QTimer::timeout, this, &Typeahead::fire);
}

void Typeahead::setSession(Session *session)
{
    if (m_session == session)
        return;
    m_session = session;
    emit sessionChanged();
}

void Typeahead::query(const QString &prefix)
{
    m_prefix = prefix;
    m_timer.start();
}

void Typeahead::clear()
{
    m_timer.stop();
    ++m_generation; // invalidates any in-flight response
    if (!m_suggestions.isEmpty()) {
        m_suggestions.clear();
        emit suggestionsChanged();
    }
}

void Typeahead::fire()
{
    if (!m_session || !m_session->loggedIn() || m_prefix.isEmpty())
        return;

    const int generation = ++m_generation;
    QVariantMap params;
    params.insert(QStringLiteral("q"), m_prefix);
    params.insert(QStringLiteral("limit"), 6);

    QPointer<Typeahead> guard(this);
    m_session->getAuthorized(QStringLiteral("app.bsky.actor.searchActorsTypeahead"),
                             params,
                             [guard, generation](const QJsonObject &data,
                                                 const QString &error) {
        if (!guard || guard->m_generation != generation)
            return;
        if (!error.isEmpty()) {
            qWarning() << "typeahead failed:" << error;
            return;
        }
        QVariantList suggestions;
        const QJsonArray actors = data.value(QStringLiteral("actors")).toArray();
        for (const QJsonValue &value : actors) {
            const QJsonObject actor = value.toObject();
            QVariantMap map;
            map.insert(QStringLiteral("handle"),
                       actor.value(QStringLiteral("handle")).toString());
            QString name = actor.value(QStringLiteral("displayName")).toString();
            if (name.isEmpty())
                name = actor.value(QStringLiteral("handle")).toString();
            map.insert(QStringLiteral("displayName"), name);
            map.insert(QStringLiteral("avatar"),
                       actor.value(QStringLiteral("avatar")).toString());
            suggestions.append(map);
        }
        guard->m_suggestions = suggestions;
        emit guard->suggestionsChanged();
    });
}

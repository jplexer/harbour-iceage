#include "notificationsmodel.h"

#include <QJsonArray>
#include <QPointer>

NotificationsModel::NotificationsModel(QObject *parent)
    : QAbstractListModel(parent)
{
    // Battery-friendly cadence; the cover action triggers manual polls
    m_pollTimer.setInterval(10 * 60 * 1000);
    connect(&m_pollTimer, &QTimer::timeout, this, &NotificationsModel::poll);
    m_pollTimer.start();
}

void NotificationsModel::setSession(Session *session)
{
    if (m_session == session)
        return;
    m_session = session;
    emit sessionChanged();
    if (m_session) {
        connect(m_session, &Session::loggedInChanged, this, [this]() {
            if (m_session->loggedIn()) {
                poll();
                refresh(); // populate the cover's notification list
            } else {
                m_lastAnnounced = -1;
                setUnreadCount(0);
                m_cursor.clear();
                if (!m_items.isEmpty()) {
                    beginResetModel();
                    m_items.clear();
                    endResetModel();
                }
            }
        });
    }
}

void NotificationsModel::setLoading(bool loading)
{
    if (m_loading == loading)
        return;
    m_loading = loading;
    emit loadingChanged();
}

void NotificationsModel::setUnreadCount(int count)
{
    if (m_unreadCount == count)
        return;
    m_unreadCount = count;
    emit unreadCountChanged();
}

int NotificationsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_items.count();
}

QVariant NotificationsModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < 0 || index.row() >= m_items.count())
        return QVariant();
    const Item &item = m_items.at(index.row());
    switch (role) {
    case UriRole: return item.uri;
    case ReasonRole: return item.reason;
    case ReasonSubjectRole: return item.reasonSubject;
    case AuthorNameRole: return item.authorName;
    case AuthorHandleRole: return item.authorHandle;
    case AuthorAvatarRole: return item.authorAvatar;
    case TextRole: return item.text;
    case IndexedAtRole: return item.indexedAt;
    case IsReadRole: return item.isRead;
    }
    return QVariant();
}

QHash<int, QByteArray> NotificationsModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles.insert(UriRole, "uri");
    roles.insert(ReasonRole, "reason");
    roles.insert(ReasonSubjectRole, "reasonSubject");
    roles.insert(AuthorNameRole, "authorName");
    roles.insert(AuthorHandleRole, "authorHandle");
    roles.insert(AuthorAvatarRole, "authorAvatar");
    roles.insert(TextRole, "text");
    roles.insert(IndexedAtRole, "indexedAt");
    roles.insert(IsReadRole, "isRead");
    return roles;
}

NotificationsModel::Item NotificationsModel::parseItem(const QJsonObject &json)
{
    Item item;
    const QJsonObject author = json.value(QStringLiteral("author")).toObject();
    item.uri = json.value(QStringLiteral("uri")).toString();
    item.reason = json.value(QStringLiteral("reason")).toString();
    item.reasonSubject = json.value(QStringLiteral("reasonSubject")).toString();
    item.authorHandle = author.value(QStringLiteral("handle")).toString();
    item.authorName = author.value(QStringLiteral("displayName")).toString();
    if (item.authorName.isEmpty())
        item.authorName = item.authorHandle;
    item.authorAvatar = author.value(QStringLiteral("avatar")).toString();
    item.isRead = json.value(QStringLiteral("isRead")).toBool(true);
    item.indexedAt = QDateTime::fromString(
            json.value(QStringLiteral("indexedAt")).toString(), Qt::ISODate);
    if (item.reason == QLatin1String("reply") || item.reason == QLatin1String("mention")
            || item.reason == QLatin1String("quote")) {
        item.text = json.value(QStringLiteral("record")).toObject()
                        .value(QStringLiteral("text")).toString();
    }
    return item;
}

void NotificationsModel::refresh()
{
    fetch(true);
}

void NotificationsModel::loadMore()
{
    if (!m_cursor.isEmpty())
        fetch(false);
}

void NotificationsModel::fetch(bool reset)
{
    if (!m_session || !m_session->loggedIn() || m_loading)
        return;
    setLoading(true);

    QVariantMap params;
    params.insert(QStringLiteral("limit"), 50);
    if (!reset)
        params.insert(QStringLiteral("cursor"), m_cursor);

    QPointer<NotificationsModel> guard(this);
    m_session->getAuthorized(QStringLiteral("app.bsky.notification.listNotifications"), params,
                             [guard, reset](const QJsonObject &data, const QString &error) {
        if (!guard)
            return;
        guard->setLoading(false);
        if (!error.isEmpty()) {
            emit guard->errorOccurred(error);
            return;
        }
        guard->m_cursor = data.value(QStringLiteral("cursor")).toString();

        QList<Item> items;
        const QJsonArray list = data.value(QStringLiteral("notifications")).toArray();
        for (const QJsonValue &value : list)
            items.append(parseItem(value.toObject()));

        if (reset) {
            guard->beginResetModel();
            guard->m_items = items;
            guard->endResetModel();
        } else if (!items.isEmpty()) {
            guard->beginInsertRows(QModelIndex(), guard->m_items.count(),
                                   guard->m_items.count() + items.count() - 1);
            guard->m_items += items;
            guard->endInsertRows();
        }
    });
}

void NotificationsModel::markSeen()
{
    if (!m_session || !m_session->loggedIn())
        return;
    QJsonObject body;
    body.insert(QStringLiteral("seenAt"),
                QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    QPointer<NotificationsModel> guard(this);
    m_session->postAuthorized(QStringLiteral("app.bsky.notification.updateSeen"), body,
                              [guard](const QJsonObject &, const QString &) {
        if (!guard)
            return;
        guard->m_lastAnnounced = 0;
        guard->setUnreadCount(0);
        // isRead flags refresh with the next list fetch
    });
}

void NotificationsModel::poll()
{
    if (!m_session || !m_session->loggedIn())
        return;

    QPointer<NotificationsModel> guard(this);
    m_session->getAuthorized(QStringLiteral("app.bsky.notification.getUnreadCount"),
                             QVariantMap(),
                             [guard](const QJsonObject &data, const QString &error) {
        if (!guard || !error.isEmpty())
            return;
        const int count = data.value(QStringLiteral("count")).toInt();
        const bool changed = count != guard->m_unreadCount;
        guard->setUnreadCount(count);
        if (changed)
            guard->refresh(); // keep the cover's list current while minimized

        if (guard->m_lastAnnounced < 0) {
            // First poll after startup/login: establish the baseline without
            // firing a system notification for old unread items.
            guard->m_lastAnnounced = count;
            return;
        }
        if (count <= guard->m_lastAnnounced) {
            guard->m_lastAnnounced = count;
            return;
        }
        guard->m_lastAnnounced = count;

        // Grab the newest notification so the banner can say who/what
        QVariantMap params;
        params.insert(QStringLiteral("limit"), 1);
        guard->m_session->getAuthorized(
                QStringLiteral("app.bsky.notification.listNotifications"), params,
                [guard, count](const QJsonObject &data, const QString &error) {
            if (!guard)
                return;
            QString name;
            QString reason;
            QString text;
            if (error.isEmpty()) {
                const QJsonArray list = data.value(QStringLiteral("notifications")).toArray();
                if (!list.isEmpty()) {
                    const Item item = parseItem(list.first().toObject());
                    name = item.authorName;
                    reason = item.reason;
                    text = item.text;
                }
            }
            emit guard->alert(count, name, reason, text);
        });
    });
}

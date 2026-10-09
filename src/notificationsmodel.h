#ifndef NOTIFICATIONSMODEL_H
#define NOTIFICATIONSMODEL_H

#include <QAbstractListModel>
#include <QDateTime>
#include <QList>
#include <QTimer>

#include "session.h"

// Bluesky notifications (app.bsky.notification.listNotifications) plus a
// background poll of getUnreadCount every minute while the app runs. When
// the unread count rises above the last announced baseline, alert() fires so
// QML can publish a system notification.
class NotificationsModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(Session *session READ session WRITE setSession NOTIFY sessionChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(int unreadCount READ unreadCount NOTIFY unreadCountChanged)
public:
    enum Role {
        UriRole = Qt::UserRole + 1,
        ReasonRole,        // like, repost, follow, mention, reply, quote
        ReasonSubjectRole, // at-uri of the liked/reposted post, if any
        AuthorNameRole,
        AuthorHandleRole,
        AuthorAvatarRole,
        TextRole, // post text for reply/mention/quote
        IndexedAtRole,
        IsReadRole,
    };

    explicit NotificationsModel(QObject *parent = nullptr);

    Session *session() const { return m_session; }
    void setSession(Session *session);
    bool loading() const { return m_loading; }
    int unreadCount() const { return m_unreadCount; }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void loadMore();
    // Tells the server everything up to now has been seen; resets the badge.
    Q_INVOKABLE void markSeen();
    Q_INVOKABLE void poll();

signals:
    void sessionChanged();
    void loadingChanged();
    void unreadCountChanged();
    void errorOccurred(const QString &message);
    // count new unread; author/reason/text describe the newest notification.
    // target is what tapping it should open: a post at:// uri, or a handle
    // for a profile; empty if unknown.
    void alert(int count, const QString &authorName, const QString &reason,
               const QString &text, const QString &target);

private:
    struct Item {
        QString uri;
        QString reason;
        QString reasonSubject;
        QString authorName;
        QString authorHandle;
        QString authorAvatar;
        QString text;
        QDateTime indexedAt;
        bool isRead = true;
    };

    static Item parseItem(const QJsonObject &json);
    static QString openTarget(const Item &item);
    void fetch(bool reset);
    void setLoading(bool loading);
    void setUnreadCount(int count);

    Session *m_session = nullptr;
    QList<Item> m_items;
    QString m_cursor;
    QTimer m_pollTimer;
    int m_unreadCount = 0;
    int m_lastAnnounced = -1; // -1: baseline not yet established
    bool m_loading = false;
};

#endif // NOTIFICATIONSMODEL_H

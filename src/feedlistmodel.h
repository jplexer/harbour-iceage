#ifndef FEEDLISTMODEL_H
#define FEEDLISTMODEL_H

#include <QAbstractListModel>
#include <QList>

#include "session.h"

// The account's pinned custom feeds: pinned generator URIs from
// app.bsky.actor.getPreferences, resolved to names via
// app.bsky.feed.getFeedGenerators.
class FeedListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(Session *session READ session WRITE setSession NOTIFY sessionChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
public:
    enum Role {
        UriRole = Qt::UserRole + 1,
        NameRole,
        AvatarRole,
    };

    explicit FeedListModel(QObject *parent = nullptr);

    Session *session() const { return m_session; }
    void setSession(Session *session);
    bool loading() const { return m_loading; }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void refresh();

signals:
    void sessionChanged();
    void loadingChanged();
    void errorOccurred(const QString &message);

private:
    struct Feed {
        QString uri;
        QString name;
        QString avatar;
    };

    void setLoading(bool loading);
    void fetchGenerators(const QStringList &uris);

    Session *m_session = nullptr;
    bool m_loading = false;
    QList<Feed> m_feeds;
};

#endif // FEEDLISTMODEL_H

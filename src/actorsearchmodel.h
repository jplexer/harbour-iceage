#ifndef ACTORSEARCHMODEL_H
#define ACTORSEARCHMODEL_H

#include <QAbstractListModel>
#include <QJsonObject>
#include <QList>

#include "session.h"

// People search results (app.bsky.actor.searchActors).
class ActorSearchModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(Session *session READ session WRITE setSession NOTIFY sessionChanged)
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
public:
    enum Role {
        DidRole = Qt::UserRole + 1,
        HandleRole,
        DisplayNameRole,
        AvatarRole,
        DescriptionRole,
        FollowsYouRole,
    };

    explicit ActorSearchModel(QObject *parent = nullptr);

    Session *session() const { return m_session; }
    void setSession(Session *session);
    QString query() const { return m_query; }
    void setQuery(const QString &query);
    bool loading() const { return m_loading; }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void search();
    Q_INVOKABLE void loadMore();

signals:
    void sessionChanged();
    void queryChanged();
    void loadingChanged();
    void errorOccurred(const QString &message);

private:
    struct Actor {
        QString did;
        QString handle;
        QString displayName;
        QString avatar;
        QString description;
        bool followsYou = false;
    };

    static Actor parseActor(const QJsonObject &json);
    void setLoading(bool loading);
    void fetch(bool reset);
    void clear();

    Session *m_session = nullptr;
    QString m_query;
    QString m_cursor;
    bool m_loading = false;
    QList<Actor> m_actors;
};

#endif // ACTORSEARCHMODEL_H

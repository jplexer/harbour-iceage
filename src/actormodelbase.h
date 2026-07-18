#ifndef ACTORMODELBASE_H
#define ACTORMODELBASE_H

#include <QAbstractListModel>
#include <QJsonObject>
#include <QList>

#include "session.h"

// Common base for lists of profiles (search results, followers,
// following): shared roles, parsing and page handling.
class ActorModelBase : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(Session *session READ session WRITE setSession NOTIFY sessionChanged)
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

    explicit ActorModelBase(QObject *parent = nullptr);

    Session *session() const { return m_session; }
    void setSession(Session *session);
    bool loading() const { return m_loading; }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

signals:
    void sessionChanged();
    void loadingChanged();
    void errorOccurred(const QString &message);

protected:
    struct Actor {
        QString did;
        QString handle;
        QString displayName;
        QString avatar;
        QString description;
        bool followsYou = false;
    };

    static Actor parseActor(const QJsonObject &json);
    static QList<Actor> parseActors(const QJsonArray &list);
    void setLoading(bool loading);
    void applyPage(bool reset, const QList<Actor> &actors);
    void clear();

    QList<Actor> m_actors;

private:
    Session *m_session = nullptr;
    bool m_loading = false;
};

#endif // ACTORMODELBASE_H

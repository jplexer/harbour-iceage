#ifndef ACTORLISTMODEL_H
#define ACTORLISTMODEL_H

#include "actormodelbase.h"

// Followers or following of one account (app.bsky.graph.getFollowers /
// getFollows), selected via the mode property.
class ActorListModel : public ActorModelBase
{
    Q_OBJECT
    Q_PROPERTY(QString actor READ actor WRITE setActor NOTIFY actorChanged)
    // "followers" (default) or "following"
    Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY modeChanged)
public:
    explicit ActorListModel(QObject *parent = nullptr);

    QString actor() const { return m_actor; }
    void setActor(const QString &actor);
    QString mode() const { return m_mode; }
    void setMode(const QString &mode);

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void loadMore();

signals:
    void actorChanged();
    void modeChanged();

private:
    void fetch(bool reset);

    QString m_actor;
    QString m_mode = QStringLiteral("followers");
    QString m_cursor;
};

#endif // ACTORLISTMODEL_H

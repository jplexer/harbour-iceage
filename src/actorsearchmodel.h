#ifndef ACTORSEARCHMODEL_H
#define ACTORSEARCHMODEL_H

#include "actormodelbase.h"

// People search results (app.bsky.actor.searchActors).
class ActorSearchModel : public ActorModelBase
{
    Q_OBJECT
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
public:
    explicit ActorSearchModel(QObject *parent = nullptr);

    QString query() const { return m_query; }
    void setQuery(const QString &query);

    Q_INVOKABLE void search();
    Q_INVOKABLE void loadMore();

signals:
    void queryChanged();

private:
    void fetch(bool reset);

    QString m_query;
    QString m_cursor;
};

#endif // ACTORSEARCHMODEL_H

#ifndef SEARCHPOSTSMODEL_H
#define SEARCHPOSTSMODEL_H

#include "abstractpostmodel.h"

// Post search results (app.bsky.feed.searchPosts).
class SearchPostsModel : public AbstractPostModel
{
    Q_OBJECT
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
public:
    explicit SearchPostsModel(QObject *parent = nullptr);

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

#endif // SEARCHPOSTSMODEL_H

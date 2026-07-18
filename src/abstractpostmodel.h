#ifndef ABSTRACTPOSTMODEL_H
#define ABSTRACTPOSTMODEL_H

#include <QAbstractListModel>
#include <QList>

#include "post.h"
#include "session.h"

// Common base for post list models (timeline, thread): shared roles plus
// like/repost toggling with optimistic updates.
class AbstractPostModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(Session *session READ session WRITE setSession NOTIFY sessionChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
public:
    explicit AbstractPostModel(QObject *parent = nullptr);

    Session *session() const { return m_session; }
    void setSession(Session *session);
    bool loading() const { return m_loading; }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void toggleLike(int row);
    Q_INVOKABLE void toggleRepost(int row);
    // Deletes the viewer's own post, optimistically removing the row.
    Q_INVOKABLE void deletePost(int row);

signals:
    void sessionChanged();
    void loadingChanged();
    void errorOccurred(const QString &message);

protected:
    void setLoading(bool loading);
    int rowForUri(const QString &uri) const;
    void notifyRow(int row);

    QList<Post> m_posts;

private:
    // collection: app.bsky.feed.like or app.bsky.feed.repost;
    // recordUri/count point at the Post members for that collection.
    void toggleRecord(int row, const QString &collection,
                      QString Post::*recordUri, int Post::*count);

    Session *m_session = nullptr;
    bool m_loading = false;
};

#endif // ABSTRACTPOSTMODEL_H

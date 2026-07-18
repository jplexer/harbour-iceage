#ifndef THREADMODEL_H
#define THREADMODEL_H

#include "abstractpostmodel.h"

// A post thread (app.bsky.feed.getPostThread), flattened into a list:
// ancestors first, then the main post (isMain), then replies depth-first
// with the depth role for indentation.
class ThreadModel : public AbstractPostModel
{
    Q_OBJECT
    Q_PROPERTY(QString postUri READ postUri WRITE setPostUri NOTIFY postUriChanged)
public:
    explicit ThreadModel(QObject *parent = nullptr);

    QString postUri() const { return m_postUri; }
    void setPostUri(const QString &uri);

    Q_INVOKABLE void load();

signals:
    void postUriChanged();

private:
    void appendReplies(const QJsonObject &threadView, int depth, QList<Post> *out);

    QString m_postUri;
};

#endif // THREADMODEL_H

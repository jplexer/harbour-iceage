#include "abstractpostmodel.h"

#include <QPointer>

namespace {
const QString kPending = QStringLiteral("pending");
}

AbstractPostModel::AbstractPostModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

void AbstractPostModel::setSession(Session *session)
{
    if (m_session == session)
        return;
    m_session = session;
    emit sessionChanged();
}

void AbstractPostModel::setLoading(bool loading)
{
    if (m_loading == loading)
        return;
    m_loading = loading;
    emit loadingChanged();
}

int AbstractPostModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_posts.count();
}

QVariant AbstractPostModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < 0 || index.row() >= m_posts.count())
        return QVariant();
    return PostRoles::data(m_posts.at(index.row()), role);
}

QHash<int, QByteArray> AbstractPostModel::roleNames() const
{
    return PostRoles::roleNames();
}

int AbstractPostModel::rowForUri(const QString &uri) const
{
    for (int i = 0; i < m_posts.count(); ++i) {
        if (m_posts.at(i).uri == uri)
            return i;
    }
    return -1;
}

void AbstractPostModel::notifyRow(int row)
{
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx);
}

void AbstractPostModel::toggleLike(int row)
{
    toggleRecord(row, QStringLiteral("app.bsky.feed.like"), &Post::likeUri, &Post::likeCount);
}

void AbstractPostModel::toggleRepost(int row)
{
    toggleRecord(row, QStringLiteral("app.bsky.feed.repost"), &Post::repostUri,
                 &Post::repostCount);
}

void AbstractPostModel::deletePost(int row)
{
    if (!m_session || !m_session->loggedIn() || row < 0 || row >= m_posts.count())
        return;
    const Post post = m_posts.at(row);
    if (post.authorDid != m_session->did())
        return;

    // Remove optimistically; put the row back if the server refuses
    beginRemoveRows(QModelIndex(), row, row);
    m_posts.removeAt(row);
    endRemoveRows();

    QJsonObject body;
    body.insert(QStringLiteral("repo"), m_session->did());
    body.insert(QStringLiteral("collection"), QStringLiteral("app.bsky.feed.post"));
    body.insert(QStringLiteral("rkey"), post.uri.section(QLatin1Char('/'), -1));

    QPointer<AbstractPostModel> guard(this);
    m_session->postAuthorized(QStringLiteral("com.atproto.repo.deleteRecord"), body,
                              [guard, post, row](const QJsonObject &, const QString &error) {
        if (!guard || error.isEmpty())
            return;
        const int r = qMin(row, guard->m_posts.count());
        guard->beginInsertRows(QModelIndex(), r, r);
        guard->m_posts.insert(r, post);
        guard->endInsertRows();
        emit guard->errorOccurred(error);
    });
}

void AbstractPostModel::toggleRecord(int row, const QString &collection,
                                     QString Post::*recordUri, int Post::*count)
{
    if (!m_session || !m_session->loggedIn() || row < 0 || row >= m_posts.count())
        return;
    Post &post = m_posts[row];
    if (post.*recordUri == kPending)
        return; // request already in flight

    QPointer<AbstractPostModel> guard(this);
    const QString postUri = post.uri;

    if ((post.*recordUri).isEmpty()) {
        // Create like/repost record, optimistically
        post.*recordUri = kPending;
        post.*count += 1;
        notifyRow(row);

        QJsonObject subject;
        subject.insert(QStringLiteral("uri"), post.uri);
        subject.insert(QStringLiteral("cid"), post.cid);
        QJsonObject record;
        record.insert(QStringLiteral("$type"), collection);
        record.insert(QStringLiteral("subject"), subject);
        record.insert(QStringLiteral("createdAt"),
                      QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
        QJsonObject body;
        body.insert(QStringLiteral("repo"), m_session->did());
        body.insert(QStringLiteral("collection"), collection);
        body.insert(QStringLiteral("record"), record);

        m_session->postAuthorized(QStringLiteral("com.atproto.repo.createRecord"), body,
                                  [guard, postUri, recordUri, count](const QJsonObject &data,
                                                                     const QString &error) {
            if (!guard)
                return;
            const int r = guard->rowForUri(postUri);
            if (r < 0)
                return;
            Post &p = guard->m_posts[r];
            if (error.isEmpty()) {
                p.*recordUri = data.value(QStringLiteral("uri")).toString();
            } else {
                p.*recordUri = QString();
                p.*count -= 1;
                emit guard->errorOccurred(error);
            }
            guard->notifyRow(r);
        });
    } else {
        // Delete the existing record; rkey is the tail of the at:// URI
        const QString oldUri = post.*recordUri;
        post.*recordUri = QString();
        post.*count -= 1;
        notifyRow(row);

        QJsonObject body;
        body.insert(QStringLiteral("repo"), m_session->did());
        body.insert(QStringLiteral("collection"), collection);
        body.insert(QStringLiteral("rkey"), oldUri.section(QLatin1Char('/'), -1));

        m_session->postAuthorized(QStringLiteral("com.atproto.repo.deleteRecord"), body,
                                  [guard, postUri, oldUri, recordUri, count](const QJsonObject &,
                                                                             const QString &error) {
            if (!guard || error.isEmpty())
                return;
            const int r = guard->rowForUri(postUri);
            if (r < 0)
                return;
            Post &p = guard->m_posts[r];
            p.*recordUri = oldUri;
            p.*count += 1;
            emit guard->errorOccurred(error);
            guard->notifyRow(r);
        });
    }
}

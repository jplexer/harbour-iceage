#include "threadmodel.h"

#include <QJsonArray>
#include <QPointer>

namespace {
const QString kThreadViewPost = QStringLiteral("app.bsky.feed.defs#threadViewPost");
const int kMaxIndentDepth = 5;
}

ThreadModel::ThreadModel(QObject *parent)
    : AbstractPostModel(parent)
{
}

void ThreadModel::setPostUri(const QString &uri)
{
    if (m_postUri == uri)
        return;
    m_postUri = uri;
    emit postUriChanged();
}

void ThreadModel::load()
{
    if (!session() || !session()->loggedIn() || m_postUri.isEmpty() || loading())
        return;
    setLoading(true);

    QVariantMap params;
    params.insert(QStringLiteral("uri"), m_postUri);
    params.insert(QStringLiteral("depth"), 10);
    params.insert(QStringLiteral("parentHeight"), 40);

    QPointer<ThreadModel> guard(this);
    session()->getAuthorized(QStringLiteral("app.bsky.feed.getPostThread"), params,
                             [guard](const QJsonObject &data, const QString &error) {
        if (!guard)
            return;
        guard->setLoading(false);
        if (!error.isEmpty()) {
            emit guard->errorOccurred(error);
            return;
        }

        const QJsonObject thread = data.value(QStringLiteral("thread")).toObject();
        if (thread.value(QStringLiteral("$type")).toString() != kThreadViewPost) {
            emit guard->errorOccurred(QStringLiteral("NotFound"));
            return;
        }

        QList<Post> posts;

        // Walk the parent chain upwards, then reverse into thread order
        QList<Post> ancestors;
        QJsonObject parent = thread.value(QStringLiteral("parent")).toObject();
        while (parent.value(QStringLiteral("$type")).toString() == kThreadViewPost) {
            ancestors.prepend(PostRoles::parsePostView(
                    parent.value(QStringLiteral("post")).toObject()));
            parent = parent.value(QStringLiteral("parent")).toObject();
        }
        posts += ancestors;

        Post main = PostRoles::parsePostView(thread.value(QStringLiteral("post")).toObject());
        main.isMain = true;
        posts.append(main);

        guard->appendReplies(thread, 1, &posts);

        guard->beginResetModel();
        guard->m_posts = posts;
        guard->endResetModel();
    });
}

void ThreadModel::appendReplies(const QJsonObject &threadView, int depth, QList<Post> *out)
{
    const QJsonArray replies = threadView.value(QStringLiteral("replies")).toArray();
    for (const QJsonValue &value : replies) {
        const QJsonObject reply = value.toObject();
        if (reply.value(QStringLiteral("$type")).toString() != kThreadViewPost)
            continue; // blocked or not-found subtree
        Post post = PostRoles::parsePostView(reply.value(QStringLiteral("post")).toObject());
        post.depth = qMin(depth, kMaxIndentDepth);
        out->append(post);
        appendReplies(reply, depth + 1, out);
    }
}

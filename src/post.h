#ifndef POST_H
#define POST_H

#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

// One post as shown in a feed or thread, parsed from
// app.bsky.feed.defs#postView / #feedViewPost.
struct Post {
    QString uri;
    QString cid;
    QString authorDid;
    QString authorHandle;
    QString authorName;
    QString authorAvatar;
    QString body;
    QString bodyHtml;
    QString repostedBy;
    QString replyToHandle; // parent author when this feed item is a reply
    QVariantList images;
    QVariantMap video;    // playlist (HLS), thumbnail, alt, aspectWidth/Height
    QVariantMap quote;    // embedded quote post: uri, authorName, authorHandle, text
    QVariantMap external; // link card: uri, title, description, thumb
    QDateTime createdAt;
    int replyCount = 0;
    int repostCount = 0;
    int likeCount = 0;
    // Viewer state; non-empty record URI when this account liked/reposted
    QString likeUri;
    QString repostUri;
    // Reply refs for composing a reply to this post
    QString replyRootUri;
    QString replyRootCid;
    // Thread rendering
    int depth = 0;
    bool isMain = false;
};

namespace PostRoles {
enum Role {
    Uri = Qt::UserRole + 1,
    Cid,
    AuthorDid,
    AuthorHandle,
    AuthorName,
    AuthorAvatar,
    Body,
    BodyHtml,
    Images,
    Video,
    Quote,
    External,
    CreatedAt,
    ReplyCount,
    RepostCount,
    LikeCount,
    RepostedBy,
    ReplyToHandle,
    LikeUri,
    RepostUri,
    ReplyRootUri,
    ReplyRootCid,
    Depth,
    IsMain,
};

QHash<int, QByteArray> roleNames();
QVariant data(const Post &post, int role);

// Parses app.bsky.feed.defs#postView
Post parsePostView(const QJsonObject &postView);
// Parses app.bsky.feed.defs#feedViewPost (postView + repost reason)
Post parseFeedItem(const QJsonObject &item);
} // namespace PostRoles

#endif // POST_H

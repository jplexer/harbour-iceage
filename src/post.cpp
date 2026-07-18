#include "post.h"

#include <QJsonArray>

#include <algorithm>

namespace {

// Renders post text plus richtext facets as StyledText HTML. Facet indices
// are byte offsets into the UTF-8 encoding of the text, so all slicing has
// to happen on the utf8 buffer, not on the QString.
QString renderRichText(const QString &text, const QJsonArray &facets)
{
    const QByteArray utf8 = text.toUtf8();

    struct Span {
        int start;
        int end;
        QString href;
    };
    QList<Span> spans;
    for (const QJsonValue &value : facets) {
        const QJsonObject facet = value.toObject();
        const QJsonObject index = facet.value(QStringLiteral("index")).toObject();
        const int start = index.value(QStringLiteral("byteStart")).toInt();
        const int end = index.value(QStringLiteral("byteEnd")).toInt();

        QString href;
        const QJsonArray features = facet.value(QStringLiteral("features")).toArray();
        for (const QJsonValue &fv : features) {
            const QJsonObject feature = fv.toObject();
            const QString type = feature.value(QStringLiteral("$type")).toString();
            if (type == QLatin1String("app.bsky.richtext.facet#link")) {
                href = feature.value(QStringLiteral("uri")).toString();
            } else if (type == QLatin1String("app.bsky.richtext.facet#mention")) {
                href = QStringLiteral("https://bsky.app/profile/")
                        + feature.value(QStringLiteral("did")).toString();
            } else if (type == QLatin1String("app.bsky.richtext.facet#tag")) {
                href = QStringLiteral("https://bsky.app/hashtag/")
                        + feature.value(QStringLiteral("tag")).toString();
            }
        }
        if (!href.isEmpty() && start >= 0 && end > start && end <= utf8.size())
            spans.append(Span{start, end, href});
    }
    std::sort(spans.begin(), spans.end(), [](const Span &a, const Span &b) {
        return a.start < b.start;
    });

    QString html;
    int pos = 0;
    for (const Span &span : spans) {
        if (span.start < pos)
            continue; // overlapping facet, keep the earlier one
        html += QString::fromUtf8(utf8.mid(pos, span.start - pos)).toHtmlEscaped();
        html += QStringLiteral("<a href=\"") + span.href.toHtmlEscaped() + QStringLiteral("\">")
                + QString::fromUtf8(utf8.mid(span.start, span.end - span.start)).toHtmlEscaped()
                + QStringLiteral("</a>");
        pos = span.end;
    }
    html += QString::fromUtf8(utf8.mid(pos)).toHtmlEscaped();
    html.replace(QLatin1String("\n"), QLatin1String("<br>"));
    return html;
}

// Extracts image attachments from app.bsky.embed.images#view, including when
// nested inside a recordWithMedia embed (quote post with images).
QVariantList parseImages(const QJsonObject &post)
{
    const QJsonObject embed = post.value(QStringLiteral("embed")).toObject();
    const QString type = embed.value(QStringLiteral("$type")).toString();

    QJsonArray imageArray;
    if (type == QLatin1String("app.bsky.embed.images#view")) {
        imageArray = embed.value(QStringLiteral("images")).toArray();
    } else if (type == QLatin1String("app.bsky.embed.recordWithMedia#view")) {
        const QJsonObject media = embed.value(QStringLiteral("media")).toObject();
        if (media.value(QStringLiteral("$type")).toString()
                == QLatin1String("app.bsky.embed.images#view"))
            imageArray = media.value(QStringLiteral("images")).toArray();
    }

    QVariantList images;
    for (const QJsonValue &value : imageArray) {
        const QJsonObject image = value.toObject();
        const QJsonObject aspect = image.value(QStringLiteral("aspectRatio")).toObject();
        QVariantMap map;
        map.insert(QStringLiteral("thumb"), image.value(QStringLiteral("thumb")).toString());
        map.insert(QStringLiteral("fullsize"), image.value(QStringLiteral("fullsize")).toString());
        map.insert(QStringLiteral("alt"), image.value(QStringLiteral("alt")).toString());
        map.insert(QStringLiteral("aspectWidth"), aspect.value(QStringLiteral("width")).toInt());
        map.insert(QStringLiteral("aspectHeight"), aspect.value(QStringLiteral("height")).toInt());
        images.append(map);
    }
    return images;
}

// Extracts a video embed (app.bsky.embed.video#view), plain or nested
// inside recordWithMedia. playlist is an HLS (m3u8) stream URL.
QVariantMap parseVideo(const QJsonObject &post)
{
    const QJsonObject embed = post.value(QStringLiteral("embed")).toObject();
    const QString type = embed.value(QStringLiteral("$type")).toString();

    QJsonObject videoView;
    if (type == QLatin1String("app.bsky.embed.video#view")) {
        videoView = embed;
    } else if (type == QLatin1String("app.bsky.embed.recordWithMedia#view")) {
        const QJsonObject media = embed.value(QStringLiteral("media")).toObject();
        if (media.value(QStringLiteral("$type")).toString()
                == QLatin1String("app.bsky.embed.video#view"))
            videoView = media;
    }

    QVariantMap video;
    if (videoView.isEmpty())
        return video;
    const QJsonObject aspect = videoView.value(QStringLiteral("aspectRatio")).toObject();
    video.insert(QStringLiteral("playlist"),
                 videoView.value(QStringLiteral("playlist")).toString());
    video.insert(QStringLiteral("thumbnail"),
                 videoView.value(QStringLiteral("thumbnail")).toString());
    video.insert(QStringLiteral("alt"), videoView.value(QStringLiteral("alt")).toString());
    video.insert(QStringLiteral("aspectWidth"), aspect.value(QStringLiteral("width")).toInt());
    video.insert(QStringLiteral("aspectHeight"),
                 aspect.value(QStringLiteral("height")).toInt());
    return video;
}

// Extracts an embedded quote post (app.bsky.embed.record#viewRecord), plain
// or nested inside recordWithMedia.
QVariantMap parseQuote(const QJsonObject &post)
{
    const QJsonObject embed = post.value(QStringLiteral("embed")).toObject();
    const QString type = embed.value(QStringLiteral("$type")).toString();

    QJsonObject record;
    if (type == QLatin1String("app.bsky.embed.record#view")) {
        record = embed.value(QStringLiteral("record")).toObject();
    } else if (type == QLatin1String("app.bsky.embed.recordWithMedia#view")) {
        record = embed.value(QStringLiteral("record")).toObject()
                     .value(QStringLiteral("record")).toObject();
    }

    QVariantMap quote;
    if (record.value(QStringLiteral("$type")).toString()
            != QLatin1String("app.bsky.embed.record#viewRecord"))
        return quote; // blocked/not-found/detached records render as nothing

    const QJsonObject author = record.value(QStringLiteral("author")).toObject();
    QString name = author.value(QStringLiteral("displayName")).toString();
    if (name.isEmpty())
        name = author.value(QStringLiteral("handle")).toString();
    quote.insert(QStringLiteral("uri"), record.value(QStringLiteral("uri")).toString());
    quote.insert(QStringLiteral("authorName"), name);
    quote.insert(QStringLiteral("authorHandle"),
                 author.value(QStringLiteral("handle")).toString());
    quote.insert(QStringLiteral("text"),
                 record.value(QStringLiteral("value")).toObject()
                       .value(QStringLiteral("text")).toString());
    return quote;
}

// Extracts an external link card (app.bsky.embed.external#view).
QVariantMap parseExternal(const QJsonObject &post)
{
    const QJsonObject embed = post.value(QStringLiteral("embed")).toObject();
    QVariantMap card;
    if (embed.value(QStringLiteral("$type")).toString()
            != QLatin1String("app.bsky.embed.external#view"))
        return card;
    const QJsonObject external = embed.value(QStringLiteral("external")).toObject();
    card.insert(QStringLiteral("uri"), external.value(QStringLiteral("uri")).toString());
    card.insert(QStringLiteral("title"), external.value(QStringLiteral("title")).toString());
    card.insert(QStringLiteral("description"),
                external.value(QStringLiteral("description")).toString());
    card.insert(QStringLiteral("thumb"), external.value(QStringLiteral("thumb")).toString());
    return card;
}

} // namespace

namespace PostRoles {

QHash<int, QByteArray> roleNames()
{
    QHash<int, QByteArray> roles;
    roles.insert(Uri, "uri");
    roles.insert(Cid, "cid");
    roles.insert(AuthorDid, "authorDid");
    roles.insert(AuthorHandle, "authorHandle");
    roles.insert(AuthorName, "authorName");
    roles.insert(AuthorAvatar, "authorAvatar");
    roles.insert(Body, "body");
    roles.insert(BodyHtml, "bodyHtml");
    roles.insert(Images, "images");
    roles.insert(Video, "video");
    roles.insert(Quote, "quote");
    roles.insert(External, "external");
    roles.insert(CreatedAt, "createdAt");
    roles.insert(ReplyCount, "replyCount");
    roles.insert(RepostCount, "repostCount");
    roles.insert(LikeCount, "likeCount");
    roles.insert(RepostedBy, "repostedBy");
    roles.insert(ReplyToHandle, "replyToHandle");
    roles.insert(LikeUri, "likeUri");
    roles.insert(RepostUri, "repostUri");
    roles.insert(ReplyRootUri, "replyRootUri");
    roles.insert(ReplyRootCid, "replyRootCid");
    roles.insert(Depth, "depth");
    roles.insert(IsMain, "isMain");
    return roles;
}

QVariant data(const Post &post, int role)
{
    switch (role) {
    case Uri: return post.uri;
    case Cid: return post.cid;
    case AuthorDid: return post.authorDid;
    case AuthorHandle: return post.authorHandle;
    case AuthorName: return post.authorName;
    case AuthorAvatar: return post.authorAvatar;
    case Body: return post.body;
    case BodyHtml: return post.bodyHtml;
    case Images: return post.images;
    case Video: return post.video;
    case Quote: return post.quote;
    case External: return post.external;
    case CreatedAt: return post.createdAt;
    case ReplyCount: return post.replyCount;
    case RepostCount: return post.repostCount;
    case LikeCount: return post.likeCount;
    case RepostedBy: return post.repostedBy;
    case ReplyToHandle: return post.replyToHandle;
    case LikeUri: return post.likeUri;
    case RepostUri: return post.repostUri;
    case ReplyRootUri: return post.replyRootUri;
    case ReplyRootCid: return post.replyRootCid;
    case Depth: return post.depth;
    case IsMain: return post.isMain;
    }
    return QVariant();
}

Post parsePostView(const QJsonObject &postView)
{
    Post p;
    const QJsonObject author = postView.value(QStringLiteral("author")).toObject();
    const QJsonObject record = postView.value(QStringLiteral("record")).toObject();
    const QJsonObject viewer = postView.value(QStringLiteral("viewer")).toObject();

    p.uri = postView.value(QStringLiteral("uri")).toString();
    p.cid = postView.value(QStringLiteral("cid")).toString();
    p.authorDid = author.value(QStringLiteral("did")).toString();
    p.authorHandle = author.value(QStringLiteral("handle")).toString();
    p.authorName = author.value(QStringLiteral("displayName")).toString();
    if (p.authorName.isEmpty())
        p.authorName = p.authorHandle;
    p.authorAvatar = author.value(QStringLiteral("avatar")).toString();
    p.body = record.value(QStringLiteral("text")).toString();
    p.bodyHtml = renderRichText(p.body, record.value(QStringLiteral("facets")).toArray());
    p.images = parseImages(postView);
    p.video = parseVideo(postView);
    p.quote = parseQuote(postView);
    p.external = parseExternal(postView);
    p.createdAt = QDateTime::fromString(record.value(QStringLiteral("createdAt")).toString(),
                                        Qt::ISODate);
    p.replyCount = postView.value(QStringLiteral("replyCount")).toInt();
    p.repostCount = postView.value(QStringLiteral("repostCount")).toInt();
    p.likeCount = postView.value(QStringLiteral("likeCount")).toInt();
    p.likeUri = viewer.value(QStringLiteral("like")).toString();
    p.repostUri = viewer.value(QStringLiteral("repost")).toString();

    // Root ref for composing replies: this post's own root if it is itself a
    // reply, otherwise the post itself.
    const QJsonObject root = record.value(QStringLiteral("reply")).toObject()
                                   .value(QStringLiteral("root")).toObject();
    p.replyRootUri = root.value(QStringLiteral("uri")).toString(p.uri);
    p.replyRootCid = root.value(QStringLiteral("cid")).toString(p.cid);
    return p;
}

Post parseFeedItem(const QJsonObject &item)
{
    Post p = parsePostView(item.value(QStringLiteral("post")).toObject());

    // Feed-level reply context: who the parent post belongs to
    const QJsonObject parent = item.value(QStringLiteral("reply")).toObject()
                                   .value(QStringLiteral("parent")).toObject();
    p.replyToHandle = parent.value(QStringLiteral("author")).toObject()
                            .value(QStringLiteral("handle")).toString();

    const QJsonObject reason = item.value(QStringLiteral("reason")).toObject();
    if (reason.value(QStringLiteral("$type")).toString()
            == QLatin1String("app.bsky.feed.defs#reasonRepost")) {
        const QJsonObject by = reason.value(QStringLiteral("by")).toObject();
        p.repostedBy = by.value(QStringLiteral("displayName")).toString();
        if (p.repostedBy.isEmpty())
            p.repostedBy = by.value(QStringLiteral("handle")).toString();
    }
    return p;
}

} // namespace PostRoles

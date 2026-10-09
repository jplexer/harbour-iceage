#include "composer.h"

#include <QBuffer>
#include <QDateTime>
#include <QImage>
#include <QJsonArray>
#include <QLocale>
#include <QPointer>
#include <QRegularExpression>
#include <QUrl>

#include <memory>

#include "dpopkey.h"
#include "tagstore.h"

namespace {

// Primary language of the system locale, e.g. "de" for de_DE, so posts
// show up in language-filtered feeds. Empty when there's nothing useful.
QString postLanguage()
{
    const QString lang = QLocale::system().bcp47Name().section(QLatin1Char('-'), 0, 0);
    return lang == QLatin1String("C") ? QString() : lang;
}

int byteOffset(const QString &text, int charOffset)
{
    return text.left(charOffset).toUtf8().size();
}

QJsonObject makeFacet(const QString &text, int charStart, int charEnd,
                      const QString &featureType, const QString &valueKey,
                      const QString &value)
{
    QJsonObject index;
    index.insert(QStringLiteral("byteStart"), byteOffset(text, charStart));
    index.insert(QStringLiteral("byteEnd"), byteOffset(text, charEnd));
    QJsonObject feature;
    feature.insert(QStringLiteral("$type"), featureType);
    feature.insert(valueKey, value);
    QJsonArray features;
    features.append(feature);
    QJsonObject facet;
    facet.insert(QStringLiteral("index"), index);
    facet.insert(QStringLiteral("features"), features);
    return facet;
}

struct MentionSpan {
    int start; // at the '@'
    int end;   // one past the handle
    QString handle;
};

// Handles are domains (alice.bsky.social); require a dot so bare "@word"
// isn't sent off for resolution. Trailing punctuation is trimmed.
QList<MentionSpan> findMentions(const QString &text)
{
    QList<MentionSpan> spans;
    const QRegularExpression re(
            QStringLiteral("(?:^|[\\s(])(@([A-Za-z0-9][A-Za-z0-9.-]*))"));
    QRegularExpressionMatchIterator it = re.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        QString handle = m.captured(2);
        int end = m.capturedEnd(1);
        while (!handle.isEmpty()
               && (handle.endsWith(QLatin1Char('.')) || handle.endsWith(QLatin1Char('-')))) {
            handle.chop(1);
            --end;
        }
        if (!handle.contains(QLatin1Char('.')))
            continue;
        MentionSpan span;
        span.start = m.capturedStart(1);
        span.end = end;
        span.handle = handle.toLower();
        spans.append(span);
    }
    return spans;
}

// Detects links, hashtags and (pre-resolved) mentions, producing richtext
// facets with UTF-8 byte offsets as the lexicon requires.
QJsonArray detectFacets(const QString &text, const QHash<QString, QString> &mentionDids)
{
    QJsonArray facets;

    const QRegularExpression linkRe(QStringLiteral("https?://[^\\s]+"));
    QRegularExpressionMatchIterator links = linkRe.globalMatch(text);
    while (links.hasNext()) {
        const QRegularExpressionMatch m = links.next();
        QString uri = m.captured();
        int end = m.capturedEnd();
        // Trim common trailing punctuation out of the link
        while (!uri.isEmpty() && QStringLiteral(".,;:!?)").contains(uri.at(uri.size() - 1))) {
            uri.chop(1);
            --end;
        }
        if (uri.isEmpty())
            continue;
        facets.append(makeFacet(text, m.capturedStart(), end,
                                QStringLiteral("app.bsky.richtext.facet#link"),
                                QStringLiteral("uri"), uri));
    }

    const QRegularExpression tagRe(QStringLiteral("(?:^|\\s)(#\\w+)"));
    QRegularExpressionMatchIterator tags = tagRe.globalMatch(text);
    while (tags.hasNext()) {
        const QRegularExpressionMatch m = tags.next();
        TagStore::add(m.captured(1).mid(1));
        facets.append(makeFacet(text, m.capturedStart(1), m.capturedEnd(1),
                                QStringLiteral("app.bsky.richtext.facet#tag"),
                                QStringLiteral("tag"), m.captured(1).mid(1)));
    }

    const QList<MentionSpan> mentions = findMentions(text);
    for (const MentionSpan &span : mentions) {
        const QString did = mentionDids.value(span.handle);
        if (did.isEmpty())
            continue; // resolution failed; leave as plain text
        facets.append(makeFacet(text, span.start, span.end,
                                QStringLiteral("app.bsky.richtext.facet#mention"),
                                QStringLiteral("did"), did));
    }

    return facets;
}

// Record key in atproto TID form: microsecond timestamp plus a random
// 10-bit clock id, base32-sortable. Needed up front when the post and its
// threadgate go out in one applyWrites, since they share the rkey.
QString makeTid()
{
    static const char alphabet[] = "234567abcdefghijklmnopqrstuvwxyz";
    static qint64 lastMicros = 0;
    qint64 micros = QDateTime::currentMSecsSinceEpoch() * 1000;
    if (micros <= lastMicros)
        micros = lastMicros + 1;
    lastMicros = micros;

    const QByteArray random = DpopKey::randomBytes(2);
    const quint64 clockId = ((quint8(random.at(0)) << 8) | quint8(random.at(1))) & 0x3FF;
    quint64 value = (quint64(micros) << 10) | clockId;
    QString tid(13, QLatin1Char('2'));
    for (int i = 12; i >= 0; --i) {
        tid[i] = QLatin1Char(alphabet[value & 31]);
        value >>= 5;
    }
    return tid;
}

// threadgate allow rules for a Composer::replyGate value; nobody is an
// empty list
QJsonArray threadgateAllow(const QString &gate)
{
    QString rule;
    if (gate == QLatin1String("following"))
        rule = QStringLiteral("app.bsky.feed.threadgate#followingRule");
    else if (gate == QLatin1String("followers"))
        rule = QStringLiteral("app.bsky.feed.threadgate#followerRule");
    else if (gate == QLatin1String("mentioned"))
        rule = QStringLiteral("app.bsky.feed.threadgate#mentionRule");
    if (rule.isEmpty())
        return QJsonArray();
    QJsonObject entry;
    entry.insert(QStringLiteral("$type"), rule);
    return QJsonArray{entry};
}

} // namespace

Composer::Composer(QObject *parent)
    : QObject(parent)
{
}

void Composer::setSession(Session *session)
{
    if (m_session == session)
        return;
    m_session = session;
    emit sessionChanged();
}

void Composer::setReplyGate(const QString &gate)
{
    if (m_replyGate == gate)
        return;
    m_replyGate = gate;
    emit replyGateChanged();
}

void Composer::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged();
}

void Composer::addImage(const QString &pathOrUrl)
{
    if (m_images.count() >= 4)
        return;
    QString path = pathOrUrl;
    if (path.startsWith(QLatin1String("file:")))
        path = QUrl(path).toLocalFile();
    if (path.isEmpty() || m_images.contains(path))
        return;
    m_images.append(path);
    m_alts.append(QString());
    emit imagesChanged();
}

void Composer::removeImage(int index)
{
    if (index < 0 || index >= m_images.count())
        return;
    m_images.removeAt(index);
    m_alts.removeAt(index);
    emit imagesChanged();
}

void Composer::setImageAlt(int index, const QString &alt)
{
    if (index < 0 || index >= m_alts.count() || m_alts.at(index) == alt)
        return;
    m_alts[index] = alt;
    emit imagesChanged();
}

QStringList Composer::suggestTags(const QString &prefix) const
{
    return TagStore::suggest(prefix);
}

void Composer::post(const QString &text, const QString &replyParentUri,
                    const QString &replyParentCid, const QString &replyRootUri,
                    const QString &replyRootCid, const QString &quoteUri,
                    const QString &quoteCid)
{
    if (!m_session || !m_session->loggedIn() || m_busy)
        return;
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty() && m_images.isEmpty() && quoteUri.isEmpty())
        return;
    setBusy(true);

    ReplyRefs reply;
    reply.parentUri = replyParentUri;
    reply.parentCid = replyParentCid;
    reply.rootUri = replyRootUri;
    reply.rootCid = replyRootCid;
    reply.quoteUri = quoteUri;
    reply.quoteCid = quoteCid;

    // Resolve every unique mentioned handle to a DID first, then post.
    QStringList handles;
    const QList<MentionSpan> mentions = findMentions(trimmed);
    for (const MentionSpan &span : mentions) {
        if (!handles.contains(span.handle))
            handles.append(span.handle);
    }
    if (handles.isEmpty()) {
        uploadImages(trimmed, reply, QHash<QString, QString>(),
                     std::make_shared<QJsonArray>(), 0);
        return;
    }

    auto dids = std::make_shared<QHash<QString, QString> >();
    auto remaining = std::make_shared<int>(handles.count());
    QPointer<Composer> guard(this);
    for (int i = 0; i < handles.count(); ++i) {
        const QString handle = handles.at(i);
        QVariantMap params;
        params.insert(QStringLiteral("handle"), handle);
        m_session->getAuthorized(QStringLiteral("com.atproto.identity.resolveHandle"), params,
                                 [guard, dids, remaining, handle, trimmed, reply](
                                         const QJsonObject &data, const QString &error) {
            if (!guard)
                return;
            if (error.isEmpty()) {
                const QString did = data.value(QStringLiteral("did")).toString();
                if (!did.isEmpty())
                    dids->insert(handle, did);
            }
            if (--(*remaining) == 0)
                guard->uploadImages(trimmed, reply, *dids,
                                    std::make_shared<QJsonArray>(), 0);
        });
    }
}

void Composer::uploadImages(const QString &text, const ReplyRefs &reply,
                            const QHash<QString, QString> &mentionDids,
                            std::shared_ptr<QJsonArray> items, int index)
{
    if (index >= m_images.count()) {
        sendRecord(text, reply, mentionDids, *items);
        return;
    }

    const QString path = m_images.at(index);
    QImage image(path);
    if (image.isNull()) {
        setBusy(false);
        emit postError(tr("Could not read image %1").arg(path));
        return;
    }
    if (image.width() > 2000 || image.height() > 2000)
        image = image.scaled(2000, 2000, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    // Recompress until under the blob size limit (~1MB)
    QByteArray jpeg;
    int quality = 85;
    forever {
        jpeg.clear();
        QBuffer buffer(&jpeg);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "JPEG", quality);
        if (jpeg.size() <= 900 * 1024 || quality <= 40)
            break;
        quality -= 15;
    }

    const int width = image.width();
    const int height = image.height();
    QPointer<Composer> guard(this);
    m_session->uploadBlob(jpeg, QStringLiteral("image/jpeg"),
                          [guard, text, reply, mentionDids, items, index, width, height](
                                  const QJsonObject &data, const QString &error) {
        if (!guard)
            return;
        if (!error.isEmpty()) {
            guard->setBusy(false);
            emit guard->postError(error);
            return;
        }
        QJsonObject aspect;
        aspect.insert(QStringLiteral("width"), width);
        aspect.insert(QStringLiteral("height"), height);
        QJsonObject item;
        item.insert(QStringLiteral("alt"), guard->m_alts.value(index));
        item.insert(QStringLiteral("image"), data.value(QStringLiteral("blob")).toObject());
        item.insert(QStringLiteral("aspectRatio"), aspect);
        items->append(item);
        guard->uploadImages(text, reply, mentionDids, items, index + 1);
    });
}

void Composer::sendRecord(const QString &text, const ReplyRefs &reply,
                          const QHash<QString, QString> &mentionDids,
                          const QJsonArray &imageItems)
{
    QJsonObject record;
    record.insert(QStringLiteral("$type"), QStringLiteral("app.bsky.feed.post"));
    record.insert(QStringLiteral("text"), text);
    record.insert(QStringLiteral("createdAt"),
                  QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    const QJsonArray facets = detectFacets(text, mentionDids);
    if (!facets.isEmpty())
        record.insert(QStringLiteral("facets"), facets);
    const QString lang = postLanguage();
    if (!lang.isEmpty())
        record.insert(QStringLiteral("langs"), QJsonArray{lang});

    if (!reply.quoteUri.isEmpty()) {
        QJsonObject ref;
        ref.insert(QStringLiteral("uri"), reply.quoteUri);
        ref.insert(QStringLiteral("cid"), reply.quoteCid);
        QJsonObject recordEmbed;
        recordEmbed.insert(QStringLiteral("$type"),
                           QStringLiteral("app.bsky.embed.record"));
        recordEmbed.insert(QStringLiteral("record"), ref);
        if (imageItems.isEmpty()) {
            record.insert(QStringLiteral("embed"), recordEmbed);
        } else {
            // Quote plus images combine into recordWithMedia
            QJsonObject media;
            media.insert(QStringLiteral("$type"),
                         QStringLiteral("app.bsky.embed.images"));
            media.insert(QStringLiteral("images"), imageItems);
            QJsonObject combined;
            combined.insert(QStringLiteral("$type"),
                            QStringLiteral("app.bsky.embed.recordWithMedia"));
            combined.insert(QStringLiteral("record"), recordEmbed);
            combined.insert(QStringLiteral("media"), media);
            record.insert(QStringLiteral("embed"), combined);
        }
    } else if (!imageItems.isEmpty()) {
        QJsonObject embed;
        embed.insert(QStringLiteral("$type"), QStringLiteral("app.bsky.embed.images"));
        embed.insert(QStringLiteral("images"), imageItems);
        record.insert(QStringLiteral("embed"), embed);
    }

    if (!reply.parentUri.isEmpty()) {
        QJsonObject parent;
        parent.insert(QStringLiteral("uri"), reply.parentUri);
        parent.insert(QStringLiteral("cid"), reply.parentCid);
        QJsonObject root;
        root.insert(QStringLiteral("uri"),
                    reply.rootUri.isEmpty() ? reply.parentUri : reply.rootUri);
        root.insert(QStringLiteral("cid"),
                    reply.rootCid.isEmpty() ? reply.parentCid : reply.rootCid);
        QJsonObject replyObj;
        replyObj.insert(QStringLiteral("parent"), parent);
        replyObj.insert(QStringLiteral("root"), root);
        record.insert(QStringLiteral("reply"), replyObj);
    }

    QJsonObject body;
    body.insert(QStringLiteral("repo"), m_session->did());
    QString endpoint;
    if (m_replyGate.isEmpty() || !reply.parentUri.isEmpty()) {
        endpoint = QStringLiteral("com.atproto.repo.createRecord");
        body.insert(QStringLiteral("collection"), QStringLiteral("app.bsky.feed.post"));
        body.insert(QStringLiteral("record"), record);
    } else {
        // The threadgate lives at the post's rkey; write both atomically
        // so a post never goes out without its reply restriction
        const QString rkey = makeTid();
        QJsonObject gate;
        gate.insert(QStringLiteral("$type"), QStringLiteral("app.bsky.feed.threadgate"));
        gate.insert(QStringLiteral("post"), QStringLiteral("at://%1/app.bsky.feed.post/%2")
                                                    .arg(m_session->did(), rkey));
        gate.insert(QStringLiteral("allow"), threadgateAllow(m_replyGate));
        gate.insert(QStringLiteral("createdAt"), record.value(QStringLiteral("createdAt")));

        QJsonObject postWrite;
        postWrite.insert(QStringLiteral("$type"),
                         QStringLiteral("com.atproto.repo.applyWrites#create"));
        postWrite.insert(QStringLiteral("collection"), QStringLiteral("app.bsky.feed.post"));
        postWrite.insert(QStringLiteral("rkey"), rkey);
        postWrite.insert(QStringLiteral("value"), record);
        QJsonObject gateWrite;
        gateWrite.insert(QStringLiteral("$type"),
                         QStringLiteral("com.atproto.repo.applyWrites#create"));
        gateWrite.insert(QStringLiteral("collection"),
                         QStringLiteral("app.bsky.feed.threadgate"));
        gateWrite.insert(QStringLiteral("rkey"), rkey);
        gateWrite.insert(QStringLiteral("value"), gate);

        endpoint = QStringLiteral("com.atproto.repo.applyWrites");
        body.insert(QStringLiteral("writes"), QJsonArray{postWrite, gateWrite});
    }

    QPointer<Composer> guard(this);
    m_session->postAuthorized(endpoint, body,
                              [guard](const QJsonObject &, const QString &error) {
        if (!guard)
            return;
        guard->setBusy(false);
        if (error.isEmpty()) {
            if (!guard->m_images.isEmpty()) {
                guard->m_images.clear();
                guard->m_alts.clear();
                emit guard->imagesChanged();
            }
            emit guard->posted();
        } else {
            emit guard->postError(error);
        }
    });
}

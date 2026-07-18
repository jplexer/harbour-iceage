#include "composer.h"

#include <QDateTime>
#include <QJsonArray>
#include <QPointer>
#include <QRegularExpression>

#include <memory>

namespace {

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

void Composer::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged();
}

void Composer::post(const QString &text, const QString &replyParentUri,
                    const QString &replyParentCid, const QString &replyRootUri,
                    const QString &replyRootCid)
{
    if (!m_session || !m_session->loggedIn() || m_busy)
        return;
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return;
    setBusy(true);

    ReplyRefs reply;
    reply.parentUri = replyParentUri;
    reply.parentCid = replyParentCid;
    reply.rootUri = replyRootUri;
    reply.rootCid = replyRootCid;

    // Resolve every unique mentioned handle to a DID first, then post.
    QStringList handles;
    const QList<MentionSpan> mentions = findMentions(trimmed);
    for (const MentionSpan &span : mentions) {
        if (!handles.contains(span.handle))
            handles.append(span.handle);
    }
    if (handles.isEmpty()) {
        sendRecord(trimmed, reply, QHash<QString, QString>());
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
                guard->sendRecord(trimmed, reply, *dids);
        });
    }
}

void Composer::sendRecord(const QString &text, const ReplyRefs &reply,
                          const QHash<QString, QString> &mentionDids)
{
    QJsonObject record;
    record.insert(QStringLiteral("$type"), QStringLiteral("app.bsky.feed.post"));
    record.insert(QStringLiteral("text"), text);
    record.insert(QStringLiteral("createdAt"),
                  QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    const QJsonArray facets = detectFacets(text, mentionDids);
    if (!facets.isEmpty())
        record.insert(QStringLiteral("facets"), facets);

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
    body.insert(QStringLiteral("collection"), QStringLiteral("app.bsky.feed.post"));
    body.insert(QStringLiteral("record"), record);

    QPointer<Composer> guard(this);
    m_session->postAuthorized(QStringLiteral("com.atproto.repo.createRecord"), body,
                              [guard](const QJsonObject &, const QString &error) {
        if (!guard)
            return;
        guard->setBusy(false);
        if (error.isEmpty())
            emit guard->posted();
        else
            emit guard->postError(error);
    });
}

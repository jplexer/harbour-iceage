#ifndef TIMELINEMODEL_H
#define TIMELINEMODEL_H

#include "abstractpostmodel.h"

// The home timeline (app.bsky.feed.getTimeline), or a custom feed
// (app.bsky.feed.getFeed) when feedUri is set. refresh() reloads from
// the top, loadMore() follows the pagination cursor.
class TimelineModel : public AbstractPostModel
{
    Q_OBJECT
    // Empty = the Following timeline
    Q_PROPERTY(QString feedUri READ feedUri WRITE setFeedUri NOTIFY feedUriChanged)
    // Display name of the selected feed, set by the switcher UI
    Q_PROPERTY(QString feedName READ feedName WRITE setFeedName NOTIFY feedNameChanged)
public:
    explicit TimelineModel(QObject *parent = nullptr);

    QString feedUri() const { return m_feedUri; }
    void setFeedUri(const QString &feedUri);
    QString feedName() const { return m_feedName; }
    void setFeedName(const QString &feedName);

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void loadMore();
    // Fetches the newest page and prepends only unseen posts, keeping
    // the scroll position; falls back to a full reset on a gap.
    Q_INVOKABLE void loadNewer();

signals:
    void feedUriChanged();
    void feedNameChanged();

private:
    QString endpoint() const;
    QVariantMap baseParams() const;
    void fetch(bool reset);

    QString m_feedUri;
    QString m_feedName;
    QString m_cursor;
};

#endif // TIMELINEMODEL_H

#ifndef TIMELINEMODEL_H
#define TIMELINEMODEL_H

#include "abstractpostmodel.h"

// The home timeline (app.bsky.feed.getTimeline). refresh() reloads from the
// top, loadMore() follows the pagination cursor.
class TimelineModel : public AbstractPostModel
{
    Q_OBJECT
public:
    explicit TimelineModel(QObject *parent = nullptr);

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void loadMore();
    // Fetches the newest page and prepends only unseen posts, keeping
    // the scroll position; falls back to a full reset on a gap.
    Q_INVOKABLE void loadNewer();

private:
    void fetch(bool reset);

    QString m_cursor;
};

#endif // TIMELINEMODEL_H

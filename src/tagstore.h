#ifndef TAGSTORE_H
#define TAGSTORE_H

#include <QString>
#include <QStringList>

// In-memory pool of hashtags seen in parsed posts (and posted by the
// user), used for composer autocomplete. Bluesky has no tag search
// endpoint, so harvesting what flows through the feeds is the best
// source available.
namespace TagStore {
void add(const QString &tag);
QStringList suggest(const QString &prefix, int max = 6);
}

#endif // TAGSTORE_H

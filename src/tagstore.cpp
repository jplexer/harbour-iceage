#include "tagstore.h"

#include <QHash>

#include <algorithm>

namespace TagStore {

namespace {
// lowercase -> display form (first seen wins)
QHash<QString, QString> s_tags;
}

void add(const QString &tag)
{
    if (tag.isEmpty())
        return;
    const QString key = tag.toLower();
    if (!s_tags.contains(key))
        s_tags.insert(key, tag);
}

QStringList suggest(const QString &prefix, int max)
{
    const QString needle = prefix.toLower();
    QStringList matches;
    for (auto it = s_tags.constBegin(); it != s_tags.constEnd(); ++it) {
        if (it.key().startsWith(needle))
            matches.append(it.value());
    }
    std::sort(matches.begin(), matches.end(),
              [](const QString &a, const QString &b) {
        return a.compare(b, Qt::CaseInsensitive) < 0;
    });
    while (matches.count() > max)
        matches.removeLast();
    return matches;
}

} // namespace TagStore

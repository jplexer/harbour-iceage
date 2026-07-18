#ifndef TYPEAHEAD_H
#define TYPEAHEAD_H

#include <QObject>
#include <QTimer>
#include <QVariantList>

#include "session.h"

// Debounced @mention autocomplete via app.bsky.actor.searchActorsTypeahead.
// suggestions is a list of {handle, displayName, avatar} maps.
class Typeahead : public QObject
{
    Q_OBJECT
    Q_PROPERTY(Session *session READ session WRITE setSession NOTIFY sessionChanged)
    Q_PROPERTY(QVariantList suggestions READ suggestions NOTIFY suggestionsChanged)
public:
    explicit Typeahead(QObject *parent = nullptr);

    Session *session() const { return m_session; }
    void setSession(Session *session);
    QVariantList suggestions() const { return m_suggestions; }

    Q_INVOKABLE void query(const QString &prefix);
    Q_INVOKABLE void clear();

signals:
    void sessionChanged();
    void suggestionsChanged();

private:
    void fire();

    Session *m_session = nullptr;
    QVariantList m_suggestions;
    QTimer m_timer;
    QString m_prefix;
    int m_generation = 0;
};

#endif // TYPEAHEAD_H

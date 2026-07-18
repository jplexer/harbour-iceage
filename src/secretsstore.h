#ifndef SECRETSSTORE_H
#define SECRETSSTORE_H

#include <QByteArray>
#include <QString>

#include <Sailfish/Secrets/secretmanager.h>

// Small synchronous wrapper around Sailfish Secrets for token storage.
// Secrets live in a device-lock protected, encrypted collection.
class SecretsStore
{
public:
    SecretsStore();

    bool store(const QString &name, const QByteArray &value);
    QByteArray load(const QString &name);
    void remove(const QString &name);

private:
    void waitForDaemon();
    void ensureCollection();

    Sailfish::Secrets::SecretManager m_manager;
    bool m_collectionEnsured = false;
};

#endif // SECRETSSTORE_H

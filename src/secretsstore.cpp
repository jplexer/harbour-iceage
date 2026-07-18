#include "secretsstore.h"

#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>

#include <Sailfish/Secrets/createcollectionrequest.h>
#include <Sailfish/Secrets/deletesecretrequest.h>
#include <Sailfish/Secrets/result.h>
#include <Sailfish/Secrets/secret.h>
#include <Sailfish/Secrets/storesecretrequest.h>
#include <Sailfish/Secrets/storedsecretrequest.h>

using namespace Sailfish::Secrets;

namespace {
const QString kCollectionName = QStringLiteral("iceage");

Secret::Identifier identifier(const QString &name)
{
    return Secret::Identifier(name, kCollectionName,
                              SecretManager::DefaultEncryptedStoragePluginName);
}
} // namespace

SecretsStore::SecretsStore()
{
}

void SecretsStore::waitForDaemon()
{
    // SecretManager connects to sailfishsecretsd asynchronously; at app
    // startup (resume during QML instantiation) the socket is usually not
    // up yet, and requests fail with "Not connected to daemon". Pump the
    // event loop briefly until the connection is established.
    if (m_manager.isInitialized())
        return;
    QElapsedTimer timer;
    timer.start();
    while (!m_manager.isInitialized() && timer.elapsed() < 5000)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    if (!m_manager.isInitialized())
        qWarning() << "SecretsStore: daemon connection timed out";
}

void SecretsStore::ensureCollection()
{
    if (m_collectionEnsured)
        return;
    waitForDaemon();

    CreateCollectionRequest request;
    request.setManager(&m_manager);
    request.setCollectionName(kCollectionName);
    request.setAccessControlMode(SecretManager::OwnerOnlyMode);
    request.setCollectionLockType(CreateCollectionRequest::DeviceLock);
    request.setDeviceLockUnlockSemantic(SecretManager::DeviceLockKeepUnlocked);
    request.setStoragePluginName(SecretManager::DefaultEncryptedStoragePluginName);
    request.setEncryptionPluginName(SecretManager::DefaultEncryptedStoragePluginName);
    request.startRequest();
    request.waitForFinished();

    // CollectionAlreadyExistsError is fine -- it was created on a previous run
    if (request.result().code() == Result::Failed
            && request.result().errorCode() != Result::CollectionAlreadyExistsError) {
        qWarning() << "SecretsStore: failed to create collection:"
                   << request.result().errorMessage();
        return;
    }
    m_collectionEnsured = true;
}

bool SecretsStore::store(const QString &name, const QByteArray &value)
{
    ensureCollection();

    // Replace any existing value first; StoreSecretRequest does not overwrite
    remove(name);

    Secret secret(identifier(name));
    secret.setData(value);
    secret.setType(Secret::TypeBlob);

    StoreSecretRequest request;
    request.setManager(&m_manager);
    request.setSecretStorageType(StoreSecretRequest::CollectionSecret);
    request.setUserInteractionMode(SecretManager::SystemInteraction);
    request.setSecret(secret);
    request.startRequest();
    request.waitForFinished();

    if (request.result().code() == Result::Failed) {
        qWarning() << "SecretsStore: failed to store" << name << ":"
                   << request.result().errorMessage();
        return false;
    }
    return true;
}

QByteArray SecretsStore::load(const QString &name)
{
    ensureCollection();

    StoredSecretRequest request;
    request.setManager(&m_manager);
    request.setUserInteractionMode(SecretManager::SystemInteraction);
    request.setIdentifier(identifier(name));
    request.startRequest();
    request.waitForFinished();

    if (request.result().code() == Result::Failed) {
        qWarning() << "SecretsStore: load" << name << "failed:"
                   << request.result().errorCode() << request.result().errorMessage();
        return QByteArray();
    }
    return request.secret().data();
}

void SecretsStore::remove(const QString &name)
{
    waitForDaemon();
    DeleteSecretRequest request;
    request.setManager(&m_manager);
    request.setUserInteractionMode(SecretManager::SystemInteraction);
    request.setIdentifier(identifier(name));
    request.startRequest();
    request.waitForFinished();
    // Failure to delete a nonexistent secret is expected; ignore
}

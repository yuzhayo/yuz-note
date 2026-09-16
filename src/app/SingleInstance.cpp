#include "SingleInstance.h"

#include <QLocalServer>
#include <QLocalSocket>

namespace YuzNote {

SingleInstance::SingleInstance(const QString &key, QObject *parent)
    : QObject(parent)
    , key_(key)
{
}

bool SingleInstance::tryBecomeOwner(const QString &filePath)
{
    QLocalSocket probe;
    probe.connectToServer(key_);
    if (probe.waitForConnected(1500)) {
        probe.write(filePath.toUtf8());
        probe.waitForBytesWritten(1500);
        probe.disconnectFromServer();
        return false; // forwarded; caller exits 0, never shows UI
    }
    QLocalServer::removeServer(key_); // reclaim stale socket, if any
    server_ = new QLocalServer(this);
    connect(server_, &QLocalServer::newConnection, this,
            &SingleInstance::onNewConnection);
    server_->listen(key_); // if this fails we still run (degraded: no handoff)
    if (!server_->isListening()) {
        serverError_ = server_->errorString();
    }
    return true;
}

void SingleInstance::onNewConnection()
{
    if (server_ == nullptr) {
        return;
    }
    while (QLocalSocket *socket = server_->nextPendingConnection()) {
        connect(socket, &QLocalSocket::readyRead, this,
                [this, socket] {
                    const QString path =
                        QString::fromUtf8(socket->readAll()).trimmed();
                    socket->deleteLater();
                    emit fileRequested(path);
                });
        connect(socket, &QLocalSocket::disconnected, socket,
                &QObject::deleteLater);
    }
}

} // namespace YuzNote

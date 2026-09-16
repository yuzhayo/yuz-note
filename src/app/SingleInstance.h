#pragma once

#include <QObject>

class QLocalServer;

namespace YuzNote {

// Single-instance gate (PLAN P4 contract). First starter becomes the owner;
// later starters forward their file path and exit 0 without showing UI.
// Stale sockets are reclaimed. Resolve-once paths: absolute file or empty.
class SingleInstance : public QObject {
    Q_OBJECT
public:
    explicit SingleInstance(const QString &key, QObject *parent = nullptr);

    // True = this process is the owner (run the app). False = forwarded,
    // caller must exit 0 immediately.
    bool tryBecomeOwner(const QString &filePath);

    // Empty when the server listens; otherwise the listen error (degraded mode:
    // app runs without handoff). Surfaces instead of failing silently.
    QString serverError() const { return serverError_; }

signals:
    void fileRequested(const QString &path); // empty = raise only

private slots:
    void onNewConnection();

private:
    QString key_;
    QLocalServer *server_ = nullptr;
    QString serverError_;
};

} // namespace YuzNote

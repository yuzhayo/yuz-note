#pragma once

#include <QObject>
#include <QString>

namespace YuzNote {

// Shell-side boundary over Velopack UpdateManager (PLAN P4). Velopack types
// never appear here (PIMPL); consumers see Qt/std only.
// No timers, no auto-check: user-triggered ops run on fire-and-forget workers;
// results are plain snapshots polled by the GUI. Workers share only
// mutex/atomics — they never touch QObject or this instance.
class UpdateService : public QObject {
    Q_OBJECT
public:
    // Feed = GitHub repo URL (GithubSource). Default placeholder stands until
    // the real remote exists at P5; harmless before then (no releases visit).
    // setFeedUrl only takes effect before the first operation.
    explicit UpdateService(QObject *parent = nullptr);
    void setFeedUrl(const QString &feedUrl);
    ~UpdateService() override;

    bool isSupported() const;
    QString currentVersion() const;

    struct CheckSnapshot {
        bool done = true;
        bool available = false;
        QString version;
        QString error;
    };
    struct DownloadSnapshot {
        bool done = true;
        bool ok = false;
        QString error;
    };

    void check();
    CheckSnapshot checkResult() const;
    bool checkBusy() const;
    void download();
    DownloadSnapshot downloadResult() const;
    bool downloadBusy() const;
    int downloadProgress() const; // 0..100 atomic snapshot, for GUI polling
    void applyAndRestart();       // needs a completed download; quits the app
    QString applyError() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace YuzNote

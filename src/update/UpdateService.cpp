#include "update/UpdateService.h"

#include "version.h"

#include <Velopack.hpp>

#include <QCoreApplication>
#include <QMetaObject>

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>

namespace YuzNote {

// Shared worker state. Owned by shared_ptr: outlives the service if a worker
// is still flying during teardown. Workers touch ONLY this (plus feed copies).
struct UpdateState {
    std::mutex mutex;
    std::optional<Velopack::UpdateInfo> pending;
    bool hasPending = false;
    bool hasDownload = false;
    bool checkDone = true;
    bool checkAvailable = false;
    QString checkVersion;
    QString checkError;
    bool downloadDone = true;
    bool downloadOk = false;
    QString downloadError;
    QString applyError;
    std::atomic<int> progress{0};
    std::atomic<bool> checkBusy{false};
    std::atomic<bool> downloadBusy{false};
};

struct UpdateService::Impl {
    QString feed = QStringLiteral("https://github.com/yuzhayo/yuz-note");
    std::shared_ptr<std::atomic<bool>> alive =
        std::make_shared<std::atomic<bool>>(true);
    std::shared_ptr<UpdateState> state = std::make_shared<UpdateState>();
    bool supported = false;
    QString current = QStringLiteral(YUZNOTE_VERSION);
};

static void ProgressTrampoline(void *userData, size_t percent)
{
    auto *pct = static_cast<std::atomic<int> *>(userData);
    const int v = percent > 100 ? 100 : static_cast<int>(percent);
    pct->store(v, std::memory_order_relaxed);
}

UpdateService::UpdateService(QObject *parent)
    : QObject(parent)
    , impl_(std::make_unique<Impl>())
{
    try {
        auto source =
            std::make_unique<Velopack::GithubSource>(impl_->feed.toStdString(), "", false);
        Velopack::UpdateManager manager(std::move(source));
        if (!manager.GetAppId().empty()) {
            impl_->supported = true;
            const std::string ver = manager.GetCurrentVersion();
            if (!ver.empty()) {
                impl_->current = QString::fromUtf8(ver);
            }
        }
    } catch (...) {
        impl_->supported = false;
    }
}

UpdateService::~UpdateService()
{
    *impl_->alive = false;
}

void UpdateService::setFeedUrl(const QString &feedUrl)
{
    impl_->feed = feedUrl;
}

bool UpdateService::isSupported() const
{
    return impl_->supported;
}

QString UpdateService::currentVersion() const
{
    return impl_->current;
}

void UpdateService::check()
{
    auto state = impl_->state;
    auto alive = impl_->alive;
    const std::string feed = impl_->feed.toStdString();
    state->checkBusy.store(true);
    {
        std::lock_guard<std::mutex> lk(state->mutex);
        state->checkDone = false;
        state->checkAvailable = false;
        state->checkVersion.clear();
        state->checkError.clear();
        state->hasPending = false;
    }
    std::thread([state, alive, feed] {
        bool available = false;
        std::string version;
        std::string error;
        try {
            auto source =
                std::make_unique<Velopack::GithubSource>(feed, "", false);
            Velopack::UpdateManager manager(std::move(source));
            if (auto info = manager.CheckForUpdates()) {
                available = true;
                version = info->TargetFullRelease.Version;
                std::lock_guard<std::mutex> lk(state->mutex);
                state->pending = *info;
                state->hasPending = true;
                state->hasDownload = false;
            }
        } catch (const std::exception &e) {
            error = e.what();
        } catch (...) {
            error = "unknown error";
        }
        if (*alive) {
            std::lock_guard<std::mutex> lk(state->mutex);
            state->checkAvailable = available;
            state->checkVersion = QString::fromUtf8(version);
            state->checkError = QString::fromUtf8(error);
            state->checkDone = true;
        }
        state->checkBusy.store(false);
    }).detach();
}

UpdateService::CheckSnapshot UpdateService::checkResult() const
{
    CheckSnapshot out;
    std::lock_guard<std::mutex> lk(impl_->state->mutex);
    out.done = impl_->state->checkDone;
    out.available = impl_->state->checkAvailable;
    out.version = impl_->state->checkVersion;
    out.error = impl_->state->checkError;
    return out;
}

bool UpdateService::checkBusy() const
{
    return impl_->state->checkBusy.load();
}

void UpdateService::download()
{
    auto state = impl_->state;
    auto alive = impl_->alive;
    const std::string feed = impl_->feed.toStdString();
    Velopack::UpdateInfo info;
    {
        std::lock_guard<std::mutex> lk(state->mutex);
        if (!state->hasPending || !state->pending) {
            return;
        }
        info = *state->pending;
    }
    state->downloadBusy.store(true);
    state->progress.store(0);
    {
        std::lock_guard<std::mutex> lk(state->mutex);
        state->downloadDone = false;
        state->downloadOk = false;
        state->downloadError.clear();
    }
    std::thread([state, alive, feed, info = std::move(info)] {
        bool ok = false;
        std::string error;
        try {
            auto source =
                std::make_unique<Velopack::GithubSource>(feed, "", false);
            Velopack::UpdateManager manager(std::move(source));
            manager.DownloadUpdates(info, &ProgressTrampoline, &state->progress);
            ok = true;
            std::lock_guard<std::mutex> lk(state->mutex);
            state->hasDownload = true;
        } catch (const std::exception &e) {
            error = e.what();
        } catch (...) {
            error = "unknown error";
        }
        if (*alive) {
            std::lock_guard<std::mutex> lk(state->mutex);
            state->downloadOk = ok;
            state->downloadError = QString::fromUtf8(error);
            state->downloadDone = true;
        }
        state->downloadBusy.store(false);
    }).detach();
}

UpdateService::DownloadSnapshot UpdateService::downloadResult() const
{
    DownloadSnapshot out;
    std::lock_guard<std::mutex> lk(impl_->state->mutex);
    out.done = impl_->state->downloadDone;
    out.ok = impl_->state->downloadOk;
    out.error = impl_->state->downloadError;
    return out;
}

bool UpdateService::downloadBusy() const
{
    return impl_->state->downloadBusy.load();
}

int UpdateService::downloadProgress() const
{
    return impl_->state->progress.load();
}

void UpdateService::applyAndRestart()
{
    auto state = impl_->state;
    auto alive = impl_->alive;
    Velopack::UpdateInfo info;
    {
        std::lock_guard<std::mutex> lk(state->mutex);
        if (!state->hasDownload || !state->pending) {
            return;
        }
        info = *state->pending;
    }
    std::thread([state, alive, info = std::move(info)] {
        try {
            auto source =
                std::make_unique<Velopack::GithubSource>("", "", false);
            Velopack::UpdateManager manager(std::move(source));
            manager.WaitExitThenApplyUpdates(info, false, true, {});
            QMetaObject::invokeMethod(
                QCoreApplication::instance(), [] { QCoreApplication::quit(); },
                Qt::QueuedConnection);
        } catch (const std::exception &e) {
            if (*alive) {
                std::lock_guard<std::mutex> lk(state->mutex);
                state->applyError = QString::fromUtf8(e.what());
            }
        } catch (...) {
            if (*alive) {
                std::lock_guard<std::mutex> lk(state->mutex);
                state->applyError = QStringLiteral("unknown error");
            }
        }
    }).detach();
}

QString UpdateService::applyError() const
{
    std::lock_guard<std::mutex> lk(impl_->state->mutex);
    return impl_->state->applyError;
}

} // namespace YuzNote

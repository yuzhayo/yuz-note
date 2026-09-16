#include "MainWindow.h"

#include "viewer/MarkdownView.h"
#include "FindBar.h"
#include "update/UpdateService.h"
#include "version.h"

#include <QAction>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressDialog>
#include <QSettings>
#include <QShortcut>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>

namespace YuzNote {
namespace {

constexpr int kMaxRecent = 8;
const char *kRecentKey = "recentFiles";

bool IsMarkdownFile(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    return suffix == QStringLiteral("md") || suffix == QStringLiteral("markdown");
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , baseTitle_(QStringLiteral("yuz-note %1 · Qt %2")
                     .arg(QStringLiteral(YUZNOTE_VERSION),
                          QString::fromLatin1(qVersion())))
{
    setWindowTitle(baseTitle_);
    setAcceptDrops(true);

    view_ = new MarkdownView(this);
    setCentralWidget(view_);

    auto *findToolBar = new QToolBar(this);
    findToolBar->setMovable(false);
    findToolBar->setFloatable(false);
    findBar_ = new FindBar(view_, this);
    findToolBar->addWidget(findBar_);
    findToolBar->hide();
    addToolBar(Qt::TopToolBarArea, findToolBar);
    connect(findBar_, &FindBar::closed, findToolBar, &QToolBar::hide);

    auto *fileMenu = menuBar()->addMenu(tr("&File"));
    auto *openAction =
        fileMenu->addAction(tr("&Open…"), this, &MainWindow::onOpenFile);
    openAction->setShortcut(QKeySequence::Open);
    recentMenu_ = fileMenu->addMenu(tr("Open &Recent"));
    connect(recentMenu_, &QMenu::aboutToShow, this, &MainWindow::onRecentShown);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("E&xit"), this, &QWidget::close);

    auto *helpMenu = menuBar()->addMenu(tr("&Help"));
    checkUpdatesAction_ = helpMenu->addAction(tr("Check for &Updates"), this,
                                              &MainWindow::onCheckUpdates);
    helpMenu->addAction(tr("&About yuz-note"), this, &MainWindow::onAbout);

    update_ = new UpdateService(this);
    checkUpdatesAction_->setEnabled(update_->isSupported());
    if (!update_->isSupported()) {
        checkUpdatesAction_->setToolTip(
            tr("Updates are available after yuz-note is installed."));
    }
    updatePoll_ = new QTimer(this);
    updatePoll_->setInterval(250);
    connect(updatePoll_, &QTimer::timeout, this, &MainWindow::onPollUpdate);

    auto *findShortcut =
        new QShortcut(QKeySequence::Find, this, nullptr, nullptr,
                      Qt::ApplicationShortcut);
    connect(findShortcut, &QShortcut::activated, this, &MainWindow::onToggleFind);
    auto *zoomInA = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Equal), this);
    connect(zoomInA, &QShortcut::activated, view_,
            [this] { view_->zoomStep(1); });
    auto *zoomInB = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Plus), this);
    connect(zoomInB, &QShortcut::activated, view_,
            [this] { view_->zoomStep(1); });
    auto *zoomOutA = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Minus), this);
    connect(zoomOutA, &QShortcut::activated, view_,
            [this] { view_->zoomStep(-1); });
    auto *zoomZero = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_0), this);
    connect(zoomZero, &QShortcut::activated, view_, [this] { view_->zoomReset(); });

    QSettings settings;
    recents_ = settings.value(QLatin1String(kRecentKey)).toStringList();

    statusBar()->showMessage(tr("Ready — open a .md file."), 4000);
    connect(view_, &MarkdownView::linkRejected, this,
            [this](const QString &url) {
                statusBar()->showMessage(tr("Blocked link: %1").arg(url), 8000);
            });
    connect(view_, &MarkdownView::codeBlockCopied, this, [this] {
        statusBar()->showMessage(tr("Code block copied."), 4000);
    });
}

bool MainWindow::loadFile(const QString &path)
{
    QString error;
    if (!view_->openMarkdownFile(path, &error)) {
        statusBar()->showMessage(tr("Failed to open %1 (%2)").arg(path, error),
                                 8000);
        return false;
    }
    setWindowTitle(baseTitle_ + QStringLiteral(" — ")
                   + QFileInfo(path).fileName());
    statusBar()->showMessage(
        tr("Loaded %1 (%2 blocks)").arg(path).arg(view_->document()->blockCount()),
        5000);
    pushRecent(path);
    return true;
}

void MainWindow::showStatus(const QString &message, int timeoutMs)
{
    statusBar()->showMessage(message, timeoutMs);
}

void MainWindow::openAndRaise(const QString &path)
{
    if (!path.isEmpty()) {
        loadFile(path);
    }
    showNormal();
    raise();
    activateWindow();
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData() != nullptr && event->mimeData()->hasUrls()) {
        for (const QUrl &url : event->mimeData()->urls()) {
            if (url.isLocalFile() && IsMarkdownFile(url.toLocalFile())) {
                event->acceptProposedAction();
                return;
            }
        }
    }
    QMainWindow::dragEnterEvent(event);
}

void MainWindow::dropEvent(QDropEvent *event)
{
    if (event->mimeData() != nullptr) {
        for (const QUrl &url : event->mimeData()->urls()) {
            if (url.isLocalFile() && IsMarkdownFile(url.toLocalFile())) {
                loadFile(url.toLocalFile());
                event->acceptProposedAction();
                return;
            }
        }
    }
    QMainWindow::dropEvent(event);
}

void MainWindow::onOpenFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open Markdown"), QString(),
        tr("Markdown (*.md *.markdown);;All files (*.*)"));
    if (!path.isEmpty()) {
        loadFile(path);
    }
}

void MainWindow::onRecentShown()
{
    rebuildRecentMenu();
}

void MainWindow::onOpenRecent()
{
    auto *action = qobject_cast<QAction *>(sender());
    if (action != nullptr) {
        loadFile(action->data().toString());
    }
}

void MainWindow::onToggleFind()
{
    if (findBar_->isVisible()) {
        findBar_->hide();
    } else {
        findBar_->activate();
    }
}

void MainWindow::rebuildRecentMenu()
{
    recentMenu_->clear();
    int shown = 0;
    for (const QString &path : recents_) {
        if (shown >= kMaxRecent) {
            break;
        }
        if (!QFileInfo::exists(path)) {
            continue;
        }
        QAction *action = recentMenu_->addAction(QFileInfo(path).fileName());
        action->setData(path);
        action->setToolTip(path);
        connect(action, &QAction::triggered, this, &MainWindow::onOpenRecent);
        ++shown;
    }
    recentMenu_->setEnabled(shown > 0);
}

void MainWindow::pushRecent(const QString &path)
{
    recents_.removeAll(path);
    recents_.prepend(path);
    while (recents_.size() > kMaxRecent) {
        recents_.removeLast();
    }
    QSettings settings;
    settings.setValue(QLatin1String(kRecentKey), recents_);
}

void MainWindow::onCheckUpdates()
{
    if (update_ == nullptr || !update_->isSupported()) {
        return;
    }
    checkUpdatesAction_->setEnabled(false);
    statusBar()->showMessage(tr("Checking for updates…"));
    update_->check();
    pollMode_ = UpdatePollMode::Check;
    updatePoll_->start();
}

void MainWindow::onPollUpdate()
{
    if (update_ == nullptr) {
        updatePoll_->stop();
        return;
    }
    if (pollMode_ == UpdatePollMode::Check) {
        const UpdateService::CheckSnapshot r = update_->checkResult();
        if (!r.done) {
            return;
        }
        updatePoll_->stop();
        pollMode_ = UpdatePollMode::None;
        checkUpdatesAction_->setEnabled(true);
        if (!r.error.isEmpty()) {
            statusBar()->showMessage(tr("Update check failed."), 8000);
            QMessageBox::warning(this, tr("Updates"),
                                 tr("Update check failed:\n%1").arg(r.error));
        } else if (!r.available) {
            statusBar()->showMessage(
                tr("Up to date (%1).").arg(update_->currentVersion()), 5000);
            QMessageBox::information(
                this, tr("Updates"),
                tr("yuz-note is up to date (%1).").arg(update_->currentVersion()));
        } else {
            statusBar()->showMessage(tr("Version %1 is available.").arg(r.version),
                                     8000);
            const auto answer = QMessageBox::question(
                this, tr("Updates"),
                tr("Version %1 is available.\nDownload and restart?").arg(r.version));
            if (answer == QMessageBox::Yes) {
                startUpdateDownload(r.version);
            }
        }
        return;
    }
    if (pollMode_ == UpdatePollMode::Download) {
        if (progress_ != nullptr) {
            progress_->setValue(update_->downloadProgress());
        }
        const UpdateService::DownloadSnapshot r = update_->downloadResult();
        if (!r.done) {
            return;
        }
        updatePoll_->stop();
        pollMode_ = UpdatePollMode::None;
        if (progress_ != nullptr) {
            progress_->close();
            progress_->deleteLater();
            progress_ = nullptr;
        }
        if (!r.ok) {
            statusBar()->showMessage(tr("Update failed."), 8000);
            QMessageBox::warning(this, tr("Updates"),
                                 tr("Update failed:\n%1").arg(r.error));
            return;
        }
        statusBar()->showMessage(tr("Restarting to apply update…"));
        update_->applyAndRestart();
        const QString err = update_->applyError();
        if (!err.isEmpty()) {
            statusBar()->showMessage(tr("Restart failed: %1").arg(err), 8000);
        }
    }
}

void MainWindow::startUpdateDownload(const QString &version)
{
    progress_ = new QProgressDialog(
        tr("Downloading update %1…").arg(version), QString(), 0, 100, this);
    progress_->setCancelButton(nullptr);
    progress_->setWindowModality(Qt::WindowModal);
    progress_->setMinimumDuration(0);
    progress_->show();
    update_->download();
    pollMode_ = UpdatePollMode::Download;
    updatePoll_->start();
}

void MainWindow::onAbout()
{
    QMessageBox::about(
        this, tr("About yuz-note"),
        tr("yuz-note %1\nMarkdown opener (Qt %2)\nUpdates via Velopack.")
            .arg(update_->currentVersion(), QString::fromLatin1(qVersion())));
}

} // namespace YuzNote

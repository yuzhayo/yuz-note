#pragma once

#include <QMainWindow>
#include <QStringList>

class QAction;
class QMenu;
class QProgressDialog;
class QTimer;

namespace YuzNote {

class FindBar;
class MarkdownView;
class UpdateService;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

    // Loads a .md file into the viewer. Returns false + status message on
    // failure; never throws.
    bool loadFile(const QString &path);

    void showStatus(const QString &message, int timeoutMs = 8000);

    // Single-instance handoff: load (if any) + bring to front.
    void openAndRaise(const QString &path);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void onOpenFile();
    void onRecentShown();
    void onOpenRecent();
    void onToggleFind();
    void onCheckUpdates();
    void onPollUpdate();
    void onAbout();

private:
    void rebuildRecentMenu();
    void pushRecent(const QString &path);
    void startUpdateDownload(const QString &version);

    enum class UpdatePollMode { None, Check, Download };

    QString baseTitle_;
    MarkdownView *view_ = nullptr;
    FindBar *findBar_ = nullptr;
    UpdateService *update_ = nullptr;
    QMenu *recentMenu_ = nullptr;
    QAction *checkUpdatesAction_ = nullptr;
    QTimer *updatePoll_ = nullptr;
    UpdatePollMode pollMode_ = UpdatePollMode::None;
    QProgressDialog *progress_ = nullptr;
    QStringList recents_;
};

} // namespace YuzNote

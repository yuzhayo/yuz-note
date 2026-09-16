#pragma once

#include <QMap>
#include <QTextBlock>
#include <QTextBrowser>

namespace YuzNote {

struct MarkdownResult;

// P3 viewer: QTextBrowser over P2 HTML with locked policies (PLAN §3a/§3b).
// openLinks=false forever; loadResource deny-all; anchorClicked router;
// <details> toggle via block visibility; read-only (undo disabled).
// P4 additions: relative zoom, code-block ranges + copy, context menu.
class MarkdownView : public QTextBrowser {
    Q_OBJECT
public:
    explicit MarkdownView(QWidget *parent = nullptr);

    bool openMarkdownFile(const QString &path, QString *error = nullptr);
    bool setMarkdownText(const QString &markdown, QString *error = nullptr);

    // Toggles a yuz-details range by id ("details-N"). False when unknown.
    // Public for the scripted gate (tests/viewer-smoke); UI path is anchors.
    bool toggleDetails(const QString &id);

    // Code ranges: maximal runs of non-breakable (pre) blocks. Pure queries
    // (also power the scripted gate); UI copy path is the context menu.
    int codeBlockCount() const;
    QString codeBlockText(int index) const;

    // Relative zoom steps (symmetric: zoomReset() exactly undoes zoomStep()).
    void zoomStep(int steps);
    void zoomReset();

signals:
    void linkRejected(const QString &url);
    void codeBlockCopied();

protected:
    QVariant loadResource(int type, const QUrl &name) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private slots:
    void onAnchorClicked(const QUrl &url);

private:
    struct DetailsRange {
        QTextBlock summary;
        QTextBlock firstBody;
        QTextBlock afterLast; // sentinel (hidden forever, never content)
        bool open = true;
    };
    bool applyResult(const MarkdownResult &r, QString *error);
    void rebuildDetailsMap();
    void applyInitialDetailsStates();
    QList<QList<QTextBlock>> codeRuns() const;
    int codeBlockIndexAt(const QPoint &pos) const;
    void copyCodeBlock(int index);

    QMap<QString, DetailsRange> details_;
    int zoomSteps_ = 0;
};

} // namespace YuzNote

#include "viewer/MarkdownView.h"

#include "viewer/ViewerTheme.h"
#include "markdown/MarkdownParse.h"

#include <QClipboard>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QMenu>
#include <QMouseEvent>
#include <QPalette>
#include <QTextCursor>

namespace YuzNote {
namespace {

const QChar kMarkerClosed(0x25B8); // BLACK RIGHT-POINTING SMALL TRIANGLE
const QChar kMarkerOpen(0x25BE);   // BLACK DOWN-POINTING SMALL TRIANGLE
const QString kDetailsScheme = QStringLiteral("yuz-details:");

// Presentation fixes the parser must not own (P2 stays semantic):
// - Qt needs explicit table border attrs (CSS borders unreliable).
// - Code language labels become a separate small paragraph ABOVE each block,
//   so P4 copy-code copies pure code only.
QString PrepareDocumentHtml(const QString &html)
{
    QString out = html;
    out.replace(QStringLiteral("<table>"),
                QStringLiteral(
                    "<table border=\"1\" cellpadding=\"4\" cellspacing=\"0\">"));
    QString result;
    result.reserve(out.size());
    int pos = 0;
    while (pos < out.size()) {
        const int pre = out.indexOf(QStringLiteral("<pre><code"), pos);
        if (pre < 0) {
            result += out.mid(pos);
            break;
        }
        result += out.mid(pos, pre - pos);
        const int tagEnd = out.indexOf('>', pre);
        if (tagEnd < 0) {
            result += out.mid(pre);
            break;
        }
        const QString tag = out.mid(pre, tagEnd - pre + 1);
        QString lang = QStringLiteral("code");
        const QString marker = QStringLiteral("class=\"language-");
        const int m = tag.indexOf(marker);
        if (m >= 0) {
            const int s = m + marker.size();
            const int e = tag.indexOf('"', s);
            if (e > s) {
                lang = tag.mid(s, e - s);
            }
        }
        result += QStringLiteral("<p><font color=\"#7d8590\" size=\"-1\">")
            + lang.toHtmlEscaped() + QStringLiteral("</font></p>");
        result += QStringLiteral("<pre><code>");
        pos = tagEnd + 1;
    }
    return result;
}

} // namespace

MarkdownView::MarkdownView(QWidget *parent)
    : QTextBrowser(parent)
{
    setOpenLinks(false); // PLAN §3a: all clicks routed by anchorClicked
    document()->setUndoRedoEnabled(false); // marker edits must not pollute undo
    document()->setDefaultStyleSheet(Theme::DefaultStyleSheet());
    QPalette pal = palette();
    pal.setColor(QPalette::Base, QColor(QString::fromLatin1(Theme::kPageBackground)));
    pal.setColor(QPalette::Text, QColor(QString::fromLatin1(Theme::kText)));
    setPalette(pal);
    connect(this, &QTextBrowser::anchorClicked, this,
            &MarkdownView::onAnchorClicked);
}

bool MarkdownView::openMarkdownFile(const QString &path, QString *error)
{
    const MarkdownResult r =
        RenderMarkdownFile(path.toUtf8().toStdString());
    return applyResult(r, error);
}

bool MarkdownView::setMarkdownText(const QString &markdown, QString *error)
{
    const MarkdownResult r =
        RenderMarkdownBytes(markdown.toUtf8().toStdString());
    return applyResult(r, error);
}

bool MarkdownView::toggleDetails(const QString &id)
{
    const auto it = details_.find(id);
    if (it == details_.end()) {
        return false;
    }
    DetailsRange &r = it.value();
    r.open = !r.open;
    for (QTextBlock b = r.firstBody; b.isValid() && !(b == r.afterLast);
         b = b.next()) {
        b.setVisible(r.open);
    }
    QTextCursor c(r.summary);
    c.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
    c.insertText(r.open ? QString(kMarkerOpen) : QString(kMarkerClosed));
    return true;
}

QVariant MarkdownView::loadResource(int /*type*/, const QUrl & /*name*/)
{
    return {}; // deny-all: no file/network loads, ever (PLAN §3a)
}

void MarkdownView::mouseMoveEvent(QMouseEvent *event)
{
    QTextBrowser::mouseMoveEvent(event);
    // Deterministic hand cursor (no reliance on openLinks-hover default).
    const QTextCursor c = cursorForPosition(event->pos());
    const QTextCharFormat f = c.charFormat();
    const bool link = f.isAnchor() && f.hasProperty(QTextFormat::AnchorHref)
        && !f.anchorHref().isEmpty();
    viewport()->setCursor(link ? Qt::PointingHandCursor : Qt::IBeamCursor);
}

void MarkdownView::onAnchorClicked(const QUrl &url)
{
    const QString s = url.toString();
    if (s.startsWith(kDetailsScheme)) {
        toggleDetails(s.mid(kDetailsScheme.size()));
        return;
    }
    if (url.scheme().isEmpty()) {
        if (!url.fragment().isEmpty()) {
            scrollToAnchor(url.fragment()); // internal #anchor
            return;
        }
        emit linkRejected(s);
        return;
    }
    const QString scheme = url.scheme().toLower();
    if (scheme == QStringLiteral("http") || scheme == QStringLiteral("https")
        || scheme == QStringLiteral("mailto")) {
        QDesktopServices::openUrl(url);
        return;
    }
    emit linkRejected(s); // file: and everything else
}

bool MarkdownView::applyResult(const MarkdownResult &r, QString *error)
{
    if (!r.ok) {
        if (error != nullptr) {
            *error = QString::fromUtf8(r.error);
        }
        return false;
    }
    setHtml(PrepareDocumentHtml(QString::fromUtf8(r.html)));
    rebuildDetailsMap();
    return true;
}

void MarkdownView::rebuildDetailsMap()
{
    details_.clear();
    QTextDocument *doc = document();
    QMap<QString, QTextBlock> anchors;
    for (QTextBlock b = doc->begin(); b.isValid(); b = b.next()) {
        for (QTextBlock::iterator it = b.begin(); !it.atEnd(); ++it) {
            // Qt 6 stores <a name> as AnchorName QStringList (anchorNames(),
            // plural — there is no singular anchorName() in Qt 6.8).
            const QStringList names =
                it.fragment().charFormat().anchorNames();
            for (const QString &name : names) {
                if (!name.isEmpty()) {
                    anchors.insert(name, b);
                }
            }
        }
    }
    for (auto it = anchors.cbegin(); it != anchors.cend(); ++it) {
        const QString &name = it.key();
        if (!name.startsWith(QStringLiteral("details-"))
            || name.endsWith(QStringLiteral("-end"))) {
            continue;
        }
        const auto jt = anchors.find(name + QStringLiteral("-end"));
        if (jt == anchors.end()) {
            continue;
        }
        DetailsRange r;
        r.summary = it.value();
        r.firstBody = r.summary.next();
        r.afterLast = jt.value();
        r.open = true;
        QTextBlock sentinel = r.afterLast;
        sentinel.setVisible(false); // metadata, never content
        details_.insert(name, r);
    }
    applyInitialDetailsStates();
}

void MarkdownView::applyInitialDetailsStates()
{
    const QList<QString> ids = details_.keys();
    for (const QString &id : ids) {
        DetailsRange &r = details_[id];
        // P2 emits the marker as state signal: ▸ starts closed, ▾ starts open.
        const bool closed =
            !r.summary.text().isEmpty() && r.summary.text()[0] == kMarkerClosed;
        r.open = !closed;
        if (closed) {
            for (QTextBlock b = r.firstBody; b.isValid() && !(b == r.afterLast);
                 b = b.next()) {
                b.setVisible(false);
            }
        }
    }
}

void MarkdownView::zoomStep(int steps)
{
    if (steps == 0) {
        return;
    }
    zoomSteps_ += steps;
    if (steps > 0) {
        zoomIn(steps);
    } else {
        zoomOut(-steps);
    }
}

void MarkdownView::zoomReset()
{
    if (zoomSteps_ > 0) {
        zoomOut(zoomSteps_);
    } else if (zoomSteps_ < 0) {
        zoomIn(-zoomSteps_);
    }
    zoomSteps_ = 0;
}

void MarkdownView::wheelEvent(QWheelEvent *event)
{
    if ((event->modifiers() & Qt::ControlModifier) != 0) {
        const int steps = event->angleDelta().y() / 120;
        zoomStep(steps);
        event->accept();
        return;
    }
    QTextBrowser::wheelEvent(event);
}

QList<QList<QTextBlock>> MarkdownView::codeRuns() const
{
    // Relies on Qt marking <pre> blocks non-breakable (verified by
    // viewer-smoke; fallback if ever red: sentinel anchors like details).
    QList<QList<QTextBlock>> runs;
    QList<QTextBlock> current;
    for (QTextBlock b = document()->begin(); b.isValid(); b = b.next()) {
        if (b.blockFormat().nonBreakableLines() && b.length() > 1) {
            current.append(b);
        } else if (!current.isEmpty()) {
            runs.append(current);
            current.clear();
        }
    }
    if (!current.isEmpty()) {
        runs.append(current);
    }
    return runs;
}

int MarkdownView::codeBlockCount() const
{
    return codeRuns().size();
}

QString MarkdownView::codeBlockText(int index) const
{
    const QList<QList<QTextBlock>> runs = codeRuns();
    if (index < 0 || index >= runs.size()) {
        return {};
    }
    QStringList lines;
    for (const QTextBlock &b : runs.at(index)) {
        lines.append(b.text());
    }
    return lines.join(QLatin1Char('\n'));
}

int MarkdownView::codeBlockIndexAt(const QPoint &pos) const
{
    const QTextBlock hit = cursorForPosition(pos).block();
    const QList<QList<QTextBlock>> runs = codeRuns();
    for (int i = 0; i < runs.size(); ++i) {
        for (const QTextBlock &b : runs.at(i)) {
            if (b == hit) {
                return i;
            }
        }
    }
    return -1;
}

void MarkdownView::copyCodeBlock(int index)
{
    const QString text = codeBlockText(index);
    if (text.isEmpty()) {
        return;
    }
    QGuiApplication::clipboard()->setText(text);
    emit codeBlockCopied();
}

void MarkdownView::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu *menu = createStandardContextMenu();
    const int index = codeBlockIndexAt(event->pos());
    if (index >= 0) {
        menu->addSeparator();
        QAction *copyCode = menu->addAction(tr("Copy code block"));
        connect(copyCode, &QAction::triggered, this,
                [this, index] { copyCodeBlock(index); });
    }
    menu->exec(event->globalPos());
    delete menu;
}

} // namespace YuzNote

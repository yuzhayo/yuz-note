// viewer-smoke — scripted P3 gate, no eyes needed. Run with
// QT_QPA_PLATFORM=offscreen (set below as well, belt and suspenders).
// Usage: viewer-smoke <corpus-dir>  -> exit 0 all-green, 1 on any failure.
#include "viewer/MarkdownView.h"

#include <QApplication>
#include <QTextBlock>
#include <QTextDocument>

#include <cstdio>

namespace {

int failures = 0;

#define SMOKE_CHECK(cond, msg)                                                 \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::fprintf(stderr, "SMOKE-FAIL %s\n", msg);                      \
            ++failures;                                                        \
        } else {                                                               \
            std::fprintf(stdout, "SMOKE-PASS %s\n", msg);                      \
        }                                                                      \
    } while (0)

int VisibleBlocks(YuzNote::MarkdownView &view)
{
    int n = 0;
    for (QTextBlock b = view.document()->begin(); b.isValid(); b = b.next()) {
        if (b.isVisible()) {
            ++n;
        }
    }
    return n;
}

// Kept permanently: structural dump on failure (anchor/block forensics).
void DumpBlocks(YuzNote::MarkdownView &view)
{
    int i = 0;
    for (QTextBlock b = view.document()->begin(); b.isValid();
         b = b.next(), ++i) {
        std::string names;
        for (QTextBlock::iterator it = b.begin(); !it.atEnd(); ++it) {
            const QStringList ns = it.fragment().charFormat().anchorNames();
            for (const QString &nm : ns) {
                names += "[" + nm.toStdString() + "]";
            }
        }
        std::fprintf(stderr, "BLOCK %d vis=%d text=%.80s anchors=%s\n", i,
                     b.isVisible() ? 1 : 0, b.text().toUtf8().constData(),
                     names.c_str());
    }
}

} // namespace

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    if (argc < 2) {
        std::fprintf(stderr, "usage: viewer-smoke <corpus-dir>\n");
        return 3;
    }
    const QString dir = QString::fromLocal8Bit(argv[1]);
    YuzNote::MarkdownView view; // never shown; flags work headless
    QString error;

    SMOKE_CHECK(view.openMarkdownFile(dir + QStringLiteral("/details-edge.md"),
                                       &error),
                "edge loads");
    // details-1 has no `open` attr: starts COLLAPSED, first toggle OPENS it.
    const int v0 = VisibleBlocks(view);
    SMOKE_CHECK(view.toggleDetails(QStringLiteral("details-1")), "toggle works");
    const int v1 = VisibleBlocks(view);
    SMOKE_CHECK(v1 > v0, "toggle opens collapsed body");
    SMOKE_CHECK(view.toggleDetails(QStringLiteral("details-1")), "toggle back works");
    SMOKE_CHECK(VisibleBlocks(view) == v0, "round-trip restores");
    SMOKE_CHECK(!view.toggleDetails(QStringLiteral("details-2")),
                "flattened inner has no range");
    SMOKE_CHECK(!view.toggleDetails(QStringLiteral("nope")), "unknown id rejected");
    // details.md block 2 HAS the `open` attr: starts OPEN, first toggle HIDES it.
    SMOKE_CHECK(view.openMarkdownFile(dir + QStringLiteral("/details.md"), &error),
                "details loads");
    const int w0 = VisibleBlocks(view);
    SMOKE_CHECK(view.toggleDetails(QStringLiteral("details-2")), "toggle open block");
    SMOKE_CHECK(VisibleBlocks(view) < w0, "toggle hides open body");
    SMOKE_CHECK(view.toggleDetails(QStringLiteral("details-2")), "toggle back again");
    SMOKE_CHECK(VisibleBlocks(view) == w0, "round-trip restores again");
    SMOKE_CHECK(view.openMarkdownFile(dir + QStringLiteral("/INSTALL.md"), &error),
                "install loads");
    SMOKE_CHECK(view.document()->blockCount() > 20, "install has substance");
    SMOKE_CHECK(view.openMarkdownFile(dir + QStringLiteral("/images.md"), &error),
                "images loads");
    SMOKE_CHECK(view.openMarkdownFile(dir + QStringLiteral("/code.md"), &error),
                "code loads");
    SMOKE_CHECK(view.codeBlockCount() >= 1, "code blocks detected");
    SMOKE_CHECK(view.codeBlockText(0).contains(QStringLiteral("mkdir")),
                "code text readable");
    SMOKE_CHECK(view.openMarkdownFile(dir + QStringLiteral("/headings.md"), &error)
                    && view.codeBlockCount() == 0,
                "no false positives");

    std::fprintf(stdout, failures == 0 ? "SMOKE-OK\n" : "SMOKE-FAILED\n");
    if (failures != 0) {
        DumpBlocks(view);
    }
    return failures == 0 ? 0 : 1;
}

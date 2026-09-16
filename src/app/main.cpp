// yuz-note — Markdown opener.
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QMessageBox>
#include <QTimer>

#include "MainWindow.h"
#include "MarkdownAssociation.h"
#include "SingleInstance.h"
#include "Velopack.hpp"
#include "version.h"

namespace {

void ShowUsage(const QString &title, const QString &text)
{
    QMessageBox::information(nullptr, title, text);
}

const QString kUsage = QStringLiteral(
    "yuz-note [options] [file.md]\n\n"
    "Options:\n"
    "  --bench    print cold-start milliseconds to first paint and keep running\n"
    "  --help     show this message\n"
    "  --version  show the version");

} // namespace

int main(int argc, char *argv[])
{
    // FIRST line: Velopack lifecycle (install/update/uninstall hooks).
    // May exit/restart the process; nothing Qt, single-instance, or UI before this.
    Velopack::VelopackApp::Build()
        .SetAutoApplyOnStartup(false)
        .OnAfterInstall(YuzNote::RegisterMarkdownAssociation)
        .OnAfterUpdate(YuzNote::RegisterMarkdownAssociation)
        .OnBeforeUninstall(YuzNote::UnregisterMarkdownAssociation)
        .Run();

    QElapsedTimer benchTimer;
    benchTimer.start();

    bool bench = false;
    bool help = false;
    bool showVersion = false;
    QString unknownFlag;
    QString file;
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == QStringLiteral("--bench")) {
            bench = true;
        } else if (arg == QStringLiteral("--help")) {
            help = true;
        } else if (arg == QStringLiteral("--version")) {
            showVersion = true;
        } else if (arg.startsWith(QLatin1Char('-')) && unknownFlag.isEmpty()) {
            unknownFlag = arg;
        } else if (file.isEmpty()) {
            file = arg;
        }
    }

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("yuz-note"));
    app.setOrganizationName(QStringLiteral("Yuzhayo"));

    if (help) {
        ShowUsage(QStringLiteral("yuz-note usage"), kUsage);
        return 0;
    }
    if (showVersion) {
        ShowUsage(QStringLiteral("yuz-note version"),
                  QStringLiteral("yuz-note %1 (Qt %2)")
                      .arg(QStringLiteral(YUZNOTE_VERSION),
                           QString::fromLatin1(qVersion())));
        return 0;
    }
    if (!unknownFlag.isEmpty()) {
        ShowUsage(QStringLiteral("yuz-note usage"),
                  QStringLiteral("Unknown flag: %1\n\n%2").arg(unknownFlag, kUsage));
        return 1;
    }

    YuzNote::SingleInstance single(QStringLiteral("Yuzhayo.YuzNote.Instance"));
    if (!single.tryBecomeOwner(file)) {
        return 0; // forwarded to the owner; never show UI here
    }

    YuzNote::MainWindow window;
    window.resize(900, 650);
    if (!single.serverError().isEmpty()) {
        window.showStatus(QStringLiteral("Single instance unavailable: ")
                          + single.serverError());
    }
    QObject::connect(&single, &YuzNote::SingleInstance::fileRequested, &window,
                     &YuzNote::MainWindow::openAndRaise);
    if (!file.isEmpty()) {
        window.loadFile(file);
    }
    window.show();

    if (bench) {
        QTimer::singleShot(0, [&benchTimer] {
            const long long ms = static_cast<long long>(benchTimer.elapsed());
            QFile f(QDir::temp().absoluteFilePath(
                QStringLiteral("yuz-note-bench-ms.txt")));
            if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                f.write(QByteArray::number(ms));
            }
        });
    }
    return app.exec();
}

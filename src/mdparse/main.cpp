// mdparse — P2 pipeline runner.
// Usage: mdparse <file.md>  -> stdout: HTML, stderr: warnings/errors.
// Exit: 0 ok · 1 unreadable/validate · 2 render · 3 usage.
#include "markdown/MarkdownParse.h"

#include <cstdio>
#include <string>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

int main(int argc, char *argv[])
{
    if (argc != 2) {
        std::fprintf(stderr, "usage: mdparse <file.md>\n");
        return 3;
    }
#ifdef _WIN32
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    const YuzNote::MarkdownResult r = YuzNote::RenderMarkdownFile(argv[1]);
    if (!r.ok) {
        std::fprintf(stderr, "error: %s\n", r.error.c_str());
        const bool validate =
            r.error == "file-unreadable" || r.error.rfind("source-", 0) == 0;
        return validate ? 1 : 2;
    }
    std::fwrite(r.html.data(), 1, r.html.size(), stdout);
    for (const auto &w : r.warnings) {
        std::fprintf(stderr, "warning: %s\n", w.c_str());
    }
    return 0;
}

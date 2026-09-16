#include "MarkdownParse.h"

#include "MarkdownDetails.h"
#include "MarkdownSanitize.h"

#include <cmark-gfm-extension_api.h>
#include <cmark-gfm-core-extensions.h>
#include <cmark-gfm.h>

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <memory>
#include <mutex>
#include <sstream>
#include <utility>
#include <vector>

namespace YuzNote {
namespace {

// ---- Validation (ports reference/01-parse.md, validate stage) ----

// Unsigned byte reader (avoids narrowing casts entirely).
bool BomEquals(const std::string &s, std::size_t off, std::initializer_list<unsigned> bs)
{
    std::size_t i = 0;
    for (const unsigned b : bs) {
        if (off + i >= s.size()
            || static_cast<unsigned char>(s[off + i]) != b) {
            return false;
        }
        ++i;
    }
    return true;
}

bool IsValidUtf8(const std::string &s)
{
    std::size_t i = 0;
    const std::size_t n = s.size();
    while (i < n) {
        const auto c = static_cast<unsigned char>(s[i]);
        if (c < 0x80) {
            ++i;
            continue;
        }
        uint32_t cp = 0;
        std::size_t need = 0;
        uint32_t floor = 0;
        if (c >= 0xC2 && c <= 0xDF) {
            cp = c & 0x1F;
            need = 1;
            floor = 0x80;
        } else if (c >= 0xE0 && c <= 0xEF) {
            cp = c & 0x0F;
            need = 2;
            floor = 0x800;
        } else if (c >= 0xF0 && c <= 0xF4) {
            cp = c & 0x07;
            need = 3;
            floor = 0x10000;
        } else {
            return false; // C0/C1 overlong, F5+ out of range, stray continuation
        }
        if (i + need >= n) {
            return false; // truncated
        }
        for (std::size_t k = 1; k <= need; ++k) {
            const auto d = static_cast<unsigned char>(s[i + k]);
            if ((d & 0xC0) != 0x80) {
                return false;
            }
            cp = (cp << 6) | (d & 0x3F);
        }
        if (cp < floor || cp > 0x10FFFF) {
            return false; // overlong or out of range
        }
        if (cp >= 0xD800 && cp <= 0xDFFF) {
            return false; // surrogate
        }
        i += 1 + need;
    }
    return true;
}

bool LooksBinary(const std::string &s)
{
    std::size_t ctl = 0;
    for (const auto ch : s) {
        const auto c = static_cast<unsigned char>(ch);
        if (c == 0x00) {
            return true;
        }
        if ((c < 0x20 && c != '\t' && c != '\n' && c != '\r') || c == 0x7F) {
            ++ctl;
        }
    }
    return !s.empty() && ctl * 10 > s.size();
}

std::string NormalizeSource(std::string s)
{
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size();) {
        if (s[i] == '\r') {
            out.push_back('\n');
            i += (i + 1 < s.size() && s[i + 1] == '\n') ? 2 : 1;
        } else {
            out.push_back(s[i++]);
        }
    }
    return out;
}

// ---- cmark segment render ----

constexpr int kCmarkOptions = CMARK_OPT_DEFAULT | CMARK_OPT_UNSAFE
    | CMARK_OPT_FOOTNOTES | CMARK_OPT_STRIKETHROUGH_DOUBLE_TILDE;

void EnsureExtensionsRegistered()
{
    static std::once_flag flag;
    std::call_once(flag, [] { cmark_gfm_core_extensions_ensure_registered(); });
}

bool AttachExtensions(cmark_parser *parser, cmark_mem *mem, cmark_llist **exts,
                      std::string &missing)
{
    static const char *const kExts[] = {
        "table", "strikethrough", "autolink", "tagfilter", "tasklist",
    };
    for (const char *name : kExts) {
        cmark_syntax_extension *ext = cmark_find_syntax_extension(name);
        if (ext == nullptr || !cmark_parser_attach_syntax_extension(parser, ext)) {
            missing = name;
            return false;
        }
        *exts = cmark_llist_append(mem, *exts, ext);
    }
    return true;
}

// Iterative tree walk: no recursion, so hostile nesting cannot overflow our stack.
bool CheckLimits(cmark_node *root, const MarkdownLimits &limits, std::string &error)
{
    struct Frame {
        cmark_node *node;
        int depth;
    };
    std::vector<Frame> stack;
    stack.reserve(1024);
    stack.push_back({root, 0});
    int count = 0;
    while (!stack.empty()) {
        const Frame f = stack.back();
        stack.pop_back();
        if (++count > limits.maxNodes) {
            error = "too-many-nodes";
            return false;
        }
        if (f.depth > limits.maxNesting) {
            error = "nesting-too-deep";
            return false;
        }
        for (cmark_node *c = cmark_node_first_child(f.node); c != nullptr;
             c = cmark_node_next(c)) {
            stack.push_back({c, f.depth + 1});
        }
    }
    return true;
}

} // namespace

ValidatedSource ValidateBytes(const std::string &bytes, const MarkdownLimits &limits)
{
    ValidatedSource v;
    if (bytes.size() > limits.maxSourceBytes) {
        v.error = "source-too-large";
        return v;
    }
    if (BomEquals(bytes, 0, {0x00, 0x00, 0xFE, 0xFF})
        || BomEquals(bytes, 0, {0xFF, 0xFE, 0x00, 0x00})
        || BomEquals(bytes, 0, {0xFE, 0xFF})
        || BomEquals(bytes, 0, {0xFF, 0xFE})) {
        v.error = "source-unsupported-bom";
        return v;
    }
    std::string s = bytes;
    if (BomEquals(s, 0, {0xEF, 0xBB, 0xBF})) {
        s.erase(0, 3);
    }
    if (LooksBinary(s)) {
        v.error = "source-binary";
        return v;
    }
    if (!IsValidUtf8(s)) {
        v.error = "source-invalid-utf8";
        return v;
    }
    v.ok = true;
    v.text = NormalizeSource(s);
    return v;
}

bool RenderMarkdownSegment(const std::string &md, const MarkdownLimits &limits,
                           std::string &htmlOut, std::string &error)
{
    EnsureExtensionsRegistered();
    cmark_parser *parser = cmark_parser_new(kCmarkOptions);
    if (parser == nullptr) {
        error = "cmark-parser-failed";
        return false;
    }
    cmark_mem *mem = cmark_get_default_mem_allocator();
    cmark_llist *exts = nullptr;
    std::string missing;
    if (!AttachExtensions(parser, mem, &exts, missing)) {
        if (exts != nullptr) {
            cmark_llist_free(mem, exts);
        }
        cmark_parser_free(parser);
        error = std::string("cmark-extension-missing:") + missing;
        return false;
    }
    cmark_parser_feed(parser, md.data(), md.size());
    cmark_node *root = cmark_parser_finish(parser);
    cmark_parser_free(parser);
    if (root == nullptr) {
        cmark_llist_free(mem, exts);
        error = "cmark-parser-failed";
        return false;
    }
    const std::unique_ptr<cmark_node, decltype(&cmark_node_free)> guard(
        root, &cmark_node_free);
    if (!CheckLimits(root, limits, error)) {
        cmark_llist_free(mem, exts);
        return false;
    }
    char *out = cmark_render_html(root, kCmarkOptions, exts);
    cmark_llist_free(mem, exts);
    if (out == nullptr) {
        error = "cmark-render-failed";
        return false;
    }
    htmlOut.assign(out);
    free(out);
    return true;
}

MarkdownResult RenderMarkdownBytes(const std::string &bytes,
                                   const MarkdownLimits &limits)
{
    MarkdownResult r;
    const ValidatedSource v = ValidateBytes(bytes, limits);
    if (!v.ok) {
        r.error = v.error;
        return r;
    }
    DetailsContext ctx;
    ctx.nextId = 1;
    ctx.warnings = &r.warnings;
    ctx.limits = &limits;
    std::string expandError;
    const std::string html = ExpandDetails(v.text, ctx, expandError);
    if (!expandError.empty()) {
        r.error = expandError;
        return r;
    }
    r.html = SanitizeHtml(html);
    r.ok = true;
    return r;
}

MarkdownResult RenderMarkdownFile(const std::string &path,
                                  const MarkdownLimits &limits)
{
    MarkdownResult r;
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec || size > limits.maxSourceBytes) {
        r.error = ec ? "file-unreadable" : "source-too-large";
        return r;
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        r.error = "file-unreadable";
        return r;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    if (in.bad()) {
        r.error = "file-unreadable";
        return r;
    }
    return RenderMarkdownBytes(ss.str(), limits);
}

} // namespace YuzNote

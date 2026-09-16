#include "MarkdownDetails.h"

#include <cctype>
#include <cstring>

namespace YuzNote {
namespace {

std::string TrimLeft(const std::string &s)
{
    std::size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) {
        ++i;
    }
    return s.substr(i);
}

std::string ToLower(std::string s)
{
    for (auto &c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

// "<tag" followed by end/space/tab/>// (case-insensitive, leading ws allowed).
bool StartsTag(const std::string &line, const char *tag)
{
    const std::string t = ToLower(TrimLeft(line));
    const std::size_t n = std::strlen(tag);
    if (t.size() < 1 + n || t[0] != '<' || t.compare(1, n, tag) != 0) {
        return false;
    }
    if (t.size() == 1 + n) {
        return true;
    }
    const char c = t[1 + n];
    return c == ' ' || c == '\t' || c == '>' || c == '/';
}

bool IsOpener(const std::string &line)
{
    return StartsTag(line, "details");
}

bool IsCloser(const std::string &line)
{
    return StartsTag(line, "/details");
}

bool IsFence(const std::string &line)
{
    const std::string t = TrimLeft(line);
    return t.rfind("```", 0) == 0 || t.rfind("~~~", 0) == 0;
}

// Single-line <summary>...</summary> (case-insensitive); inner in source case.
bool ExtractSummary(const std::string &line, std::string &inner)
{
    const std::string low = ToLower(line);
    const std::size_t a = low.find("<summary");
    if (a == std::string::npos) {
        return false;
    }
    const std::size_t ag = low.find('>', a);
    if (ag == std::string::npos) {
        return false;
    }
    const std::size_t b = low.find("</summary>", ag);
    if (b == std::string::npos) {
        return false;
    }
    inner = line.substr(ag + 1, b - ag - 1);
    return true;
}

std::string EscapeHtmlText(std::string s)
{
    std::string out;
    out.reserve(s.size());
    for (const char c : s) {
        switch (c) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        default: out.push_back(c); break;
        }
    }
    return out;
}

std::vector<std::string> SplitLines(const std::string &s)
{
    std::vector<std::string> v;
    std::size_t start = 0;
    for (;;) {
        const std::size_t p = s.find('\n', start);
        if (p == std::string::npos) {
            v.push_back(s.substr(start));
            return v;
        }
        v.push_back(s.substr(start, p - start));
        start = p + 1;
    }
}

std::string JoinLines(const std::vector<std::string> &v, std::size_t from,
                      std::size_t till)
{
    std::string s;
    for (std::size_t i = from; i < till && i < v.size(); ++i) {
        s += v[i];
        s.push_back('\n');
    }
    return s;
}

// The rendered fragment unwrapped from a single surrounding <p>...</p>.
std::string StripSingleParagraph(const std::string &html)
{
    static const std::string kOpen = "<p>";
    static const std::string kClose = "</p>\n";
    if (html.size() > kOpen.size() + kClose.size()
        && html.compare(0, kOpen.size(), kOpen) == 0
        && html.compare(html.size() - kClose.size(), kClose.size(), kClose) == 0
        && html.find("<p>", kOpen.size()) == std::string::npos) {
        return html.substr(kOpen.size(),
                           html.size() - kOpen.size() - kClose.size());
    }
    return html;
}

} // namespace

std::string ExpandDetails(const std::string &source, DetailsContext &ctx,
                          std::string &error)
{
    const std::vector<std::string> lines = SplitLines(source);
    const std::size_t n = lines.size();
    std::string mdBuf; // plain markdown, flushed at block boundaries
    std::string out;   // final HTML

    auto flush = [&]() -> bool {
        if (mdBuf.empty()) {
            return true;
        }
        std::string html, err;
        if (!RenderMarkdownSegment(mdBuf, *ctx.limits, html, err)) {
            error = err;
            return false;
        }
        out += html;
        mdBuf.clear();
        return true;
    };

    static const std::string kMarker = "\xE2\x96\xB8 "; // U+25B8 closed
    static const std::string kMarkerOpen = "\xE2\x96\xBE "; // U+25BE open

    std::size_t i = 0;
    bool inFence = false;
    while (i < n) {
        const std::string &line = lines[i];
        if (!inFence && IsFence(line)) {
            inFence = true;
        } else if (inFence && IsFence(line)) {
            inFence = false;
        }
        if (inFence || !IsOpener(line)) {
            mdBuf += line;
            mdBuf.push_back('\n');
            ++i;
            continue;
        }
        // Opener at i. `open` attribute: standalone word, case-insensitive.
        bool initiallyOpen = false;
        {
            const std::string head = ToLower(TrimLeft(line));
            const std::size_t gt = head.find('>');
            const std::string tagHead = head.substr(0, gt);
            std::size_t p = 0;
            while (p < tagHead.size()) {
                while (p < tagHead.size()
                       && (tagHead[p] == ' ' || tagHead[p] == '\t')) {
                    ++p;
                }
                std::size_t q = p;
                while (q < tagHead.size() && tagHead[q] != ' '
                       && tagHead[q] != '\t' && tagHead[q] != '/') {
                    ++q;
                }
                if (tagHead.substr(p, q - p) == "open") {
                    initiallyOpen = true;
                    break;
                }
                p = q;
            }
        }
        // Opener at i. Same-line summary is the common GitHub form.
        std::string sameLineInner;
        const bool sameLineSummary = ExtractSummary(line, sameLineInner);
        int depth = 1;
        std::size_t j = i + 1;
        std::size_t summaryIdx = sameLineSummary ? i : std::string::npos;
        std::string summaryInner = sameLineInner;
        bool bodyFence = false;
        for (; j < n; ++j) {
            if (IsFence(lines[j])) {
                bodyFence = !bodyFence;
                continue;
            }
            if (bodyFence) {
                continue;
            }
            if (IsOpener(lines[j])) {
                ++depth;
            } else if (IsCloser(lines[j])) {
                if (--depth == 0) {
                    break;
                }
            } else if (depth == 1 && summaryIdx == std::string::npos) {
                std::string inner;
                if (ExtractSummary(lines[j], inner)) {
                    summaryIdx = j;
                    summaryInner = inner;
                }
            }
        }
        if (j >= n) {
            ctx.warnings->push_back("unclosed-details");
            mdBuf += line;
            mdBuf.push_back('\n');
            ++i;
            continue;
        }
        if (summaryIdx == std::string::npos) {
            ctx.warnings->push_back("details-without-summary");
            for (std::size_t k = i; k <= j; ++k) {
                mdBuf += lines[k];
                mdBuf.push_back('\n');
            }
            i = j + 1;
            continue;
        }
        if (!flush()) {
            return out;
        }
        std::string sumHtml, sumErr;
        if (!RenderMarkdownSegment(summaryInner, *ctx.limits, sumHtml, sumErr)) {
            error = sumErr;
            return out;
        }
        const int id = ctx.nextId++;
        out += "<p><a name=\"details-" + std::to_string(id)
            + "\" href=\"yuz-details:details-" + std::to_string(id) + "\">"
            + (initiallyOpen ? kMarkerOpen : kMarker)
            + StripSingleParagraph(sumHtml) + "</a></p>\n";
        const std::size_t bodyFrom = (summaryIdx == i) ? i + 1 : summaryIdx + 1;
        const std::string body = JoinLines(lines, bodyFrom, j);
        if (ctx.depth + 1 > ctx.limits->maxDetailsDepth) {
            ctx.warnings->push_back("details-depth-capped");
            std::string capped, capErr;
            if (!RenderMarkdownSegment(EscapeHtmlText(body), *ctx.limits, capped,
                                       capErr)) {
                error = capErr;
                return out;
            }
            out += capped;
        } else {
            std::vector<std::string> blines = SplitLines(body);
            bool nested = false;
            bool fence = false;
            for (auto &bl : blines) {
                if (IsFence(bl)) {
                    fence = !fence;
                    continue;
                }
                if (fence) {
                    continue;
                }
                std::string tmp;
                if (IsOpener(bl) || IsCloser(bl) || ExtractSummary(bl, tmp)) {
                    bl = EscapeHtmlText(bl);
                    nested = true;
                }
            }
            if (nested) {
                ctx.warnings->push_back("nested-details-flattened");
            }
            ++ctx.depth;
            std::string subErr;
            std::string bodyHtml = ExpandDetails(JoinLines(blines, 0, blines.size()),
                                                 ctx, subErr);
            --ctx.depth;
            if (!subErr.empty()) {
                error = subErr;
                return out;
            }
            out += bodyHtml;
        }
        out += "<p><a name=\"details-" + std::to_string(id) + "-end\"></a></p>\n";
        i = j + 1;
    }
    if (!flush()) {
        return out;
    }
    return out;
}

} // namespace YuzNote

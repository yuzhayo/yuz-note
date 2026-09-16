#include "MarkdownSanitize.h"

#include <cctype>

namespace YuzNote {
namespace {

bool TagOpenAt(const std::string &s, std::size_t pos)
{
    if (pos + 4 > s.size() || s[pos] != '<') {
        return false;
    }
    auto ci = [](char a, char b) {
        return std::tolower(static_cast<unsigned char>(a)) == b;
    };
    if (!(ci(s[pos + 1], 'i') && ci(s[pos + 2], 'm') && ci(s[pos + 3], 'g'))) {
        return false;
    }
    if (pos + 4 == s.size()) {
        return true;
    }
    const char c = s[pos + 4];
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '>' || c == '/';
}

// End of the tag opened at pos ('<'), honouring single/double quotes.
// npos when unclosed.
std::size_t TagEnd(const std::string &s, std::size_t pos)
{
    char quote = 0;
    for (std::size_t i = pos; i < s.size(); ++i) {
        const char c = s[i];
        if (quote != 0) {
            if (c == quote) {
                quote = 0;
            }
        } else if (c == '"' || c == '\'') {
            quote = c;
        } else if (c == '>') {
            return i;
        }
    }
    return std::string::npos;
}

// First name=".."|name='..'|name=bare value (case-insensitive name).
std::string Attr(const std::string &tag, const char *name)
{
    std::string low(tag.size(), ' ');
    for (std::size_t i = 0; i < tag.size(); ++i) {
        low[i] = static_cast<char>(
            std::tolower(static_cast<unsigned char>(tag[i])));
    }
    const std::string key = std::string(" ") + name + "=";
    const std::string padded = " " + low;
    const std::size_t p = padded.find(key);
    if (p == std::string::npos) {
        return {};
    }
    std::size_t q = p + key.size() - 1; // back to tag coordinates
    while (q < tag.size()
           && (tag[q] == ' ' || tag[q] == '\t' || tag[q] == '\n'
               || tag[q] == '\r')) {
        ++q;
    }
    if (q >= tag.size()) {
        return {};
    }
    const char quote = tag[q];
    if (quote != '"' && quote != '\'') {
        std::size_t e = q;
        while (e < tag.size() && tag[e] != ' ' && tag[e] != '\t'
               && tag[e] != '\n' && tag[e] != '\r' && tag[e] != '>') {
            ++e;
        }
        return tag.substr(q, e - q);
    }
    const std::size_t e = tag.find(quote, q + 1);
    if (e == std::string::npos) {
        return {};
    }
    return tag.substr(q + 1, e - q - 1);
}

std::string EscapeCode(const std::string &s)
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

} // namespace

std::string SanitizeHtml(const std::string &html)
{
    std::string out;
    out.reserve(html.size());
    std::size_t i = 0;
    while (i < html.size()) {
        if (html[i] == '<' && TagOpenAt(html, i)) {
            const std::size_t e = TagEnd(html, i);
            std::string label;
            if (e == std::string::npos) {
                label = "?";
                i = html.size();
            } else {
                const std::string tag = html.substr(i, e - i + 1);
                const std::string alt = Attr(tag, "alt");
                const std::string src = Attr(tag, "src");
                label = !alt.empty() ? alt : (!src.empty() ? src : "image");
                i = e + 1;
            }
            out += "<p><code>[image: " + EscapeCode(label) + "]</code></p>";
        } else {
            out.push_back(html[i++]);
        }
    }
    return out;
}

} // namespace YuzNote

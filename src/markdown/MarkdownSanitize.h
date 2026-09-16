#pragma once

#include <string>

namespace YuzNote {

// Post-render HTML sanitizer (PLAN §3a). Replaces every <img ...> (quote-aware;
// unclosed counts as well) with `<p><code>[image: LABEL]</code></p>` where
// LABEL is the alt text, else the src, else "image" (HTML-escaped).
// Guarantees: output never contains "<img". Pure function, never throws.
std::string SanitizeHtml(const std::string &html);

} // namespace YuzNote

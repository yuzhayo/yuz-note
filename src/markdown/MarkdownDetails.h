#pragma once

#include "MarkdownParse.h"

namespace YuzNote {

// Source-level <details> handling (mirrors Telegram's recursive details body
// re-parse; see ../reference/01-parse.md FillNodeAttributes).
//
// The source is split into plain-markdown segments and details blocks BEFORE
// cmark runs, so blank lines inside a block cannot tear it apart. Bodies are
// rendered by recursing through the same entry point (depth-capped).
struct DetailsContext {
    int depth = 0;
    int nextId = 1; // shared across the document (mutated in place)
    std::vector<std::string> *warnings = nullptr;
    const MarkdownLimits *limits = nullptr;
};

// Returns document HTML. On segment failure sets error (caller aborts).
// Never throws for in-limit inputs.
std::string ExpandDetails(const std::string &source, DetailsContext &ctx,
                          std::string &error);

} // namespace YuzNote

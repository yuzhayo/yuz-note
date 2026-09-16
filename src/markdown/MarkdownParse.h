#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace YuzNote {

// Limits ported from Telegram Iv::Markdown (see ../reference/01-parse.md).
struct MarkdownLimits {
    std::size_t maxSourceBytes = 4u * 1024u * 1024u; // 4 MiB
    int maxNodes = 100000;
    int maxNesting = 128;
    int maxDetailsDepth = 8;
};

struct MarkdownResult {
    bool ok = false;
    std::string html;                  // valid UTF-8 when ok
    std::string error;                 // stable machine token, e.g. "source-too-large"
    std::vector<std::string> warnings; // stable tokens, e.g. "nested-details-flattened"
};

// Full pipeline on bytes: validate encoding -> normalize -> details split ->
// cmark parse (limits) -> HTML -> sanitize. Never throws for in-limit inputs.
MarkdownResult RenderMarkdownBytes(const std::string &bytes,
                                   const MarkdownLimits &limits = MarkdownLimits{});

// File convenience: size-guarded read, then the same pipeline.
// "file-unreadable" on IO failure (missing, directory, no permission).
MarkdownResult RenderMarkdownFile(const std::string &path,
                                  const MarkdownLimits &limits = MarkdownLimits{});

// ---- Internal stages (exposed for Details recursion + tests) ----

struct ValidatedSource {
    bool ok = false;
    std::string error;
    std::string text; // normalized (BOM stripped, newlines are \n) when ok
};

ValidatedSource ValidateBytes(const std::string &bytes, const MarkdownLimits &limits);

// Render one plain-markdown segment (no <details> handling) to HTML.
bool RenderMarkdownSegment(const std::string &md, const MarkdownLimits &limits,
                           std::string &htmlOut, std::string &error);

} // namespace YuzNote

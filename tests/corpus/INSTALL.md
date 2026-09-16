# SampleApp — Installation Guide

Welcome to the SampleApp installer notes. This document exercises the renderer:
headings, lists, tables, code, details, links, and footnotes[^1].

## Before You Start

You need:

- The `SampleApp` archive (the `.zip` file)
- A terminal or file manager
- The tool you want to open it with

## Step 1: Create the folder structure

Open your terminal in your project folder. Run:

```bash
mkdir -p sampleapp/data
cp sample.zip sampleapp/data/app.zip
```

This creates a hidden folder `.config` with an `agent` subfolder inside it.

## Step 2: Copy the persona file

```bash
cp AGENTS.md .opencode/agent/ltx-quasar.md
```

This copies the persona into the agent folder. The file MUST be named `ltx-quasar.md`.

| Step | Action | Needs |
|:-----|--------|------:|
| 1 | Create folders | terminal |
| 2 | Copy file | archive |
| 3 | Verify | eyes |

## Learn more

See [the manual](https://example.com/manual) or jump back to
[Before You Start](#before-you-start).

<details><summary>Troubleshooting</summary>
If the copy fails, check disk space and permissions.

- [ ] disk has space
- [x] archive downloaded
</details>

[^1]: Footnotes render at the bottom of the document.

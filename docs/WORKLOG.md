# yuz-note — Worklog: Dokumentasi Pekerjaan

> Catatan lengkap semua yang sudah dikerjakan, per fase, dengan bukti gate.
> Referensi silang: [PLAN.md](PLAN.md) (rencana), [DECISIONS.md](DECISIONS.md) (keputusan),
> [RELEASE.md](RELEASE.md) (spesifikasi distribusi).
> Toolchain beku: MSVC 14.51 · Windows SDK 10.0.26100 · CMake 4.3.1 (VS 18 2026) ·
> Qt 6.8.3 LTS · cmark-gfm 0.29.0.gfm.13 · Velopack 1.2.0.

## Ringkasan Status

| Fase | Isi | Status | Bukti |
|---|---|---|---|
| P0 | Toolchain freeze | ✅ Selesai | `YUZ-TOOLCHAIN-OK msvc=1951` |
| P1 | Skeleton + hook Velopack | ✅ Selesai | `release/artifacts/deploy-test` jalan standalone |
| P2 | Parse (cmark-gfm + validasi + sanitasi) | ✅ Selesai | corpus 17/17 |
| P3 | Render (viewer + tema + details) | ✅ Kode + gate skrip | smoke 19/19; visual sign-off ⏳ |
| P4 | Shell (CLI, single-instance, find, updater) | ✅ Kode + automated gate | Explorer + updater offline manual belum dicatat |
| P5 | Distribusi (installer + CI/CD) | 🔶 Infra selesai, uji lokal belum | tools + workflows ada |
| P6 | Kunci (bench + finalisasi) | ⬜ Belum | — |

---

## P0 — Toolchain Freeze ✅ (15-09-2026)

- Verifikasi MSVC 14.51.36231 (`cl.exe` 4 varian host/target), compile-proof
  `hello.cpp` via `VsDevCmd -arch=amd64` → `YUZ-TOOLCHAIN-OK msvc=1951`.
- Windows SDK 10.0.26100.0 (Include + Lib terverifikasi).
- CMake bawaan VS 18 Community (4.3.1) mengenal generator `Visual Studio 18 2026`.
- Qt 6.8.3 LTS `win64_msvc2022_64` di `C:\Qt\6.8.3\msvc2022_64` — 0,3 GB ramping.
- `windeployqt` ada. `.NET SDK` baseline `10.0.400` hanya untuk `vpk` CLI;
  `global.json` memakai `latestPatch` dan pada 16-09-2026 meresolusikan SDK `10.0.401`.

## P1 — Skeleton + Hook Velopack ✅ (15-09-2026)

**Dibuat:**
- `CMakeLists.txt` root + `cmake/CompilerFlags.cmake` (`/W4 /EHsc /permissive- /utf-8`).
- `src/app/main.cpp` — Velopack lifecycle hooks baris pertama `main()`:
  `OnAfterInstall`/`OnAfterUpdate` → register asosiasi `.md`; `OnBeforeUninstall` → unregister.
- `src/app/MarkdownAssociation.{h,cpp}` — helper registry HKCU via raw WinAPI
  (tanpa Qt; hook jalan pra-`QApplication`):
  ```
  HKCU\Software\Classes\.md        = Yuzhayo.YuzNote.Markdown
  HKCU\Software\Classes\.markdown  = Yuzhayo.YuzNote.Markdown
  HKCU\...\Yuzhayo.YuzNote.Markdown\shell\open\command = "<exe>" "%1"
  ```
- `src/app/MainWindow.{h,cpp}` awal — title `yuz-note <ver> · Qt <qtver>`.
- `global.json` (baseline .NET SDK 10.0.400, resolve `latestPatch` = 10.0.401 pada
  16-09-2026) + `.config/dotnet-tools.json` (vpk 1.2.0, `rollForward: false`) +
  `tools/Restore-Dependencies.ps1`.
- Velopack 1.2.0 dipulihkan dari ZIP resmi ke `release/artifacts/dependencies/velopack/`;
  hash file diverifikasi oleh `Restore-Dependencies.ps1`, lockstep dengan `vpk`.

**Insiden yang dipecahkan (DECISIONS #9):** exe mati saat start (0xC0000135) karena
import lib merekam nama `velopack_libc.dll` sementara file vendor bernama
`velopack_libc_win_x64_msvc.dll`. Fix: POST_BUILD copy dengan RENAME di CMake.

**Gate:** build Release bersih (nol warning), `release/artifacts/deploy-test` jalan tanpa Qt di PATH.

## P2 — Parse ✅ (15-09-2026)

**Dibuat (`src/markdown/` + `src/mdparse/`):**
- `MarkdownParse.{h,cpp}` — wrapper cmark-gfm via FetchContent (pin tag + SHA-256
  di `cmake/FetchCmarkGfm.cmake`). Ekstensi GFM: table, tasklist, footnote,
  strikethrough, autolink, tagfilter. Opsi `CMARK_OPT_UNSAFE` + tagfilter =
  kombinasi persis Telegram.
- Validasi port Telegram: tolak BOM asing, biner, UTF-8 rusak, > 4 MiB
  (pre-check `file_size` sebelum baca — anti-OOM); limit 100.000 node, nesting 128.
- `MarkdownDetails.{h,cpp}` — parse `<details>` di level SOURCE pra-split
  (fence-aware, blank line di dalam blok tak merusak; nested di-flatten + warning;
  rekurpsi cap 8). Atribut `open` → ▾+visible, tanpa → ▸+collapsed (DECISIONS #20).
- `MarkdownSanitize.{h,cpp}` — strip semua `<img>` di level HTML-string,
  quote-aware, tag tak tertutup juga diganti → deterministik.
- `src/mdparse/main.cpp` — runner CLI `mdparse <file>`, exit code 0/1/2/3.
- `tests/corpus/` — 12 fixture teks + EXPECTATIONS.md (headings, tabel GFM,
  tasklist, footnote, details + nested + edge + many, gambar jahat, math, deep-nesting,
  INSTALL.md utuh). Fixture biner/>4MiB di-generate saat gate (DECISIONS #15).

**Gate:** `tests/run-corpus.ps1` → **17/17 PASS** (render + penolakan bersih,
HTML tanpa `<img>`, warning details terdeteksi).

## P3 — Render ✅ kode + gate skrip (15-09-2026)

**Dibuat (`src/viewer/` + `tests/viewer-smoke/`):**
- `MarkdownView.{h,cpp}` (`QTextBrowser` subclass) dengan kebijakan terkunci:
  - `openLinks=false` permanen; semua klik ke router `anchorClicked`:
    `yuz-details:*` → toggle, `#fragment` → scroll, http(s)/mailto → browser
    eksternal, `file:`/lainnya → ditolak + status bar.
  - `loadResource()` deny-all → nol pemuatan file/jaringan by construction.
  - `<details>` toggle via `QTextBlock::setVisible()` per-range + tukar marker
    ▸/▾; state per-details independen; sentinel block hidden permanen.
  - Zoom relatif (`zoomStep`/`zoomReset` simetris), Ctrl+wheel.
  - Copy isi code block murni (label bahasa paragraf terpisah — DECISIONS #21).
  - Context menu (copy code / copy semua), cursor pointing-hand manual
    (DECISIONS #23), undo disabled (DECISIONS #25).
- `ViewerTheme.{h,cpp}` — tema gelap aproksimasi Telegram-dark (hex di-tune
  saat gate visual; DECISIONS #28).
- `tests/viewer-smoke/main.cpp` — gate offscreen 19 assertion: load semua fixture,
  toggle details dua arah + round-trip, flatten nested, id asing ditolak,
  code-block count + text, INSTALL.md substansial.

**Gate:** `viewer-smoke.exe tests\corpus` → **19/19 PASS**, exit 0.
**Tersisa:** visual sign-off manual — banding `release\artifacts\deploy-test\yuz-note.exe
tests\corpus\INSTALL.md` dengan viewer Telegram (dilakukan user).

## P4 — Shell ✅ kode + gate skrip (16-09-2026)

**Dibuat:**
- `src/app/main.cpp` final — parsing CLI lengkap: `--bench` (first-paint ms →
  `%TEMP%\yuz-note-bench-ms.txt`, GUI exe tak punya console), `--help`,
  `--version`, flag asing → dialog usage exit 1; file arg → load langsung.
- `src/app/SingleInstance.{h,cpp}` — kontrak terkunci PLAN §5: probe
  `QLocalSocket` 1,5 dtk → terhubung = kirim path + exit 0 tanpa UI;
  gagal = `removeServer` + listen (degraded mode dilaporkan ke status bar,
  app tetap jalan). Handoff terbukti: owner title berubah jadi
  `... - INSTALL.md` setelah instance kedua exit 0.
- `src/app/FindBar.{h,cpp}` — Ctrl+F, highlight-all, navigasi Enter/Shift+Enter,
  Esc tutup.
- `MainWindow` diperluas: menu File (Open, Recent dinamis via QSettings,
  Exit), Help (Check for Updates, About), drag-drop `.md`, shortcut zoom
  Ctrl+=/-/0, `openAndRaise` (showNormal+raise+activateWindow), polling state
  updater 250 ms.
- `src/update/UpdateService.{h,cpp}` — boundary Velopack `UpdateManager`
  (PIMPL, thread-safe snapshots via `shared_ptr<atomic>`): `checkForUpdates`,
  `downloadUpdates` (progress callback), `applyAndRestart`. Feed = GitHub repo
  URL (`GithubSource`), `setFeedUrl` sebelum operasi pertama. Tanpa cek otomatis
  saat startup — hanya aksi user Help → Check for Updates (3 state:
  tidak-ada / tersedia → Download & Restart / gagal-jaringan + pesan).
- `tests/single-instance-check.ps1` — skenario A→B otomatis.
- Qt `Network` component ditambahkan (QLocalServer/QLocalSocket butuh itu).

**Insiden yang dipecahkan (16-09-2026):** app "hidup tapi tanpa jendela" saat
diagnosis P4 — root cause: sisa kode diag (`ExitProcess(42)`, `MessageBoxA`,
trace) tertinggal di `main.cpp` dari iterasi debug. Dibersihkan total; setelah
rebuild semua gate hijau. Pelajaran: keadaan diag harus dibersihkan sebelum
mengklaim gate.

**Gate automatis (hijau 16-09-2026):**
- corpus 17/17, viewer-smoke 19/19
- single-instance A→B: owner hidup, second exit 0, title owner = `... - INSTALL.md`
- `release/artifacts/deploy-test` standalone tanpa Qt di PATH: jalan + load INSTALL.md
- `--version`/`--help` exit 0

**Belum dicatat sebagai bukti manual:** klik-penuh dari Explorer tanpa console error,
serta tiga state updater saat offline. Karena itu P4 tetap `[ ]` di `PLAN.md` sampai
bukti tersebut dicatat.

## P5 — Infrastruktur Distribusi 🔶 (16-09-2026)

**Dibuat (pola Citadel, diadaptasi C++/Qt):**
- `.gitignore` — release/build/, release/artifacts/, release/Releases/, IDE files,
  generated files, OS junk.
- `tools/Get-ProjectVersion.ps1` — baca + validasi `version.txt` (semver 3 bagian).
- `tools/Get-ReleaseVersion.ps1` — `max(version.txt, tag v*)` + bump
  patch/minor/major (identik perilaku Citadel).
- `tools/Build-Release.ps1` — cmake build Release → windeployqt staging →
  copy `velopack_libc.dll` → verify staging (exe ada, tanpa PDB) →
  `vpk pack` (`packId Yuzhayo.YuzNote` — BEKU, shortcuts Desktop+StartMenu,
  icon `assets/icon.ico` bila ada) → output JSON {Version, Setup, SHA-256}.
- `tools/Convert-Icon.ps1` — PNG → ICO (System.Drawing).
- `src/app/yuz-note.rc` — resource Windows: icon + VERSIONINFO
  (CompanyName Yuzhayo, ProductName yuz-note, versi 0.1.0).
- `src/app/CMakeLists.txt` — `.rc` masuk `add_executable`.
- `.github/workflows/ci.yml` — push main/PR: setup Qt 6.8.3 (install-qt-action)
  + .NET 10 → Restore-Dependencies → configure → build → corpus → viewer-smoke.
- `.github/workflows/release.yml` — workflow_dispatch (bump patch/minor/major),
  main-only, green-CI gate di SHA sama, hitung versi, release notes dari
  git log, `vpk download github` (delta bila rilis sebelumnya ada),
  Build-Release.ps1, `vpk upload github --publish --tag vX.Y.Z`,
  summary SHA-256. `permissions: {checks: read, contents: write}`,
  `concurrency: yuz-note-release`.

**Belum diuji:** pack lokal end-to-end, dispatch workflow (butuh repo GitHub +
rilis pertama), uji install di VM (gate penuh PLAN §5).

## Aset & Konfigurasi

- `version.txt` = `0.1.0` → `cmake/version.h.in` → `build/generated/version.h`
  → macro `YUZNOTE_VERSION` (title + `--version`).
- `assets/icon.png` — aset scroll sumber 448×448 transparan; `assets/icon.ico`
  dibuat dari aset ini dengan frame 16/24/32/48/64/128/256 px.
- `version.h.in` di-`configure_file` per build.

## Angka Proyek

- 29 file source/tests/tools/cmake/CI (~150 KB kode + skrip).
- 3 target lib (`yznote-markdown`, `yznote-viewer`, `yznote-update`),
  2 exe (`yuz-note`, `viewer-smoke`) + `mdparse` + cmark-gfm static.
- Build: 0 error, 0 warning (Release, `/W4`).
- Gate terakumulasi: corpus 17/17 + smoke 19/19 + single-instance + `release/artifacts/deploy-test`.

## Yang Tersisa Menuju v1

1. **P3 visual sign-off** — user banding INSTALL.md vs Telegram, catat di DECISIONS.
2. **P5 uji lokal** — jalankan `tools/Build-Release.ps1` end-to-end (butuh `dotnet vpk`).
3. **P5 E2E** — push ke GitHub, dispatch release, install Setup di VM, uji
   asosiasi + update (gate penuh PLAN §5).
4. **P6** — fixture `tests/bench/bench-1mib.md`, `docs/BENCH.md`, BENCH-1
   median 5× ≤ 1000 ms, tandai plan SELESAI, rilis 0.1.0 pertama.

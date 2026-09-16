# YUZ-NOTE — Build Plan v1 (Markdown Opener, C++/Qt)

> Status: PLAN — belum ada kode proyek. Setiap fase berakhir di *verification gate*;
> fase berikutnya tidak mulai sebelum gate lolos. Prinsip: *plan matang, debug sedikit.*
> Amandemen A (15-09-2026): menutup 5 temuan review PARTIAL — `details` (§3b+P3),
> gambar (§3a+P2), single-instance (P4), benchmark (§7+P6), artefak acuan (P2/P3).
> Amandemen B (15-09-2026): klik details via `href`+`anchorClicked` (§3b),
> kebijakan link & navigasi (§3a), fixture bench di-commit tanpa Python (§7).
> Amandemen C (15-09-2026): installer Velopack via GitHub Releases masuk v1
> (`docs/RELEASE.md` + P5); in-app updater C++ = fase v2.
> Amandemen D (15-09-2026): packId `Yuzhayo.YuzNote` (beku sebelum 0.1.0); hook lifecycle
> Velopack C++ di P1 + kontrak registry HKCU; deps terkunci (global.json/dotnet-tools/
> restore Velopack resmi ke `release/artifacts/dependencies`); UpdateManager ringan masuk v1 (boundary P4 + aksi Help,
> E2E di P5); `release.yml` cermin Citadel.

## 1. Tujuan & batasan v1

**Tujuan:** aplikasi desktop Windows native — double-click `.md` → jendela render cantik
(seperti viewer Markdown Telegram), dibuka-cepat, terasa seperti Notepad++.

**Masuk v1 (opener, read-only):**
- Buka file `.md`/`.markdown` via argumen CLI, double-click (asosiasi file), drag-drop
- Render gelap gaya Telegram: heading, list, task list, tabel, code block + label bahasa,
  blockquote, footnote, link/anchor, `<details>` collapsible (mekanisme custom §3b),
  gambar → placeholder teks `[image: …]` (aturan sanitasi §3a)
- Zoom (`Ctrl+=/-/0`, `Ctrl+wheel`), find (`Ctrl+F`), copy isi code block
- Recent files, single-instance, gagal-bersih (file rusak/biner/raksasa → pesan, bukan crash)

**Keluar v1 (dicatat, tidak dikerjakan):** edit/save, gambar (`![]()` — Telegram juga
sengaja tidak render), render rumus LaTeX (fallback teks mono; MicroTeX = fase v2),
embed web, slideshow media. **Masuk v1:** installer resmi Velopack via GitHub Releases
(detail `docs/RELEASE.md`, eksekusi fase P5) + in-app updater ringan
(`Help → Check for Updates`, boundary P4, E2E di P5; tanpa cek otomatis saat startup).

## 2. Toolchain beku (terverifikasi, bukan asumsi)

| Komponen | Hasil verifikasi (15-09-2026) |
|---|---|
| MSVC | `14.51.36231`, `cl.exe` 4 varian host/target — **compile-proof lolos** (`YUZ-TOOLCHAIN-OK msvc=1951` via `VsDevCmd -arch=amd64`) |
| Windows SDK | `10.0.26100.0` (`Include` + `Lib` ada) |
| CMake | bawaan VS 18 Community (`.../CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe`) |
| Qt | **6.8.3 LTS** `win64_msvc2022_64` di `development/Qt/6.8.3/msvc2022_64` (`qmake`, `Qt6Config.cmake`, `Qt6Widgets.lib`; bootstrap clone pertama) |
| `windeployqt` | terverifikasi ada (review 15-09-2026) — dipakai gate P1 dan P5 |
| .NET SDK | HANYA untuk `vpk` CLI (app tetap C++/Qt murni); pin `10.0.400` di `global.json` (ada di host, sama seperti Citadel) |
| `vpk` CLI | versi di-pin di `.config/dotnet-tools.json` (`rollForward: false`); wajib lolos `dotnet vpk --version` via skrip restore (P1) |
| Velopack C++ | ZIP resmi dipulihkan ke `release/artifacts/dependencies/velopack`; versi dan hash dikunci di `Restore-Dependencies.ps1`, sama dengan `vpk` |
| git | ada |

`development/Qt/6.8.3/msvc2022_64` untuk semua configure. Keputusan generator
CMake (VS-generator vs NMake) diputuskan di P1 setelah cek versi CMake bawaan —
fallback yang selalu jalan: `NMake Makefiles` dalam env `VsDevCmd`.

## 3. Arsitektur (cermin 4 lapis Telegram, versi ramping)

```
yuz-note/
├── CMakeLists.txt + cmake/      ← flags MSVC, cari Qt, FetchContent cmark-gfm
├── src/
│   ├── app/                     ← main (hook Velopack baris pertama), MainWindow,
│   │                               MarkdownAssociation (HKCU, hanya via hook),
│   │                               recent files, single-instance, drag-drop
│   ├── update/                  ← UpdateService boundary (Check/Download/ApplyRestart)
│   ├── markdown/                ← PARSE: wrapper cmark-gfm + limit port Telegram:
│   │                               4 MiB source · 100.000 node · nesting 128
│   │                               + tolak BOM asing/biner/UTF-8 rusak (cf. ../reference/01-parse.md)
│   └── viewer/                  ← RENDER+SHELL: cmark-gfm → HTML → QTextDocument,
│                                   tema gelap, zoom, find, copy-code, anchor
├── docs/                        ← PLAN ini + catatan build
└── tests/                       ← korpus .md jebakan + parse-runner
```

**Keputusan renderer v1 (dikunci):** `cmark-gfm → HTML → QTextDocument::setHtml`
di `QTextBrowser`. Alasan: parser & limit **persis** Telegram (paritas perilaku di lapis
parse — bagian tersulit), Qt menanggung layout/seleksi/scroll (bagian termahal bila
ditulis manual). HTML adalah *seam*: engine custom ala 70 file bisa menggantikan
`QTextBrowser` nanti tanpa menyentuh parser. Yang ditulis manual hanya yang Qt tak
punya: tombol copy di header code (konteks/klik), token tema, anchor `#` — plus dua
desain pengecualian di bawah (bukan asumsi, keduanya punya gate).

### 3a. Sanitasi HTML + kebijakan resource (dikunci)

- Aturan: tahap prepare (di dalam `mdparse`, dipakai bersama runner + viewer)
  menghapus **semua** `<img …>` → diganti paragraf placeholder mono
  `[image: <alt-text-atau-URL>]`. Gambar tidak pernah sampai ke `QTextDocument`.
- Kebijakan resource: override `QTextBrowser::loadResource()` → selalu kembalikan
  kosong untuk **semua** tipe. Nol pemuatan file/jaringan by construction
  (bukan by convention). Rasional: Telegram memetakan image → `Unsupported`;
  placeholder teks adalah padanan jujurnya.
- Kebijakan link & navigasi (dikunci): `openLinks=false` permanen; semua klik masuk
  slot `anchorClicked(QUrl)` dan di-routing aplikasi — `yuz-details:*` → toggle §3b;
  `#fragmen` internal → `scrollToAnchor`; `http(s)`/`mailto:` → browser/handler
  eksternal (`QDesktopServices`); `file:` dan skema lain → ditolak (log + status bar,
  tanpa navigasi). Dokumen tidak pernah berpindah sendiri; file lokal tak terduga
  tidak pernah terbuka.

### 3b. `<details>` collapsible — interaksi custom minimal (dikunci)

- Fakta: subset HTML `QTextDocument` tidak mengenal `details`/`summary` — importer Qt
  membuang tag asing dan menyisakan isi polos. Maka collapsible = satu-satunya
  perilaku interaktif custom di v1, dengan desain minimal:
  1. Prepare tulis ulang `<details><summary>S</summary>B</details>` → paragraf summary
     `<p><a name="details-N" href="yuz-details:details-N">▸ S</a></p>` + blok body +
     sentinel akhir (paragraf kosong ber-anchor `details-N-end`). `name` = target
     anchor (agar `#details-N` bisa di-scroll); `href` = satu-satunya yang membuat
     teks **bisa diklik** (Qt mengikuti `href`, bukan `name`).
  2. Viewer bangun peta `details-N → [blok-body-awal … blok-body-akhir]` sekali saat load.
  3. `openLinks=false`; slot `anchorClicked(QUrl)`: skema `yuz-details` → toggle
     `QTextBlock::setVisible()` seluruh range + tukar marker ▸/▾ (kursor pointing-hand
     otomatis karena ada `href`). Independen per-`details`; QTextBrowser tidak pernah
     navigasi sendiri.
  4. Nested `<details>` ditolak seperti Telegram: inner di-flatten jadi konten polos
     + warning (tidak diabaikan diam-diam).
- Pre-approved downgrade: bila gate P3(b) gagal → selalu-expanded tanpa toggle,
  catat di `docs/DECISIONS.md`, **tanpa** amandemen plan baru.

## 4. Third-party (pin, jangan hanyut)

| Lib | Cara ambil | Catatan |
|---|---|---|
| `cmark-gfm` | CMake FetchContent, **tag/commit di-pin** di `cmake/FetchCmarkGfm.cmake` | Parser C Telegram; ekstensi GFM: table, tasklist, footnote, strikethrough, autolink, tagfilter |
| `MicroTeX` | **TUNDA ke v2** | Alasan: risiko build terbesar ke-2 setelah Qt; v1 rumus = teks mono (Telegram punya mode fallback yang sama) |

## 5. Fase + verification gate

- [x] **P0 — Toolchain freeze.** Gate: `hello.cpp` lolos via `VsDevCmd`. ✅ 15-09-2026
- [x] **P1 — Skeleton + hook Velopack.** ✅ 15-09-2026 — configure (`Visual Studio 18 2026`) +
  build Release bersih (nol warning); title `yuz-note 0.1.0 · Qt 6.8.3` terbukti;
  `release/artifacts/deploy-test` jalan tanpa Qt di PATH; hook + assoc helper terkompilasi (perilaku diuji P5);
  insiden DLL-name 0xC0000135 → DECISIONS #9.
- [x] **P2 — Parse.** ✅ 15-09-2026 — `yznote-markdown` + `mdparse` hijau nol warning;
  gate korpus **17/17** (`tests/run-corpus.ps1`): render, penolakan bersih (BOM/biner/
  UTF-8-rusak/>4MiB/nesting-200), HTML tanpa `<img>`, 3 warning details;
  fixture biner di-generate (korpus commit teks-only).
- [ ] **P3 — Render.** HTML → `QTextBrowser` + tema gelap + code-header + tabel rapi +
  toggle `<details>` (§3b). Gate: (a) `tests/corpus/INSTALL.md` tampil setara viewer
  Telegram (banding visual manual); (b) uji klik details — buka-tutup independen,
  nested ter-flatten + warning, 50 details tak merusak layout, state tahan zoom;
  (c) tabel rusak tidak merusak layout halaman. Gagal (b) → downgrade pre-approved §3b.
- [ ] **P4 — Shell.** CLI arg, drag-drop, recent files (`QSettings`), single-instance,
  zoom, `Ctrl+F` + highlight, copy code, anchor `#` scroll.
  Kontrak single-instance (dikunci): instance ke-2 mengirim path absolut via
  `QLocalServer` → tunggu ACK (timeout 3 dtk) → keluar kode 0 **tanpa menampilkan
  jendela**; instance pertama memuat file (bila `.md` valid), naik ke depan
  (`showNormal`+`raise`+`activateWindow`), balas ACK; socket basi → instance ke-2
  merebut peran server. Gate: skenario A→B — buka A, buka B dari Explorer, assert:
  satu proses, jendela di depan, B termuat, proses ke-2 exit 0; plus klik-penuh
  dari Explorer tanpa console error.
- [ ] **P5 — Distribusi (installer + update E2E via GitHub Releases, detail `docs/RELEASE.md`).**
  Staging verify (exe + Qt DLLs + tanpa PDB) → `windeployqt` → `vpk pack`
  (packId `Yuzhayo.YuzNote` — beku sebelum 0.1.0; shortcuts Desktop+StartMenu;
  asosiasi `.md` HKCU tanpa admin via hook P1) → workflow dispatch (patch/minor/major;
  main-only; green-CI gate; download rilis sebelumnya untuk delta) →
  `vpk upload --publish` (tag `vX.Y.Z`, notes, SHA-256).
  Updater ringan (P4) diuji E2E di sini, bukan di P4.
  Gate: install Setup di VM → auto-launch tanpa UI hook → `reg query` ekstensi +
  ProgID OK → double-click sample.md (satu instance) → Setup versi baru (asosiasi
  tetap) → publish 0.1.1 → Help→Check→Download & Restart → berjalan 0.1.1 →
  uninstall: shortcut + key ProgID hilang, QSettings utuh.
- [ ] **P6 — Kunci.** Korpus tests permanen + fixture `tests/bench/bench-1mib.md` +
  `docs/DECISIONS.md` + `docs/BENCH.md` (spesifikasi mesin acuan).
  Gate: semua test hijau, BENCH-1 median ≤ 1000 ms (§7), plan ini ditandai SELESAI.

## 6. Risiko & mitigasi

| Risiko | Mitigasi |
|---|---|
| Subset HTML Qt memutilasi tabel kompleks | Gate P3 menangkap; fallback: sederhanakan tabel di lapis prepare (masih faithful) |
| Generator CMake bawaan VS tak kenal "VS 18" | Fallback NMake (sudah disiapkan di §2) |
| FetchContent butuh jaringan saat configure | Pin + catat hash; vendor tarball bila perlu (§4) |
| Scope merayap (edit? gambar? math?) | Daftar §1 adalah kontrak; tambahan = amandemen plan tertulis, bukan kode diam-diam |
| QTextDocument buang `details` / render `<img>` | Desain §3a/§3b + gate P2/P3; downgrade details pre-approved |
| Kontrak single-instance ambigu | Dikunci di P4 + skenario A→B sebagai gate |
| Setup unsigned → SmartScreen | Batas sadar (cf. Citadel); signing bila sertifikat ada |
| API/hook C++ Velopack tak sesuai ekspektasi | Pin versi (`vpk` == client lib); verifikasi P1 (restore + hook terpasang); referensi docs C++ tercatat di P1 |
| `dotnet vpk` butuh runtime .NET tertentu | `Restore-Dependencies.ps1` assert saat P1; pasang runtime sesuai pesan error + catat di DECISIONS |

## 7. Kriteria selesai v1

- **BENCH-1:** fixture `tests/bench/bench-1mib.md` (1 MiB, **di-commit** — tanpa
  prasyarat Python; dibuat sekali dari generator seed-tetap, generatornya dibuang);
  ukur cold start dari invocasi proses sampai konten pertama tampil (flag `--bench`
  cetak ms saat first paint); median 5x **≤ 1000 ms** di mesin acuan yang
  spesifikasinya tercatat di `docs/BENCH.md` (CPU/RAM/SSD/OS/build Qt).
- Semua gate P1–P6 hijau, tidak ada `TODO` di kode, `docs/DECISIONS.md` mencatat tiap
  penyimpangan dari plan ini beserta alasannya.








## 8. IMPLEMENTATION PLAN END TO END

> Cara baca: tiap fase = tujuan → file → langkah berurutan → perintah exact → gate.
> Gate merah = berhenti, catat di `docs/DECISIONS.md`, jangan lanjut ke fase berikut.
> Build shell untuk semua perintah = Developer Prompt via:
> `"C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=amd64 -no_logo`
> dan setiap configure memakai Qt lokal di `development/Qt/6.8.3/msvc2022_64`.

### Konvensi global (semua fase)

- Resolve-once: path runtime dihitung sekali di shell → oper absolut ke bawah;
  tidak ada modul menghitung path sendiri; tidak ada tulis di samping exe.
- Nol `TODO` di kode. Penyimpangan → `docs/DECISIONS.md` (tanggal, apa, kenapa,
  alternatif yang ditolak).
- Gate = demo tercatat (tanggal + hasil + bukti) sebelum fase berikut dimulai.
- CRT default dinamis; bila exe gagal di mesin bersih tanpa VC Redist, evaluasi
  `/MT` statik di P5 dan catat di DECISIONS (jangan diputuskan sekarang).

### P0 — Toolchain freeze ✅ (selesai 15-09-2026, bukti §2)

### P1 — Skeleton + lifecycle hooks Velopack (jendela ter-deploy; hook terpasang)

File: `CMakeLists.txt` (root: `project(YuzNote)`, CXX20, `find_package(Qt6 Widgets)`,
output dir seragam) · `cmake/` (flags MSVC `/W4 /EHsc /permissive-`) ·
`src/CMakeLists.txt` + `src/app/CMakeLists.txt` ·
`src/app/main.cpp` + `src/app/MainWindow.{h,cpp}` (QMainWindow; title
`yuz-note <version> · Qt <qtversion>` sebagai bukti link; central placeholder) ·
`src/app/MarkdownAssociation.{h,cpp}` (helper registrasi HKCU — kontrak di bawah;
dipanggil HANYA oleh hook, bukan skrip terpisah) ·
`global.json` (pin SDK .NET 10.0.400) · `.config/dotnet-tools.json` (`vpk` pin,
`rollForward: false`) · `tools/Restore-Dependencies.ps1` (restore + assert
`dotnet vpk --version` + download/hash-verifikasi ZIP resmi Velopack) ·
`release/artifacts/dependencies/velopack/` (output restore: header + import lib +
runtime DLL C++; referensi Velopack docs C++ packaging & C API).

Langkah: (0) restore deps via skrip (gagal → stop; bila `vpk` menolak jalan karena
runtime .NET, pasang runtime sesuai pesan error + catat di DECISIONS);
(1) cek versi CMake bawaan → pilih generator (`Visual Studio 18 2026` bila
dikenal, else `Visual Studio 17 2022`, fallback `NMake Makefiles`) → catat pilihan +
alasan di DECISIONS; (2) configure + build Release; (3) run dari build shell;
(4) `windeployqt` ke `release/artifacts/deploy-test/` → run exe dari situ (bukti deploy mandiri);
(5) tulis hook + helper assoc (kode di bawah; kompilasi bersih — perilaku hook
diuji di P5, P1 hanya memastikan terpasang sebagai baris pertama `main()`).

Hook (baris pertama `main()`, sebelum `QApplication`/single-instance/UI —
meniru `Program.cs:14` Citadel; hook dijalankan dalam proses app saat
install/update/uninstall):
```cpp
Velopack::VelopackApp::Build()
  .SetAutoApplyOnStartup(false)
  .OnAfterInstall(RegisterMarkdownAssociation)
  .OnAfterUpdate(RegisterMarkdownAssociation)
  .OnBeforeUninstall(UnregisterMarkdownAssociation)
  .Run();
```
Kontrak registry (HKCU, tanpa admin; hanya key milik ProgID ini):
```text
HKCU\Software\Classes\.md              = Yuzhayo.YuzNote.Markdown
HKCU\Software\Classes\.markdown        = Yuzhayo.YuzNote.Markdown
HKCU\Software\Classes\Yuzhayo.YuzNote.Markdown\shell\open\command
                                        = "<absolute path yuz-note.exe>" "%1"
```
`OnBeforeUninstall` hanya menghapus key di atas; `QSettings`/data LocalAppData
TIDAK dihapus.

Perintah (dari build shell, root repo `yuz-note/`):
```
powershell -ExecutionPolicy Bypass -File tools/Restore-Dependencies.ps1
"C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --version
cmake --preset dev
cmake --build release/build --config Release
development\Qt\6.8.3\msvc2022_64\bin\windeployqt.exe release\build\src\app\Release\yuz-note.exe --dir release\artifacts\deploy-test
```
Gate: jendela terbuka + title benar + exe di `release/artifacts/deploy-test/` jalan (PATH Qt tidak
diandalkan) + hook terpasang + restore skrip hijau.

### P2 — Parse (mdparse + korpus, fondasi kebenaran)

File: `cmake/FetchCmarkGfm.cmake` (FetchContent `cmark-gfm`, tag+hash di file ini;
ekstensi: table, tasklist, footnote, strikethrough, autolink,
tagfilter) · `src/markdown/*` (wrapper: baca → validasi ukuran/BOM/biner/UTF-8
per `../reference/01-parse.md` → parse + limit 4 MiB/100rb node/nesting-128 → HTML →
sanitasi §3a (strip `<img>` → `[image: …]`) + tulis-ulang `<details>` (§3b langkah 1))
· `src/mdparse/main.cpp` (`mdparse <file>` → cetak HTML / kode error) ·
`tests/corpus/` (headings, tabel GFM, tasklist, footnote, details+ nested,
`INSTALL.md` utuh, fixture gambar jahat, fixture penolakan).

Langkah: fetch-pin → lapisan validasi → parse+limit → HTML+sanitasi+details-rewrite →
runner → korpus+INSTALL.md → jalankan gate.
Gate (§5): semua render/tolak sesuai ekspektasi; HTML **tanpa `<img>`**;
nested-details ter-flatten + warning; tak hang/crash, pesan jelas.

### P3 — Render (viewer widget, INSTALL.md setara Telegram)

File: `src/viewer/*` (`MarkdownView : QTextBrowser`: `openLinks=false` permanen;
`loadResource()` deny-all; router `anchorClicked` (§3a); peta details + toggle
`setVisible` (§3b); tema gelap — token palet + stylesheet; blok code gaya Telegram + label bahasa; tabel rapi).

Langkah: dokument tampil → tema → code/tabel → details-toggle → uji klik.
Gate (§5): (a) `tests/corpus/INSTALL.md` setara viewer Telegram (banding visual);
(b) klik details independen + tahan zoom + 50 details aman; gagal (b) → downgrade
pre-approved §3b; (c) tabel rusak tak merusak halaman.

### P4 — Shell (aplikasi v1 lengkap + updater ringan)

File: `src/app/*` (argumen CLI incl. `--bench`; drag-drop; recent files `QSettings`
di LocalAppData; single-instance `QLocalServer` persis kontrak §5; zoom;
FindBar `Ctrl+F` + highlight; aksi copy-code isi murni; scroll anchor `#`;
`--bench` cetak ms saat first paint via `QElapsedTimer`) ·
`src/update/UpdateService.{h,cpp}` (satu boundary, tanpa timer/polling:
`CheckForUpdates()` / `DownloadUpdates(progress)` / `ApplyAndRestart()`;
tanpa cek otomatis saat startup — hanya aksi pengguna, cf. Citadel) ·
UI: satu aksi `Help → Check for Updates` (BUKAN halaman Settings baru; 3 state:
tidak-ada-update / tersedia → tombol `Download & Restart` / gagal-jaringan +
pesan jelas, app tetap normal).

Langkah: CLI → drag-drop → recent → single-instance → zoom/find/copy → anchor →
updater wiring + 3 state → bench-hook. Gate (§5): skenario A→B (satu proses, depan,
B termuat, exit 0) + klik-penuh Explorer tanpa console error + aksi Help tampil
3 state tanpa crash offline (E2E update penuh di P5).

### P5 — Distribusi (installer via GitHub Releases, detail `docs/RELEASE.md`)

File: `tools/Build-Release.ps1` (port guardrail Citadel: cek semver → confine output
di repo → `cmake --install` staging → `windeployqt` → verify staging (exe + Qt DLLs,
tanpa PDB) → `vpk pack` packId **`Yuzhayo.YuzNote`** — beku sejak rilis pertama) ·
`tools/Get-ProjectVersion.ps1` (baca `version.txt`) ·
`tools/Get-ReleaseVersion.ps1` (`max(version.txt, tag v*)` + bump) ·
`.github/workflows/release.yml` (+ `ci.yml` minimal build+korpus — wajib ada karena
release mensyaratkan check hijau di commit yang sama), cermin Citadel:
```yaml
permissions: { checks: read, contents: write }
concurrency: { group: yuz-note-release, cancel-in-progress: false }
```
job: wajib `main` → checkout `fetch-depth: 0` → setup SDK pin → cek check `build`
hijau di SHA yang sama → hitung versi → `vpk download github` (delta; absen = full
saja) → `Build-Release.ps1` → `vpk upload github --publish --tag vX.Y.Z
--targetCommitish $SHA` → SHA-256 ke summary. (Asosiasi registry = helper C++ via
hook P1, BUKAN skrip ps1 terpisah.)

Langkah: skrip lokal → pack lokal → dispatch uji → gate VM kontrak nyata:
install Setup di VM → app auto-launch tanpa UI hook → `reg query` dua ekstensi +
ProgID OK → double-click sample.md (satu instance, file termuat) → install Setup
versi lebih baru → asosiasi tetap berfungsi → publish 0.1.1 → Help→Check→
Download & Restart → berjalan 0.1.1 → uninstall: shortcut + key ProgID milik
YuzNote hilang, QSettings tetap ada.

### P6 — Kunci (selesai v1)

Commit fixture `tests/bench/bench-1mib.md` → tulis `docs/BENCH.md` (spesifikasi mesin
acuan) → jalankan BENCH-1 (median 5x ≤ 1000 ms) → hijaukan semua test → lengkapi
`docs/DECISIONS.md` → tandai plan SELESAI → **baru** dispatch rilis `0.1.0` pertama
via alur P5. Urutan ini disengaja: yang di-tag rilis adalah kode yang sudah terkunci,
bukan sebaliknya.

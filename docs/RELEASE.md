# YUZ-NOTE — Release & Installer Guide (Velopack via GitHub Releases)

> Kontrak distribusi v1. Eksekusi di fase P5 (`docs/PLAN.md`). Pola diadopsi dari
> Citadel (`docs/operations/release.md`); yang beda hanya sisi build (CMake+Qt)
> dan packId. `vpk` CLI stack-agnostic — perintahnya sama untuk C++.

## 1. Alur pengguna (yang kamu minta)

1. Buka halaman **GitHub Releases** yuz-note.
2. Unduh `YuzNote-<versi>-Setup.exe`, jalankan.
3. Terinstall per-user ke `%LocalAppData%\YuzNote` — **tanpa admin** — + shortcut
   Desktop & StartMenu + asosiasi `.md`/`.markdown` (double-click langsung buka).
4. Versi baru = jalankan Setup barunya (update full yang rapi; in-app updater = v2).

## 2. Versioning

- `version.txt` (repo root proyek, satu baris, saat ini `0.1.0`) = single source of truth,
  mengalir ke `version.h` (via `configure_file`) dan `--packVersion` saat pack.
- Rilis berikutnya = `max(version.txt, tag v* terbaru)` + bump (patch/minor/major).
  Tidak pernah mundur. Rilis pertama `0.1.0`.
- `packId` = **`Yuzhayo.YuzNote`** — dibekukan SEBELUM rilis `0.1.0` (Velopack mengikat
  install/uninstall/delta ke ID ini; ganti ID sesudahnya = install baru yang terpisah).

## 3. Pack lokal (pra-syarat: build Release + `windeployqt` sudah jalan)

1. Build CMake ke `release/build/`, lalu stage payload ke `release/artifacts/publish/win-x64/`.
2. `windeployqt` ke staging (Qt DLL ikut — tanpa ini exe mentah tak jalan di PC lain).
3. **Verify staging, bukan asumsi**: exe ada, Qt6Core/Gui/Widgets DLL ada, tanpa PDB,
   ukuran wajar. Gagal satu → stop (meniru guardrail `Build-Release.ps1` Citadel).
4. `vpk pack --packId Yuzhayo.YuzNote --packVersion <ver> --packDir <staging> --mainExe yuz-note.exe --outputDir release/Releases/`.
5. Output `release/Releases/`: `*-Setup.exe`, `*-Portable.zip`, `*-full.nupkg`
   (+ `-delta.nupkg` bila rilis sebelumnya tersedia), `RELEASES`, `releases.win.json`.

## 4. GitHub release (otomatis, dispatch manual)

Workflow `release.yml` (dibuat di P5, meniru Citadel ~90%): dispatch
patch/minor/major → wajib branch `main` → wajib check `build` hijau **di commit yang
persis sama** → hitung versi (§2) → notes dari `git log` sejak tag terakhir →
`vpk download github` rilis sebelumnya (bahan delta; absen = full saja) → pack (§3) →
`vpk upload github --publish` (aset + tag `vX.Y.Z` + notes + SHA-256 di summary).
Struktur cermin Citadel (bukan paralel):

```yaml
permissions: { checks: read, contents: write }
concurrency: { group: yuz-note-release, cancel-in-progress: false }
```

Hanya build command yang diganti CMake/Qt; guardrail sama: main-only,
`fetch-depth: 0`, green-`build`-check di SHA yang sama, download-prev, publish bertag.

## 5. Detail installer & uninstall

- Per-user, tanpa admin, self-contained (PC target tak perlu Qt terinstall).
- Unsigned (seperti Citadel): SmartScreen boleh protes sampai ada sertifikat —
  batas sadar, bukan bug.
- Uninstall menghapus payload + shortcut; **data user selamat** karena state
  (`QSettings`, recent files) tinggal di folder LocalAppData terpisah, bukan di
  dalam payload install. Ini hasil langsung disiplin read-only (§6).
- Jangan edarkan exe mentah: butuh Qt DLL + pendampingnya (setara larangan
  Citadel edarkan `Citadel.Shell.exe` sendirian).

### Kontrak registry asosiasi (HKCU, tanpa admin — milik ProgID sendiri)

Ditulis HANYA oleh hook lifecycle (`OnAfterInstall`/`OnAfterUpdate`), dihapus HANYA
oleh `OnBeforeUninstall` — bukan skrip terpisah:

```text
HKCU\Software\Classes\.md              = Yuzhayo.YuzNote.Markdown
HKCU\Software\Classes\.markdown        = Yuzhayo.YuzNote.Markdown
HKCU\Software\Classes\Yuzhayo.YuzNote.Markdown\shell\open\command
                                        = "<absolute path yuz-note.exe>" "%1"
```

Uninstall menghapus key di atas + shortcut; `QSettings`/data LocalAppData
disisakan. Karena `OnAfterUpdate` mendaftarkan ulang, asosiasi selamat melewati
update versi.

## 6. Disiplin read-only (kontrak kode, bukan imbauan)

Folder install = payload read-only (Velopack mengganti `current/` utuh tiap update).
Maka: **resolve sekali di shell → oper path absolut ke bawah; tidak ada yang
menghitung path sendiri; tidak ada tulis di samping exe/repo/folder install.**
Config rusak = fail-soft (default jalan), bukan crash. Bukti kepatuhan: install
read-only tetap jalan penuh (assert di gate P5).

## 7. Update policy

- **v1 (ini):** hook lifecycle Velopack C++ (§P1 PLAN: `SetAutoApplyOnStartup(false)` +
  `OnAfterInstall/OnAfterUpdate/OnBeforeUninstall`) + updater ringan:
  boundary `src/update/UpdateService` (`CheckForUpdates`/`DownloadUpdates`/
  `ApplyAndRestart`, tanpa timer) + satu aksi `Help → Check for Updates`
  (state: tidak-ada / tersedia→`Download & Restart` / gagal-jaringan).
  Tanpa hook ini + tanpa client ini, `full/delta.nupkg` yang dihasilkan rilis
  tidak punya konsumen — keduanya satu integrasi yang sama, bukan dua sistem.
- **Kebijakan:** tidak ada cek update otomatis saat startup; hanya aksi pengguna
  (cf. Citadel).
- **v2:** dicadangkan bila butuh alur lebih kaya (progress latar, jadwal, channel).
  Tidak didefinisikan sekarang.

## 8. Dependensi rilis (dikunci sebelum pack pertama)

| Artefak | Isi | Aturan |
|---|---|---|
| `global.json` | pin SDK .NET (untuk `vpk`; app tetap C++/Qt murni) | sama seperti Citadel (`10.0.400`) |
| `.config/dotnet-tools.json` | `vpk` pin, `rollForward: false` | `dotnet vpk --version` wajib lolos via skrip restore |
| `release/artifacts/dependencies/velopack/` | header + import lib + runtime DLL C++ hasil restore | generated; tidak disimpan di source tree |
| `tools/Restore-Dependencies.ps1` | download ZIP resmi + verifikasi versi/hash | gagal = stop sebelum configure (P1) |

Referensi: Velopack docs C++ packaging & C API (dicatat di P1 saat implementasi).

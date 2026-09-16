param(
    [string] $VersionFile = (Join-Path $PSScriptRoot '..\version.txt')
)

$ErrorActionPreference = 'Stop'
$version = (Get-Content -LiteralPath $VersionFile -Raw).Trim()
if ($version -notmatch '^\d+\.\d+\.\d+$') {
    throw "version.txt harus semver tiga bagian, contoh: 0.1.0 (got: $version)"
}
$version

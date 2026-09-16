# Restore locked release dependencies before CMake configure.
$ErrorActionPreference = 'Stop'

$root = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$velopackVersion = '1.2.0'
$velopackUrl = 'https://github.com/velopack/velopack/releases/download/1.2.0/velopack_libc_1.2.0.zip'
$velopackZipSha256 = '547262ED7A1AB1FF62F580AA53851EDE2F1A451AC61B8974EB7BC01117488835'
$dependencyRoot = Join-Path $root 'release\artifacts\dependencies\velopack'
$want = @{
    'include/Velopack.h' = 'E6711FCC565386EF6C4E80079C44B6F3221A98E5C43B33A9331311A28B54C8FC'
    'include/Velopack.hpp' = '16CFDF96C48360B7D76BA06DD941D464E3427FA7ACA2F84626C6EE3AF76B14C9'
    'lib/velopack_libc_win_x64_msvc.dll' = 'C36D8B984639A8AF9D3397088D3FFB8213FE1BD0917F555CF0C6E33F014403EC'
    'lib/velopack_libc_win_x64_msvc.dll.lib' = '063DEF3F77CCDD44FD719536B9CB464015CC34E3C10197ADD6643AEC02408D11'
}

function Need-File([string] $Relative) {
    $p = Join-Path $root $Relative
    if (-not (Test-Path -LiteralPath $p -PathType Leaf)) { throw "missing: $Relative" }
    $p
}

Need-File 'global.json' | Out-Null
Need-File '.config/dotnet-tools.json' | Out-Null

$missing = @($want.Keys | Where-Object {
    -not (Test-Path -LiteralPath (Join-Path $dependencyRoot $_) -PathType Leaf)
})
if ($missing.Count -gt 0) {
    $dependencyParent = Split-Path -Parent $dependencyRoot
    New-Item -ItemType Directory -Path $dependencyParent -Force | Out-Null
    $archive = Join-Path $dependencyParent 'velopack_libc_1.2.0.zip'
    Invoke-WebRequest -Uri $velopackUrl -OutFile $archive
    if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $velopackZipSha256) {
        throw 'Velopack archive hash mismatch.'
    }
    Expand-Archive -LiteralPath $archive -DestinationPath $dependencyRoot -Force
    Remove-Item -LiteralPath $archive -Force
}

foreach ($rel in $want.Keys) {
    $p = Join-Path $dependencyRoot $rel
    if (-not (Test-Path -LiteralPath $p -PathType Leaf)) { throw "missing restored Velopack file: $rel" }
    $got = (Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash
    if ($got -ne $want[$rel]) { throw "hash mismatch: $rel" }
}

Push-Location -LiteralPath $root
try {
    & dotnet tool restore 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'dotnet tool restore failed' }
    $tl = ((& dotnet tool list --local) -join "`n")
    if ($tl -notmatch '(?m)^vpk\s+1\.2\.0\s') { throw "vpk 1.2.0 not installed locally: $tl" }
}
finally {
    Pop-Location
}

"DEPS-OK velopack=$velopackVersion vpk=1.2.0"

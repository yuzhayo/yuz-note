param()

$ErrorActionPreference = 'Stop'

$root = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$qtRoot = Join-Path $root 'development\Qt\6.8.3\msvc2022_64'
$qmake = Join-Path $qtRoot 'bin\qmake.exe'

if (Test-Path -LiteralPath $qmake -PathType Leaf) {
    Write-Host "Development prerequisites ready: $qtRoot"
    exit 0
}

throw @"
Qt 6.8.3 is required locally at:
  $qtRoot

Qt should be present after a complete repository clone. Restore the missing
development\Qt\6.8.3 SDK from the repository, then rerun this script.
C:\Qt is not used.
"@

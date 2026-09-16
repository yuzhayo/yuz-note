# P4 gate: single-instance A->B contract. Needs an interactive desktop.
# Owner starts (no UI assertions beyond alive) -> second forwards INSTALL.md
# and must exit 0 -> exactly one process remains -> its title proves handoff.
param(
    [string] $Exe = (Join-Path $PSScriptRoot '..\release\build\src\app\Release\yuz-note.exe'),
    [string] $File = (Join-Path $PSScriptRoot 'corpus\INSTALL.md')
)

$ErrorActionPreference = 'Stop'
$exe = [System.IO.Path]::GetFullPath($Exe)
$file = [System.IO.Path]::GetFullPath($File)
if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) { throw "exe not found: $exe" }

Get-Process 'yuz-note' -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1

$owner = Start-Process -FilePath $exe -PassThru
Start-Sleep -Seconds 3
$one = @(Get-Process 'yuz-note' -ErrorAction SilentlyContinue)
if ($one.Count -ne 1) { throw "want 1 owner, got $($one.Count)" }

$second = Start-Process -FilePath $exe -ArgumentList "`"$file`"" -PassThru
Start-Sleep -Seconds 3
if ($null -ne (Get-Process -Id $second.Id -ErrorAction SilentlyContinue)) {
    throw 'second instance still alive (must exit 0 after handoff)'
}
$second.Refresh()
if ($second.ExitCode -ne 0) { throw "second exit=$($second.ExitCode), want 0" }
$ownerNow = @(Get-Process 'yuz-note' -ErrorAction SilentlyContinue)
if ($ownerNow.Count -ne 1) { throw "want still 1 owner, got $($ownerNow.Count)" }
$title = $ownerNow[0].MainWindowTitle
if ([string]::IsNullOrEmpty($title) -or (-not $title.Contains('INSTALL.md'))) {
    throw "handoff failed, owner title=[$title]"
}
Stop-Process -Id $ownerNow[0].Id -Force
'SINGLE-INSTANCE-OK title=[' + $title + ']'

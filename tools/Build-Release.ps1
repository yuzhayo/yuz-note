param(
    [string] $Version = (& (Join-Path $PSScriptRoot 'Get-ProjectVersion.ps1')),
    [ValidateSet('win-x64')]
    [string] $Runtime = 'win-x64',
    [string] $BuildDir = (Join-Path $PSScriptRoot '..\release\build'),
    [string] $PublishDirectory = (Join-Path $PSScriptRoot '..\release\artifacts\publish\win-x64'),
    [string] $OutputDirectory = (Join-Path $PSScriptRoot '..\release\Releases'),
    [string] $ReleaseNotes
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

if ($Version -notmatch '^\d+\.\d+\.\d+$') {
    throw 'Version harus menggunakan semver tiga bagian, contoh 0.1.0.'
}

$repositoryRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$repositoryPrefix = $repositoryRoot.TrimEnd('\') + '\'
$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
$cmake = if ($cmakeCommand) {
    $cmakeCommand.Source
} else {
    'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
}
if (-not (Test-Path -LiteralPath $cmake -PathType Leaf)) {
    throw 'cmake not found in PATH or Visual Studio 18 CMake location.'
}

function Resolve-RepositoryOutput([string] $Path) {
    $resolved = [System.IO.Path]::GetFullPath($Path)
    if ($resolved -eq $repositoryRoot -or
        -not $resolved.StartsWith($repositoryPrefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Output harus berupa subfolder repository: $resolved"
    }
    $resolved
}

$publishPath = Resolve-RepositoryOutput $PublishDirectory
$outputPath = Resolve-RepositoryOutput $OutputDirectory

if (Test-Path -LiteralPath $publishPath) {
    Remove-Item -LiteralPath $publishPath -Recurse -Force
}
New-Item -ItemType Directory -Path $publishPath -Force | Out-Null
New-Item -ItemType Directory -Path $outputPath -Force | Out-Null

# Build Release
Write-Host "Building Release..."
& $cmake --build $BuildDir --config Release
if ($LASTEXITCODE -ne 0) { throw "cmake --build failed" }

# Stage with windeployqt
$exePath = Join-Path $BuildDir 'src\app\Release\yuz-note.exe'
if (-not (Test-Path -LiteralPath $exePath -PathType Leaf)) {
    throw "Executable not found: $exePath"
}

$windeployqt = if ([string]::IsNullOrWhiteSpace($env:QT_DIR)) {
    $null
} else {
    Join-Path $env:QT_DIR 'bin\windeployqt.exe'
}
if (-not $windeployqt -or -not (Test-Path -LiteralPath $windeployqt -PathType Leaf)) {
    $windeployqt = 'C:\Qt\6.8.3\msvc2022_64\bin\windeployqt.exe'
}
if (-not (Test-Path -LiteralPath $windeployqt -PathType Leaf)) {
    throw "windeployqt not found. Set QT_DIR or use default Qt path."
}

Write-Host "Staging with windeployqt..."
$mainExe = Join-Path $publishPath 'yuz-note.exe'
Copy-Item -LiteralPath $exePath -Destination $mainExe -Force
& $windeployqt $mainExe --dir $publishPath
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed" }

# Copy Velopack DLL
$velopackDll = Join-Path $repositoryRoot 'release\artifacts\dependencies\velopack\lib\velopack_libc_win_x64_msvc.dll'
$velopackTarget = Join-Path $publishPath 'velopack_libc.dll'
if (Test-Path -LiteralPath $velopackDll) {
    Copy-Item -LiteralPath $velopackDll -Destination $velopackTarget -Force
}

# Verify staging
if (-not (Test-Path -LiteralPath $mainExe -PathType Leaf)) {
    throw "Staged executable not found: $mainExe"
}

# Remove PDB files
Get-ChildItem -LiteralPath $publishPath -Filter '*.pdb' -Recurse | Remove-Item -Force

Write-Host "Staging verified: $publishPath"

# Pack with Velopack
Write-Host "Packing with Velopack..."
$iconPath = Join-Path $repositoryRoot 'assets\icon.ico'
$packArguments = @(
    'vpk', 'pack',
    '--packId', 'Yuzhayo.YuzNote',
    '--packVersion', $Version,
    '--packDir', $publishPath,
    '--mainExe', 'yuz-note.exe',
    '--packTitle', 'yuz-note',
    '--packAuthors', 'yuzhayo',
    '--outputDir', $outputPath,
    '--runtime', $Runtime,
    '--shortcuts', 'Desktop,StartMenuRoot'
)
if (Test-Path -LiteralPath $iconPath) {
    $packArguments += @('--icon', $iconPath)
}
if (-not [string]::IsNullOrWhiteSpace($ReleaseNotes)) {
    $notesPath = [System.IO.Path]::GetFullPath($ReleaseNotes)
    if (-not (Test-Path -LiteralPath $notesPath -PathType Leaf)) {
        throw "Release notes not found: $notesPath"
    }
    $packArguments += @('--releaseNotes', $notesPath)
}

& dotnet @packArguments
if ($LASTEXITCODE -ne 0) { throw "vpk pack failed" }

$setup = Get-ChildItem -LiteralPath $outputPath -Filter '*-Setup.exe' |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1
if (-not $setup) {
    throw 'Velopack did not produce a Setup.exe installer.'
}

$sha256 = (Get-FileHash -LiteralPath $setup.FullName -Algorithm SHA256).Hash
Write-Host "Installer: $($setup.Name)"
Write-Host "SHA-256: $sha256"

[pscustomobject]@{
    Version = $Version
    Setup = $setup.FullName
    Sha256 = $sha256
} | ConvertTo-Json

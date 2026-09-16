# P2 gate runner: mdparse over tests/corpus (+ generated reject fixtures).
# Fails (exit 1) on the first violated expectation. Mirrors EXPECTATIONS.md.
param(
    [string] $MdParse = (Join-Path $PSScriptRoot '..\release\build\src\mdparse\Release\mdparse.exe')
)

$ErrorActionPreference = 'Stop'
$script:pass = 0
$script:fail = 0

$exe = [System.IO.Path]::GetFullPath($MdParse)
if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) { throw "mdparse not found: $exe" }
$corpus = Join-Path $PSScriptRoot 'corpus'
$gen = Join-Path ([System.IO.Path]::GetTempPath()) 'yuz-corpus-gen'
New-Item -ItemType Directory -Path $gen -Force | Out-Null
$outHtml = Join-Path $gen 'out.html'
$outErr = Join-Path $gen 'out.err'
$triClosed = [char]0x25B8
$triOpen = [char]0x25BE

# Generated (binary) fixtures — committed corpus stays text-only.
[System.IO.File]::WriteAllBytes((Join-Path $gen 'reject-binary.md'),
    [byte[]] (0x23, 0x20, 0x68, 0x69, 0x0A, 0x00, 0x41))
[System.IO.File]::WriteAllBytes((Join-Path $gen 'reject-utf16.md'),
    ([byte[]] (0xFF, 0xFE)) + [System.Text.Encoding]::Unicode.GetBytes("# hi`n"))
[System.IO.File]::WriteAllBytes((Join-Path $gen 'reject-badutf8.md'),
    [byte[]] (0x23, 0x20, 0xC0, 0xAF, 0x0A))
[System.IO.File]::WriteAllBytes((Join-Path $gen 'big-5mb.md'),
    [System.Text.Encoding]::ASCII.GetBytes(('x' * (5 * 1024 * 1024))))

function Invoke-Case {
    param(
        [string] $File,
        [int] $Exit,
        [string[]] $Has = @(),
        [string[]] $HasNot = @(),
        [string[]] $Warn = @(),
        [string[]] $ErrHas = @()
    )
    $p = Start-Process -FilePath $exe -ArgumentList "`"$File`"" `
        -NoNewWindow -Wait -PassThru `
        -RedirectStandardOutput $outHtml -RedirectStandardError $outErr
    $out = [System.IO.File]::ReadAllText($outHtml)
    $err = [System.IO.File]::ReadAllText($outErr)
    $ok = $true
    if ($p.ExitCode -ne $Exit) {
        "FAIL $(Split-Path $File -Leaf): exit=$($p.ExitCode) want=$Exit err=[$err]"
        $ok = $false
    }
    foreach ($h in $Has) {
        if (-not $out.Contains($h)) { "FAIL $(Split-Path $File -Leaf): stdout missing [$h]"; $ok = $false }
    }
    foreach ($h in $HasNot) {
        if ($out.Contains($h)) { "FAIL $(Split-Path $File -Leaf): stdout contains forbidden [$h]"; $ok = $false }
    }
    foreach ($h in $Warn) {
        if (-not $err.Contains($h)) { "FAIL $(Split-Path $File -Leaf): stderr missing warning [$h]"; $ok = $false }
    }
    foreach ($h in $ErrHas) {
        if (-not $err.Contains($h)) { "FAIL $(Split-Path $File -Leaf): stderr missing [$h]"; $ok = $false }
    }
    if ($Warn.Count -eq 0 -and $err -match 'warning:') {
        "FAIL $(Split-Path $File -Leaf): unexpected warning [$err]"
        $ok = $false
    }
    if ($ok) { 'PASS ' + (Split-Path $File -Leaf); $script:pass++ } else { $script:fail++ }
}

Invoke-Case (Join-Path $corpus 'headings.md') 0 `
    -Has '<h1>Title One</h1>', '<em>', '<strong>', '<code>inline code</code>'
Invoke-Case (Join-Path $corpus 'lists.md') 0 `
    -Has '<ul>', '<ol>', 'checkbox', 'nested paren'
Invoke-Case (Join-Path $corpus 'table.md') 0 -Has '<table>', 'cell one'
Invoke-Case (Join-Path $corpus 'code.md') 0 `
    -Has 'language-bash', 'mkdir -p .opencode/agent', '<pre><code></code></pre>'
Invoke-Case (Join-Path $corpus 'links.md') 0 `
    -Has 'href="https://example.com"', 'mailto:', 'href="#title-one"'
Invoke-Case (Join-Path $corpus 'footnote.md') 0 -Has 'footnotes', 'First note.'
Invoke-Case (Join-Path $corpus 'details.md') 0 `
    -Has 'yuz-details:details-1', 'yuz-details:details-2', 'Hidden <strong>bold</strong>', 'details-1-end', $triClosed, $triOpen
Invoke-Case (Join-Path $corpus 'details-edge.md') 0 `
    -Has 'yuz-details:details-1', '&lt;details' `
    -Warn 'nested-details-flattened', 'details-without-summary', 'unclosed-details'
Invoke-Case (Join-Path $corpus 'details-many.md') 0 -Has 'yuz-details:details-50'
Invoke-Case (Join-Path $corpus 'images.md') 0 `
    -Has '[image: Alt text]', '[image: file:///C:/tmp/y.jpg]', '[image: linked]' `
    -HasNot '<img'
Invoke-Case (Join-Path $corpus 'math.md') 0 -Has '$E=mc^2$', '$5'
Invoke-Case (Join-Path $corpus 'deep-nesting.md') 2 -ErrHas 'nesting-too-deep'
Invoke-Case (Join-Path $corpus 'INSTALL.md') 0 `
    -Has '<h1>', '<table>', 'yuz-details:', 'language-bash', 'footnotes' `
    -HasNot '<img'
Invoke-Case (Join-Path $gen 'reject-binary.md') 1 -ErrHas 'source-binary'
Invoke-Case (Join-Path $gen 'reject-utf16.md') 1 -ErrHas 'source-unsupported-bom'
Invoke-Case (Join-Path $gen 'reject-badutf8.md') 1 -ErrHas 'source-invalid-utf8'
Invoke-Case (Join-Path $gen 'big-5mb.md') 1 -ErrHas 'source-too-large'

"PASS=$($script:pass) FAIL=$($script:fail)"
if ($script:fail -gt 0) { exit 1 }

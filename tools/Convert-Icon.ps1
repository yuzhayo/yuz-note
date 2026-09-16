# Convert a transparent PNG to a multi-resolution Windows ICO.
# Usage: .\tools\Convert-Icon.ps1 -InputPath assets\icon.png -OutputPath assets\icon.ico

param(
    [Parameter(Mandatory = $true)]
    [string] $InputPath,
    [string] $OutputPath = ($InputPath -replace '\.png$', '.ico')
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$input = [System.IO.Path]::GetFullPath($InputPath)
$output = [System.IO.Path]::GetFullPath($OutputPath)
if (-not (Test-Path -LiteralPath $input -PathType Leaf)) {
    throw "Input file not found: $input"
}

$outputParent = Split-Path -Parent $output
if (-not (Test-Path -LiteralPath $outputParent -PathType Container)) {
    throw "Output directory not found: $outputParent"
}

$source = [System.Drawing.Image]::FromFile($input)
try {
    $frames = @()
    foreach ($size in @(16, 24, 32, 48, 64, 128, 256)) {
        $bitmap = New-Object System.Drawing.Bitmap($size, $size,
            [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        try {
            $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
            try {
                $graphics.Clear([System.Drawing.Color]::Transparent)
                $graphics.InterpolationMode =
                    [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
                $graphics.PixelOffsetMode =
                    [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
                $graphics.DrawImage($source, 0, 0, $size, $size)

                $stream = New-Object System.IO.MemoryStream
                try {
                    $bitmap.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
                    $frames += ,@($size, $stream.ToArray())
                }
                finally {
                    $stream.Dispose()
                }
            }
            finally {
                $graphics.Dispose()
            }
        }
        finally {
            $bitmap.Dispose()
        }
    }

    $file = [System.IO.File]::Open($output, [System.IO.FileMode]::Create,
        [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
    try {
        $writer = New-Object System.IO.BinaryWriter($file)
        try {
            $writer.Write([UInt16]0) # reserved
            $writer.Write([UInt16]1) # ICO
            $writer.Write([UInt16]$frames.Count)

            [UInt32]$offset = 6 + (16 * $frames.Count)
            foreach ($frame in $frames) {
                [int]$size = $frame[0]
                [byte[]]$bytes = $frame[1]
                $writer.Write([byte]$(if ($size -eq 256) { 0 } else { $size }))
                $writer.Write([byte]$(if ($size -eq 256) { 0 } else { $size }))
                $writer.Write([byte]0)
                $writer.Write([byte]0)
                $writer.Write([UInt16]1)
                $writer.Write([UInt16]32)
                $writer.Write([UInt32]$bytes.Length)
                $writer.Write($offset)
                $offset += $bytes.Length
            }
            foreach ($frame in $frames) {
                $writer.Write([byte[]]$frame[1])
            }
        }
        finally {
            $writer.Dispose()
        }
    }
    finally {
        $file.Dispose()
    }
}
finally {
    $source.Dispose()
}

Write-Host "Icon created: $output (16, 24, 32, 48, 64, 128, 256 px)"

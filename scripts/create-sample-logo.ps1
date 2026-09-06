param([string]$Output = (Join-Path $PSScriptRoot "..\config\sample-logo.png"))

Add-Type -AssemblyName System.Drawing
$directory = Split-Path -Parent $Output
New-Item -ItemType Directory -Force -Path $directory | Out-Null
$bitmap = [System.Drawing.Bitmap]::new(128, 72, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
try {
    $graphics.Clear([System.Drawing.Color]::Transparent)
    $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.FillRectangle([System.Drawing.Brushes]::White, 4, 4, 120, 64)
    $graphics.FillRectangle([System.Drawing.Brushes]::Navy, 8, 8, 112, 56)
    $font = [System.Drawing.Font]::new("Yu Gothic UI", 24, [System.Drawing.FontStyle]::Bold)
    try { $graphics.DrawString("VB", $font, [System.Drawing.Brushes]::White, 36, 16) } finally { $font.Dispose() }
    $bitmap.Save($Output, [System.Drawing.Imaging.ImageFormat]::Png)
} finally {
    $graphics.Dispose()
    $bitmap.Dispose()
}
Write-Host "Created $Output"

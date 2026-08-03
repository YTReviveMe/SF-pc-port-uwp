param(
    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory,

    [Parameter(Mandatory = $true)]
    [string]$SourcePath
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

if (-not (Test-Path -LiteralPath $SourcePath -PathType Leaf)) {
    throw "Launcher icon source is missing: $SourcePath"
}

[System.IO.Directory]::CreateDirectory($OutputDirectory) | Out-Null
$source = [System.Drawing.Image]::FromFile($SourcePath)

function Write-SquareAsset([string]$name, [int]$size) {
    $bitmap = [System.Drawing.Bitmap]::new(
        $size, $size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
        try {
            $graphics.Clear([System.Drawing.Color]::Transparent)
            $graphics.InterpolationMode =
                [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
            $graphics.PixelOffsetMode =
                [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
            $graphics.DrawImage($source, 0, 0, $size, $size)
        } finally {
            $graphics.Dispose()
        }
        $bitmap.Save((Join-Path $OutputDirectory $name),
            [System.Drawing.Imaging.ImageFormat]::Png)
    } finally {
        $bitmap.Dispose()
    }
}

try {
    Write-SquareAsset 'StoreLogo.png' 50
    Write-SquareAsset 'Square44x44Logo.png' 44
    Write-SquareAsset 'Square150x150Logo.png' 150

    $splash = [System.Drawing.Bitmap]::new(
        620, 300, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
        $graphics = [System.Drawing.Graphics]::FromImage($splash)
        try {
            $graphics.Clear([System.Drawing.Color]::FromArgb(255, 3, 7, 17))
            $graphics.InterpolationMode =
                [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
            $graphics.PixelOffsetMode =
                [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
            $graphics.DrawImage($source, 160, 20, 260, 260)
        } finally {
            $graphics.Dispose()
        }
        $splash.Save((Join-Path $OutputDirectory 'SplashScreen.png'),
            [System.Drawing.Imaging.ImageFormat]::Png)
    } finally {
        $splash.Dispose()
    }
} finally {
    $source.Dispose()
}

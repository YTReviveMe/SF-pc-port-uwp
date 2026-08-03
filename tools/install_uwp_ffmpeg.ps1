#requires -Version 7.0
[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidateScript({ Test-Path -LiteralPath $_ -PathType Container })]
    [string]$VcpkgRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$resolvedVcpkgRoot = (Resolve-Path -LiteralPath $VcpkgRoot).Path
if ($resolvedVcpkgRoot -match '\s') {
    throw "FFmpeg's vcpkg port cannot build from a path with spaces. Use a root such as D:\\deps\\vcpkg."
}

$vcpkg = Join-Path $resolvedVcpkgRoot 'vcpkg.exe'
if (-not (Test-Path -LiteralPath $vcpkg -PathType Leaf)) {
    throw "vcpkg.exe was not found under $VcpkgRoot. Bootstrap vcpkg first."
}

& $vcpkg install --classic 'ffmpeg[avcodec,avformat,swresample,swscale]:x64-uwp'
if ($LASTEXITCODE -ne 0) {
    throw "vcpkg failed with exit code $LASTEXITCODE."
}

$installed = Join-Path $resolvedVcpkgRoot 'installed/x64-uwp'
Write-Host 'FFmpeg UWP dependencies are ready. Configure the port with:'
Write-Host "  -DSF_UWP_FFMPEG_INCLUDE_ROOT='$installed/include'"
Write-Host "  -DSF_UWP_FFMPEG_LIBRARY_ROOT='$installed/lib'"
Write-Host "  -DSF_UWP_FFMPEG_RUNTIME_ROOT='$installed/bin'"
